# CH552 QMK/VIA Keyboard Firmware

A small keyboard firmware for the CH552 (enhanced 8051 with a USB
full-speed device controller). It does not use the full QMK code base;
instead it implements the **QMK/VIA raw HID host protocol**, so VIA can
remap keys and control RGB without recompiling the firmware.

[中文](README.md) | English

## Features

* CH552 USB full-speed composite HID device
  * Keyboard + consumer/media keys (Consumer Control)
  * VIA raw HID interface (usage page `0xFF60` / usage `0x61`)
* VIA protocol `0x000D`
  * Read / write / reset dynamic keycodes
  * Keymap buffer read/write (`id_dynamic_keymap_get/set_buffer`)
  * RGB brightness, effect, speed and color
  * Jump to the WCH ISP bootloader
* Dynamic keymap stored in the CH552's 128-byte on-chip Data Flash
* Key matrix or direct-wired keys (one pin per key), selected in
  `keyboard_config.h`
* Layers (`MO` / `TG`) and transparent keys (`KC_TRNS`)
* WS2812/WS2812B RGB, using hardware SPI encoding or a GPIO bit-bang driver
* Common QMK keycodes, media keys, `QK_BOOTLOADER`, `QK_CLEAR_EEPROM`
* SDCC build, 16 MHz by default; 12/24 MHz supported

> This is a lightweight "QMK protocol compatible" implementation, not full
> QMK. Macros, Tap Dance, Combos, encoders, NKRO, etc. are not implemented.
> See the limitations at the end.

---

## Repository layout

```
.
├── Makefile
├── firmware/
│   ├── include/
│   │   ├── keyboard_config.h   <- main configuration file
│   │   ├── board_4key.h        4-key direct-wired board profile
│   │   ├── keycodes.h          QMK keycode values
│   │   ├── ch552.h             CH552 register definitions
│   │   ├── ch554_usb.h         USB constants / SETUP structure
│   │   ├── usb.h
│   │   ├── keyboard.h
│   │   ├── matrix.h
│   │   ├── via.h
│   │   ├── rgb.h
│   │   ├── ws2812.h
│   │   ├── eeprom.h
│   │   └── debug.h
│   └── src/
│       ├── main.c              main loop
│       ├── usb.c               USB descriptors, EP0, HID endpoints, VIA transport
│       ├── keyboard.c          scan/debounce, layers, dynamic keymap, HID reports
│       ├── matrix.c            matrix / direct-pin scanner
│       ├── via.c               VIA raw HID command dispatcher
│       ├── rgb.c               RGB effects and VIA settings
│       ├── ws2812.c            SPI / bit-bang WS2812 driver
│       ├── eeprom.c            Data Flash access
│       └── debug.c             clock, delays, 1 ms system tick
├── via/
│   ├── ch552_qmk_pad.json      VIA 3 definition (default 3x4 layout)
│   ├── ch552_qmk_pad_via2.json VIA 2 compatible definition (3x4)
│   ├── ch552_qmk_4key.json     VIA 3 definition (4-key board)
│   └── ch552_qmk_4key_via2.json VIA 2 compatible definition (4-key)
├── tools/
│   └── via_probe.py            command-line VIA protocol probe
└── docs/
    ├── hardware.md
    └── protocol.md
```

The detailed documents under `docs/` are currently written in Chinese.
The register/descriptor/protocol details are still useful as a reference.

---

## Default hardware wiring

The default profile is **3 rows x 4 columns = 12 keys**, with one WS2812 per
key:

| CH552 | Direction | Connected to |
|-------|-----------|--------------|
| P1.0  | output | matrix row 0 |
| P1.1  | output | matrix row 1 |
| P1.2  | output | matrix row 2 |
| P3.0  | input, pull-up | matrix column 0 |
| P3.1  | input, pull-up | matrix column 1 |
| P3.2  | input, pull-up | matrix column 2 |
| P3.3  | input, pull-up | matrix column 3 |
| P1.5  | output | WS2812 DIN (SPI0 MOSI) |
| P1.7  | output | reserved for SPI0 SCK, not connected to the LED |
| P3.6  | USB | D- |
| P3.7  | USB | D+ |
| 5V    | power | USB VBUS / external 5 V |
| GND   | ground | GND |

A diode per key is recommended: **anode to column, cathode to row** (the
firmware drives one row low at a time and reads the columns). Without
diodes, pressing three or more keys at once can cause ghosting.

> RGB uses P1.5/P1.7. Do not assign those pins to the matrix when RGB is
> enabled. P1.6 is unused and can be used by the matrix; if RGB is disabled,
> P1.5/P1.7 become free as well.

Power WS2812 LEDs from an external 5 V rail, not from the CH552's 3.3 V
LDO. Twelve white LEDs draw roughly 700 mA; the default brightness is
64/255, but a 100-470 uF capacitor is still recommended.

---

## 4-key direct-wired board

The board profile lives in `firmware/include/board_4key.h`. The default 3x4
configuration is left untouched.

| Silkscreen | CH552 pin | Notes |
|------------|-----------|-------|
| But1 | P1.7 | one side to the pin, the other to GND |
| But2 | P1.6 | same |
| But3 | P1.4 | same |
| But4 | P3.2 | same |
| RGB  | P1.5 | WS2812 data chain DIN (4 pixels by default) |
| 5V/GND | power | external 5 V recommended, common ground |

This profile drives WS2812 through a **GPIO software bit-bang**, using only
P1.5. P1.7 is therefore free for But1 and cannot conflict with SPI0 SCK.
The 4-key profile also uses a **standard 8-byte boot keyboard** (no report
ID, no Consumer collection) for Windows compatibility, so media keys are not
sent on this board. Default keymap:

```text
But1 -> 1
But2 -> 2
But3 -> 3
But4 -> MO(1)    hold But4: But1..But3 become F1..F3
```

Build and flash:

```bash
make BOARD=4key             # 16 MHz, output: build/ch552_qmk_4key.hex
make BOARD=4key VCC=3V3     # 12 MHz
make BOARD=4key flash       # ch55xtool
```

The VIA definition for the 4-key firmware is
`via/ch552_qmk_4key.json`, VID:PID `1209:5524`. The default 3x4 firmware
uses `1209:5523`; both definitions can be loaded separately.

> VIA explicitly rejects `0xFEED` as a vendor ID, so the firmware and the
> JSON definitions use `0x1209`. You must flash the firmware with the new
> VID; otherwise the device still enumerates as `FEED:5524` and will not
> match the JSON.

The 4-key board has been verified under Windows 10/11: standard keyboard
input, VIA remapping, and persistence across replug all work. It uses
`CONSUMER_ENABLE 0` (an 8-byte boot keyboard without report IDs). The
default 3x4 board keeps `CONSUMER_ENABLE 1`.

Notes:

* The software bit-bang timing is calibrated for 12/16 MHz;
  `make BOARD=4key FREQ_SYS=24000000` is rejected at compile time. If you
  really need 24 MHz, use the SPI driver, but P1.7 will be occupied by SCK
  and cannot be used by But1.
* Buttons are designed as "other side to GND, internal pull-up". If your
  buttons are active-high (to VCC), invert the read polarity in `matrix.c`.
* The 4-key profile defaults to four WS2812 pixels (one per key). If your
  strip has a different count, change `WS2812_LED_COUNT` in
  `board_4key.h`.

---

## Building

SDCC 4.x is required:

```bash
sudo apt install sdcc
make
```

Outputs:

```
build/ch552_qmk.hex   Intel HEX, for WCHISPTool
build/ch552_qmk.bin   raw binary, for ch55xtool and similar tools
```

Common options:

```bash
make VCC=3V3            # 3.3 V supply, 12 MHz by default
make FREQ_SYS=24000000  # 24 MHz (5 V)
make clean
make size
```

The default is 16 MHz. The CH552 datasheet requires VCC above 3.3 V for
16 MHz; for a 3.3 V board use `make VCC=3V3` (12 MHz).

### Flashing

Windows: use WCHISPTool and select `build/ch552_qmk.hex`.

Linux/macOS:

```bash
pip install ch55xtool
make flash
```

The first flash usually requires holding the BOOT/download button while
powering up so the chip enters the WCH ISP bootloader
(VID:PID `4348:55E0`). A key can also be assigned `QK_BOOTLOADER`, or VIA
can send `id_bootloader_jump` to return to the bootloader.

---

## Configuration

All user configuration is in:

```
firmware/include/keyboard_config.h
```

### 1. Matrix size and pins

For example, a 4x4 matrix:

```c
#define MATRIX_ROWS 4
#define MATRIX_COLS 4

#define ROW_PINS { {1, 0}, {1, 1}, {1, 2}, {1, 3} }
#define COL_PINS { {3, 0}, {3, 1}, {3, 2}, {3, 3} }
```

In `{port, bit}`, port can only be 1 or 3; P3.6/P3.7 are USB pins and cannot
be used. After changing the matrix, update `matrix.rows/cols` and
`layouts.keymap` in the matching `via/*.json`, otherwise VIA will show the
wrong key positions.

### 2. Direct-wired mode (one pin per key)

When there is no matrix and each key connects one pin to GND:

```c
#define KEYBOARD_MODE       KEYBOARD_MODE_DIRECT
#define DIRECT_KEY_COUNT    6
#define DIRECT_PINS { {1,0}, {1,1}, {1,2}, {1,3}, {3,0}, {3,1} }
```

In direct mode `MATRIX_ROWS` becomes 1 and `MATRIX_COLS = DIRECT_KEY_COUNT`.
Change `DEFAULT_KEYMAP` to one row of six keys, and use
`"rows": 1, "cols": 6` in the VIA definition. See `board_4key.h` for a
complete example.

### 3. Default keymap and layers

```c
#define DYNAMIC_KEYMAP_LAYERS 2

#define DEFAULT_KEYMAP {                                             \
    { /* layer 0 */                                                  \
        { KC_1,    KC_2,    KC_3,    KC_4    },                      \
        { KC_Q,    KC_W,    KC_E,    KC_R    },                      \
        { KC_A,    KC_S,    KC_D,    MO(1)   }                       \
    },                                                               \
    { /* layer 1 */                                                  \
        { KC_F1,   KC_F2,   KC_F3,   KC_F4   },                      \
        { KC_MUTE, KC_VOLD, KC_VOLU, KC_MPLY },                      \
        { KC_BRIU, KC_BRID, KC_TRNS, MO(1)   }                       \
    }                                                                \
}
```

* `KC_TRNS` (`_______`) is transparent: lookup continues on the next lower
  active layer.
* `MO(1)` activates layer 1 while held; `TG(1)` toggles it.
* Layer 0 is always active.
* Default behavior: holding the bottom-right `MO(1)` turns the top row into
  F1-F4, the middle row into mute/volume/play, and the bottom row into
  brightness. Release to return.

### 4. RGB

```c
#define RGB_ENABLE             1
#define WS2812_LED_COUNT       12
#define WS2812_GRB             1
#define RGB_DEFAULT_BRIGHTNESS 64
#define RGB_DEFAULT_HUE        0
#define RGB_DEFAULT_SAT        255
#define RGB_DEFAULT_EFFECT     1
#define RGB_DEFAULT_SPEED      128
```

* To disable RGB: `RGB_ENABLE 0`, `WS2812_LED_COUNT 0`; this saves about
  3 KB of flash.
* The default 3x4 profile uses SPI0 (P1.5/P1.7); the 4-key profile uses
  `WS2812_DRIVER_BITBANG 1` and only P1.5.
* `WS2812_LED_COUNT` is limited to 32 by the 1 KB xRAM.
* Most LEDs use GRB order; set `WS2812_GRB 1`, or 0 for RGB order.
* Effect numbers roughly follow QMK `RGBLIGHT_MODE_*`: 1 solid, 2-4
  breathing, 5-6 rainbow mood, 7-13 rainbow swirl, 14-19 snake, 20-21
  knight, 22 Christmas, 23-33 static gradient, 34 RGB test, 35 alternating,
  36-41 twinkle.

### 5. Keymap storage capacity

The CH552 has only 128 bytes of Data Flash: 8 bytes of header, 1 checksum
byte, 8 bytes of RGB settings, leaving about 103 bytes for the keymap. The
constraint is:

```
8 + KEY_COUNT * DYNAMIC_KEYMAP_LAYERS * 2 + 1 <= 120
```

For example, 12 keys allow up to 4 layers, 16 keys up to 3 layers. A
violation is reported at compile time.

---

## Using VIA

1. Open <https://usevia.app> in Chrome/Edge.
2. Connect the keyboard, click **Authorize device**, and select
   `CH552 QMK Pad` (VID:PID `1209:5523`).
3. Open **Settings** and enable **Show Design tab**.
4. Open the **Design** tab, click **Load Draft Definition**, and choose
   `via/ch552_qmk_pad.json` (VIA 3) or
   `via/ch552_qmk_pad_via2.json` (old VIA 2).

> VIA 3 uses the `menus` field; the old `lighting` field was removed. VIA 2
> is the opposite. If importing reports
> `must NOT have additional properties`, the JSON version does not match
> the VIA version; use the other JSON file.
5. Return to **Configure** to remap keys; the **Lighting** tab controls RGB.

For the 4-key board the device is `CH552 QMK 4Key` (`1209:5524`); in step 4
choose `via/ch552_qmk_4key.json` (VIA 3) or
`via/ch552_qmk_4key_via2.json` (old VIA 2).

VIA changes are written to Data Flash immediately through
`id_dynamic_keymap_set_keycode`. To reset the keyboard:

* use Reset Keymap in VIA;
* or assign a key to `QK_CLEAR_EEPROM` and press it;
* or send an EEPROM reset from VIA's Design/Protocol tools.

After changing the matrix size or layer count in `keyboard_config.h` (or
`board_4key.h`), update the matching VIA JSON as well.

If VIA does not show the keyboard, verify the protocol from the command
line:

```bash
pip install pyusb
./tools/via_probe.py info
./tools/via_probe.py get 0 0 0
./tools/via_probe.py rgb
```

On Linux, if `usbhid` owns the raw HID interface, the script attempts to
detach it. Running with `sudo` is also an option.

---

## Supported keycodes

* Basic HID keycodes `0x04`-`0xE7`: `KC_A`, `KC_1`, `KC_ENT`, etc.
* Modifiers `0xE0`-`0xE7`: `KC_LCTL`, `KC_LSFT`, `KC_LGUI`, etc.
* QMK media keycodes `0xA5`-`0xBE` (`KC_MUTE`, `KC_VOLU`, `KC_MPLY`,
  `KC_BRIU`, etc.). These are sent through the Consumer Control report when
  `CONSUMER_ENABLE 1` (default 3x4 board; disabled on the 4-key board for
  Windows compatibility).
* Layer keys:
  * `MO(layer)`: current QMK `0x5220 + layer`, also compatible with the old
    `0x5100 + layer`.
  * `TG(layer)`: current `0x5260 + layer`, also compatible with the old
    `0x5200 + layer`.
* `QK_BOOTLOADER`: jump to the WCH bootloader.
* `QK_CLEAR_EEPROM`: restore the default keymap and RGB settings.
* `KC_NO` / `KC_TRNS`.

For compatibility with different VIA versions, `0x5200`-`0x521F` is treated
as `TG`. Current QMK `TO(layer)` also falls into that range, so it behaves
closer to "toggle" than "exclusive switch".

---

## VIA protocol implementation

Raw HID reports are 32 bytes: output endpoint `0x03`, input endpoint `0x82`.
Protocol version `0x000D`.

| Command | Function |
|---------|----------|
| `0x01` | protocol version |
| `0x02` | uptime / layout / matrix state / firmware version / keycodes version |
| `0x03` | layout options / device indication (accepted and ignored) |
| `0x04` / `0x05` | read / write one dynamic keycode |
| `0x06` | reset keymap |
| `0x07` / `0x08` / `0x09` | RGB set / get / save |
| `0x0A` | reset keymap + RGB |
| `0x0B` | jump to bootloader |
| `0x0C`-`0x10` | macro commands, return empty (macros unsupported) |
| `0x11` | layer count |
| `0x12` / `0x13` | keymap buffer read / write |

The RGB channel accepts both `2` (`qmk_rgblight`) and `3`
(`qmk_rgb_matrix`). Value IDs: `1` brightness, `2` effect, `3` speed,
`4` hue + saturation.

See [`docs/protocol.md`](docs/protocol.md) for timing, descriptors and the
storage layout (document is in Chinese).

---

## Implementation notes

### USB

* EP0: control transfers, standard requests and HID class requests
* EP1 IN `0x81`: keyboard / media reports
* EP2 IN `0x82`: VIA raw HID IN
* EP3 OUT `0x03`: VIA raw HID OUT
* Report descriptors:
  * `CONSUMER_ENABLE 1`: report ID 1 keyboard + report ID 2 Consumer
  * `CONSUMER_ENABLE 0`: plain 8-byte keyboard without report IDs (4-key
    board)
  * separate HID interface: usage page `0xFF60`, 32-byte raw HID
* Boot protocol supported: after `SET_PROTOCOL(0)` the keyboard report has
  no report ID and media reports are stopped.

### Matrix scanning

* Scanned every 1 ms; a state must be stable for `DEBOUNCE_MS` scans.
* Rows are push-pull outputs driven low one at a time; columns are
  quasi-bidirectional inputs with pull-ups.
* Direct mode reads each pin as an independent pulled-up input.

### RGB

* SPI0 runs at about 2.4-3 MHz.
* Each WS2812 bit is encoded as three SPI bits: `0 -> 100`, `1 -> 110`.
* Interrupts are disabled briefly while a frame is shifted; 12 LEDs take
  about 0.3 ms.
* Effect refresh period is `RGB_UPDATE_MS`, 20 ms by default.

The 4-key profile uses a GPIO bit-bang driver on P1.5 instead of SPI. The
loop is calibrated for 12/16 MHz; 24 MHz is rejected.

### Data Flash

| Offset | Contents |
|--------|----------|
| `0x00`-`0x03` | `CQMK` |
| `0x04` | layout version |
| `0x05` | rows |
| `0x06` | columns |
| `0x07` | layers |
| `0x08`... | keymap, layer-major/row-major/column-major, little-endian 16-bit |
| 1 byte after keymap | checksum |
| `0x78`-`0x7F` | `RGB` + brightness/hue/saturation/effect/speed |

---

## Limitations and notes

* **Data Flash is only 128 bytes.** Reduce the layer count for large
  keymaps, or use external EEPROM.
* No macros, Tap Dance, Combos, encoders, NKRO, mouse keys, audio, etc.
* No anti-ghosting in the matrix; add diodes if multiple simultaneous keys
  are needed.
* `CONSUMER_ENABLE`: 1 = keyboard report IDs + Consumer media; 0 = standard
  8-byte keyboard (best Windows compatibility, used by the 4-key board).
* The default 3x4 RGB uses P1.5/P1.7; the 4-key board uses only P1.5 for
  bit-bang, leaving P1.7 for But1. Matrix pins and VIA JSON must match the
  actual configuration.
* WS2812 frames briefly disable interrupts. Twelve LEDs take about 0.3 ms;
  the 4-key board's four LEDs take about 0.1 ms. More LEDs increase USB
  jitter; 16-24 LEDs is a sensible upper range.
* The 4-key board's WS2812 driver is software bit-banged and calibrated for
  12/16 MHz; 24 MHz is rejected at compile time. The default 3x4 SPI
  driver supports 12/16/24 MHz.
* Firmware size is about 12 KB with a 14 KB user flash limit (`0x3800`);
  disabling RGB saves about 3 KB.
* USB VID/PID: default 3x4 board `0x1209:0x5523`, 4-key board
  `0x1209:0x5524`. `0x1209` is the pid.codes open-source hardware VID;
  for a formal release, allocate a PID according to pid.codes rules and
  update the VIA JSON.

---

## References

* [ieiao/ch554_sdcc](https://github.com/ieiao/ch554_sdcc) - CH552/CH554 SDCC
  register definitions and USB device examples
* [SoCXin/CH552](https://github.com/SoCXin/CH552) - CH552 documentation and
  SDK collection
* [QMK VIA protocol implementation](https://github.com/qmk/qmk_firmware/blob/master/quantum/via.c)
* CH552/CH551 datasheet (WCH)
