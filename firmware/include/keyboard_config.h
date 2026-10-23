/*==========================================================================*
 * keyboard_config.h - the only file most users need to edit
 *
 * Change the matrix size, the pin list, the default keymap and the RGB
 * settings here.  Everything else is generic firmware.
 *
 * Supported pins
 * --------------
 *   Port 1 : P1.0 .. P1.7
 *   Port 3 : P3.0 .. P3.5   (P3.6/P3.7 are the USB D-/D+ pins)
 *
 * The WS2812 RGB output uses the SPI0 hardware pins, which are fixed:
 *   P1.5 = MOSI (LED data), P1.7 = SCK (unused by the LED, but reserved)
 * Do not use P1.5/P1.7 for the key matrix when RGB is enabled.  P1.6 is
 * free because the LED driver does not use MISO.
 *==========================================================================*/

#ifndef __KEYBOARD_CONFIG_H__
#define __KEYBOARD_CONFIG_H__

#include "keycodes.h"

/* The 4-key board profile keeps the default 3x4 configuration below
 * untouched.  Select it with -DBOARD_4KEY (make BOARD=4key). */
#ifdef BOARD_4KEY
#include "board_4key.h"
#else

/*---------------------------------------------------------------*
 * USB identity                                                  *
 *                                                               *
 * VIA matches its keyboard definition against these two values. *
 * The JSON in via/ch552_qmk_pad.json uses the same numbers.     *
 *---------------------------------------------------------------*/
#define USB_VENDOR_ID       0x1209
#define USB_PRODUCT_ID      0x5523

/* 1 = keyboard report IDs + consumer/media keys in interface 0.
 * 0 = plain 8-byte boot keyboard (best Windows compatibility). */
#define CONSUMER_ENABLE     1
#define USB_DEVICE_VERSION  0x0001
/* The actual USB string descriptors are in firmware/src/usb.c.  Keep the
 * names below in sync with them if you change the product name. */
#define USB_MANUFACTURER    "CH552"
#define USB_PRODUCT         "CH552 QMK Pad"

/*---------------------------------------------------------------*
 * System clock                                                  *
 *                                                               *
 * 16 MHz is the default: it is fast enough for USB, works with  *
 * both 5 V and (within datasheet limits) 3.3 V, and gives the   *
 * RGB SPI divider an exact-enough value.  24 MHz is also fine.  *
 *---------------------------------------------------------------*/
#ifndef FREQ_SYS
#define FREQ_SYS            16000000UL
#endif

/*---------------------------------------------------------------*
 * Keyboard scan mode                                            *
 *                                                               *
 * MATRIX: rows x columns, one diode per key recommended.        *
 * DIRECT: one pin per key, key to GND, no diodes, no row pin.   *
 *---------------------------------------------------------------*/
#define KEYBOARD_MODE_MATRIX 0
#define KEYBOARD_MODE_DIRECT 1

#define KEYBOARD_MODE       KEYBOARD_MODE_MATRIX

#if KEYBOARD_MODE == KEYBOARD_MODE_DIRECT

/* Each key connects its pin to GND.  Example: 6 keys on P1.0..P1.3 and
 * P3.0..P3.1.  MATRIX_ROWS is 1, MATRIX_COLS is DIRECT_KEY_COUNT. */
#define DIRECT_KEY_COUNT    0
#define DIRECT_PINS         { }

#define MATRIX_ROWS         1
#define MATRIX_COLS         DIRECT_KEY_COUNT

#else /* matrix mode */

/* 3 rows x 4 columns = 12 keys.  Change these two numbers and the pin
 * lists below together.  KEY_COUNT is derived automatically. */
#define MATRIX_ROWS         3
#define MATRIX_COLS         4

/* One {port, bit} entry per row.  Rows are driven low one at a time. */
#define ROW_PINS            { {1, 0}, {1, 1}, {1, 2} }

/* One {port, bit} entry per column.  Columns are inputs with pull-ups. */
#define COL_PINS            { {3, 0}, {3, 1}, {3, 2}, {3, 3} }

#define DIRECT_KEY_COUNT    0
#define DIRECT_PINS         { }

#endif /* scan mode */

#define KEY_COUNT           (MATRIX_ROWS * MATRIX_COLS)

#if MATRIX_ROWS < 1 || MATRIX_COLS < 1
#error "MATRIX_ROWS and MATRIX_COLS (or DIRECT_KEY_COUNT) must be at least 1"
#endif

#if KEY_COUNT > 64
#error "KEY_COUNT > 64: the scanner/report code uses 8-bit key indices"
#endif


/*---------------------------------------------------------------*
 * Layers and debounce                                           *
 *                                                               *
 * Keep the keymap small enough for the 128-byte data flash:     *
 *   8 + KEY_COUNT * DYNAMIC_KEYMAP_LAYERS * 2 + 1 <= 120       *
 * For 12 keys that allows up to 4 layers; for 16 keys, 3.       *
 *---------------------------------------------------------------*/
#define DYNAMIC_KEYMAP_LAYERS 2
#if DYNAMIC_KEYMAP_LAYERS < 1
#error "DYNAMIC_KEYMAP_LAYERS must be at least 1"
#endif
#define DEBOUNCE_MS           5
#if DEBOUNCE_MS < 1
#error "DEBOUNCE_MS must be at least 1"
#endif

/*---------------------------------------------------------------*
 * RGB (WS2812 / WS2812B, GRB order by default)                  *
 *                                                               *
 * The driver uses SPI0 at about 2.4-3 MHz and encodes each LED  *
 * bit as three SPI bits (100 = 0, 110 = 1).  This gives stable   *
 * WS2812 timing without cycle-counted delays.                   *
 *                                                               *
 * Set RGB_ENABLE to 0 to compile the whole subsystem out.       *
 * WS2812_LED_COUNT should normally equal KEY_COUNT; it may be   *
 * smaller or larger, the effects simply address the strip.      *
 *---------------------------------------------------------------*/
#define RGB_ENABLE             1
#define WS2812_LED_COUNT       12
#define WS2812_DRIVER_BITBANG  0   /* 0 = SPI0 (P1.5/P1.7), 1 = GPIO bit-bang */
#define WS2812_GRB             1   /* 1 = GRB (most strips), 0 = RGB */
#define RGB_DEFAULT_BRIGHTNESS 64  /* 0-255; keep USB current sane     */
#define RGB_DEFAULT_HUE        0
#define RGB_DEFAULT_SAT        255
#define RGB_DEFAULT_EFFECT     1   /* 1 = solid                       */
#define RGB_DEFAULT_SPEED      128
#define RGB_UPDATE_MS          20  /* effect refresh period           */

/*---------------------------------------------------------------*
 * Default keymap                                                *
 *                                                               *
 * Flat layout, one row per physical row.  Use KC_TRNS (_______) *
 * on a higher layer to fall through to the layer below.         *
 * MO(1) is a momentary layer key; TG(1) would toggle layer 1.   *
 *                                                               *
 * The first layer should normally start with layer 0.  Matrix   *
 * position (row, col) maps to element [layer][row][col].        *
 *---------------------------------------------------------------*/
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

#endif /* BOARD_4KEY */

#endif /* __KEYBOARD_CONFIG_H__ */
