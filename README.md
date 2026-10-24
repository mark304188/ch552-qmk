# CH552 QMK/VIA 键盘固件

[English](README_en.md) | 中文

在 CH552（增强型 8051 + USB 全速设备）上实现的一个小型键盘固件。
它不使用完整的 QMK 代码库，而是实现了 **QMK/VIA 的 raw HID 主机协议**，
因此可以用 VIA 直接改键、调 RGB，不需要每次重新编译。

默认构建目标是 **4 键直连板**：直接执行 `make` 即可。3×4 矩阵板用
`make BOARD=default` 构建。

## 特性

* CH552 USB 全速复合 HID 设备
  * 键盘 + 多媒体键（Consumer Control）
  * VIA raw HID 接口（usage page `0xFF60` / usage `0x61`）
* VIA 协议 `0x000D`
  * 动态键位读取 / 修改 / 重置
  * 键位缓冲区读写（`id_dynamic_keymap_get/set_buffer`）
  * RGB 亮度、效果、速度、颜色
  * 跳转 WCH ISP bootloader
* 动态键位保存在 CH552 片内 128 字节 Data Flash 中，掉电不丢
* 按键矩阵或“每键一个引脚”的直连模式，改 `keyboard_config.h` 即可
* 分层（MO / TG）与透明键 `KC_TRNS`
* WS2812/WS2812B RGB，使用硬件 SPI 编码，不依赖精确的软件延时
* 支持常用 QMK 键码、多媒体键、`QK_BOOTLOADER`、`QK_CLEAR_EEPROM`
* SDCC 构建，默认 16 MHz；可切 12/24 MHz

> 这是“QMK 协议兼容”的轻量实现，不是完整 QMK。宏、Tap Dance、
> Combo、旋钮、NKRO 等没有实现，见文末的限制。

---

## 目录结构

```
.
├── Makefile
├── firmware/
│   ├── include/
│   │   ├── keyboard_config.h   <- 主要配置文件
│   │   ├── board_4key.h        4 键直连板配置
│   │   ├── keycodes.h          QMK 键码数值
│   │   ├── ch552.h             CH552 寄存器定义
│   │   ├── ch554_usb.h         USB 常量 / SETUP 结构
│   │   ├── usb.h
│   │   ├── keyboard.h
│   │   ├── matrix.h
│   │   ├── via.h
│   │   ├── rgb.h
│   │   ├── ws2812.h
│   │   ├── eeprom.h
│   │   └── debug.h
│   └── src/
│       ├── main.c              主循环
│       ├── usb.c               USB 描述符、EP0、HID 端点、VIA 传输
│       ├── keyboard.c          扫描消抖、分层、动态键位、HID 报告
│       ├── matrix.c            矩阵 / 直连扫描
│       ├── via.c               VIA raw HID 命令分发
│       ├── rgb.c               RGB 效果与 VIA 设置
│       ├── ws2812.c            SPI 编码 WS2812 驱动
│       ├── eeprom.c            Data Flash 读写
│       └── debug.c             时钟、延时、1 ms 系统 tick
├── via/
│   ├── ch552_qmk_pad.json      VIA 3 键盘定义（3x4 布局）
│   ├── ch552_qmk_pad_via2.json VIA 2 兼容定义（3x4）
│   ├── ch552_qmk_4key.json     VIA 3 键盘定义（4 键直连板）
│   └── ch552_qmk_4key_via2.json VIA 2 兼容定义（4 键）
├── tools/
│   └── via_probe.py            不依赖 VIA 的命令行协议测试工具
└── docs/
    ├── hardware.md
    └── protocol.md
```

---

## 3×4 矩阵板硬件连接

3×4 配置是 **3 行 × 4 列 = 12 键**，每个键带一颗 WS2812：

| CH552 | 方向 | 连接到 |
|-------|------|--------|
| P1.0  | 输出 | 矩阵第 0 行 |
| P1.1  | 输出 | 矩阵第 1 行 |
| P1.2  | 输出 | 矩阵第 2 行 |
| P3.0  | 输入上拉 | 矩阵第 0 列 |
| P3.1  | 输入上拉 | 矩阵第 1 列 |
| P3.2  | 输入上拉 | 矩阵第 2 列 |
| P3.3  | 输入上拉 | 矩阵第 3 列 |
| P1.5  | 输出 | WS2812 DIN（SPI0 MOSI） |
| P1.7  | 输出 | 保留给 SPI0 SCK，不接 LED |
| P3.6  | USB   | D- |
| P3.7  | USB   | D+ |
| 5V    | 电源  | USB VBUS / 外部 5V |
| GND   | 地    | GND |

矩阵建议每键串一只二极管：**阳极接列，阴极接行**（本固件逐行拉低、
读列）。没有二极管时，同时按三个以上键可能出现鬼键。

> RGB 使用 P1.5/P1.7。启用 RGB 时不要把这几个引脚再分配给矩阵。
> P1.6 没有使用，可以留给矩阵；关闭 RGB 后 P1.5/P1.7 也可以释放。

WS2812 的电源请从外部 5V 走线，不要经过 CH552 的 3.3V LDO。
12 颗全白约 700 mA；默认亮度 64/255，仍建议加 100–470 µF 电容。

---

## 4 键直连板

对应的板级配置在 `firmware/include/board_4key.h`，3×4 配置完全保留。

| 丝印 | CH552 引脚 | 说明 |
|------|------------|------|
| But1 | P1.7 | 按键一端接该脚，另一端接 GND |
| But2 | P1.6 | 同上 |
| But3 | P1.4 | 同上 |
| But4 | P3.2 | 同上 |
| RGB  | P1.5 | WS2812 数据链 DIN（默认按 4 颗） |
| 5V/GND | 电源 | WS2812 建议外部 5V，共地 |

这个配置使用 **GPIO 软件 bit-bang** 驱动 WS2812，只占用 P1.5，因此 P1.7
可以留给 But1，不会和 SPI0 的 SCK 冲突。4 键板还使用**标准 8 字节
boot keyboard**（无 report ID、无 Consumer 集合），这是为了 Windows
兼容性；因此 4 键板上不发送多媒体键。默认键位：

```text
But1 -> 1
But2 -> 2
But3 -> 3
But4 -> MO(1)    按住 But4 时，But1..But3 变成 F1..F3
```

编译和烧录：

```bash
make                       # 默认 4 键板，16 MHz
make VCC=3V3               # 12 MHz
make flash                 # ch55xtool
```

4 键固件的 VIA 定义是 `via/ch552_qmk_4key.json`，VID:PID 为
`1209:5524`。3×4 固件是 `1209:5523`，两者可以分别加载。

> VIA 明确拒绝 `0xFEED` 作为 vendorId，所以固件和 JSON 都改用了
> `0x1209`。**需要重新烧录新固件**，否则设备仍然枚举为 `FEED:5524`，
> 与 JSON 匹配不上。

4 键板在 Windows 10/11 下已验证：标准键盘输入、VIA 改键、拔插后保存均正常。
它使用 `CONSUMER_ENABLE 0`（无 report ID 的 8 字节 boot keyboard），因此
不发送多媒体键；3×4 板仍然是 `CONSUMER_ENABLE 1`。

注意：

* 软件 bit-bang 的时序按 12/16 MHz 校准；`make BOARD=4key FREQ_SYS=24000000`
  会在编译时被拒绝。若一定要 24 MHz，可改回 SPI 驱动，但 P1.7 会被
  SCK 占用，不能再接 But1。
* 按键按“另一端接 GND、内部上拉”设计；如果你的按键是接 VCC 的高电平
  有效方式，需要把 `matrix.c` 的读取极性反过来。
* 4 键板默认按 4 颗 WS2812 配置（每键一颗）；如果实际灯珠数不同，
  把 `board_4key.h` 里的 `WS2812_LED_COUNT` 改成实际数量。

---

## 编译

需要 SDCC 4.x：

```bash
sudo apt install sdcc
make                 # 默认 4 键板
make BOARD=default   # 3×4 矩阵板
```

输出：

```
build/ch552_qmk_4key.hex   默认 4 键板的 Intel HEX
build/ch552_qmk_4key.bin   默认 4 键板的原始二进制
build/ch552_qmk.hex        3×4 板的 Intel HEX
build/ch552_qmk.bin        3×4 板的原始二进制
```

常用选项：

```bash
make VCC=3V3            # 3.3 V 供电，默认 12 MHz
make FREQ_SYS=24000000  # 24 MHz（5 V）
make clean
make size
```

默认 16 MHz。CH552 数据手册对 16 MHz 要求 VCC 高于 3.3 V；3.3 V 板子
建议用 `make VCC=3V3`（12 MHz）。

### 烧录

Windows：WCHISPTool，默认选择 `build/ch552_qmk_4key.hex`；3×4 板选择
`build/ch552_qmk.hex`。

Linux/macOS：

```bash
pip install ch55xtool
make flash                 # 默认 4 键板
make BOARD=default flash   # 3×4 板
```

首次烧录通常需要按住板上的 BOOT/下载键再上电，让芯片进入 WCH ISP
bootloader（VID:PID `4348:55E0`）。固件里也可以把某个键设成
`QK_BOOTLOADER`，或通过 VIA 发送 `id_bootloader_jump` 跳回 bootloader。

---

## 配置

所有用户配置都在：

```
firmware/include/keyboard_config.h
```

### 1. 改矩阵大小和引脚

例如改成 4 行 × 4 列：

```c
#define MATRIX_ROWS 4
#define MATRIX_COLS 4

#define ROW_PINS { {1, 0}, {1, 1}, {1, 2}, {1, 3} }
#define COL_PINS { {3, 0}, {3, 1}, {3, 2}, {3, 3} }
```

`{port, bit}` 中 port 只能是 1 或 3；P3.6/P3.7 是 USB，不能使用。
改完矩阵后，`via/ch552_qmk_pad.json` 里的 `matrix.rows/cols` 和
`layouts.keymap` 也要同步修改，否则 VIA 显示的位置会错。

### 2. 直连模式（每键一个引脚）

没有矩阵、每个按键一端接引脚、另一端接 GND 时：

```c
#define KEYBOARD_MODE       KEYBOARD_MODE_DIRECT
#define DIRECT_KEY_COUNT    6
#define DIRECT_PINS { {1,0}, {1,1}, {1,2}, {1,3}, {3,0}, {3,1} }
```

直连模式下 `MATRIX_ROWS` 自动为 1，`MATRIX_COLS = DIRECT_KEY_COUNT`；
同时把 `DEFAULT_KEYMAP` 改成 1 行 6 列，VIA 里把键盘定义写成
`"rows": 1, "cols": 6` 即可。4 键板的完整示例见 `board_4key.h`。

### 3. 默认键位与层数

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

* `KC_TRNS`（`_______`）表示“透明”，继续向下一层查找。
* `MO(1)` 按住时临时进入第 1 层；`TG(1)` 按一下切换。
* 第 0 层始终有效。
* 默认布局：按住右下角 `MO(1)` 后，上排变 F1–F4，中排变静音/音量/播放，
  下排变亮度。松开恢复。

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

* 不需要 RGB：`RGB_ENABLE 0`，`WS2812_LED_COUNT 0`，固件会小约 3 KB。
* 3×4 配置使用 SPI0（P1.5/P1.7）；4 键配置使用
  `WS2812_DRIVER_BITBANG 1`，只占 P1.5。
* `WS2812_LED_COUNT` 最大 32（受 1 KB xRAM 限制）。
* 灯珠顺序大多是 GRB，设 `WS2812_GRB 1`；如果是 RGB 顺序设为 0。
* 效果编号大致对应 QMK `RGBLIGHT_MODE_*`：1 单色、2–4 呼吸、
  5–6 彩虹、7–13 彩虹旋转、14–19 蛇形、20–21 骑士、22 圣诞、
  23–33 渐变、34 RGB 测试、35 交替、36–41 闪烁。

### 5. 键位存储容量

CH552 只有 128 字节 Data Flash，其中 8 字节做头、1 字节校验、8 字节
RGB 设置，键位可用约 103 字节。约束是：

```
8 + KEY_COUNT * DYNAMIC_KEYMAP_LAYERS * 2 + 1 <= 120
```

例如 12 键最多 4 层，16 键最多 3 层。超出会在编译时报错。

---

## 在 VIA 中使用

1. 用 Chrome/Edge 打开 <https://usevia.app>。
2. 连接键盘，点击 Authorize device，选择 `CH552 QMK Pad`
   （VID:PID `1209:5523`）。
3. 打开 **Settings**，勾选 **Show Design tab**。
4. 进入 **Design** 标签，点 **Load Draft Definition**，选择
   `via/ch552_qmk_pad.json`（VIA 3）或 `via/ch552_qmk_pad_via2.json`
   （旧版 VIA 2）。

> VIA 3 用 `menus` 字段，旧的 `lighting` 字段已被移除；VIA 2 则相反。
> 如果导入时提示 `must NOT have additional properties`，通常就是文件版本
> 和 VIA 版本不匹配，换用另一个 JSON 即可。
5. 回到 **Configure** 标签即可改键；**Lighting** 标签控制 RGB。

如果烧的是 4 键直连板，设备名是 `CH552 QMK 4Key`（`1209:5524`），
第 4 步选择 `via/ch552_qmk_4key.json`（VIA 3）或
`via/ch552_qmk_4key_via2.json`（旧版 VIA 2）。

VIA 的改键会立即通过 `id_dynamic_keymap_set_keycode` 写进 Data Flash。
如果要把键盘恢复默认：

* 在 VIA 里使用 Reset Keymap；
* 或把某个键设为 `QK_CLEAR_EEPROM` 按一下；
* 或在 VIA 的 Design/Protocol 里发送 EEPROM reset。

修改了 `keyboard_config.h`（或 `board_4key.h`）里的矩阵大小 / 层数后，
记得同步修改对应 JSON。
如果 VIA 不显示键盘，可以先用命令行确认协议：

```bash
pip install pyusb
./tools/via_probe.py info
./tools/via_probe.py get 0 0 0
./tools/via_probe.py rgb
```

在 Linux 上如果 raw HID 接口被 `usbhid` 占用，脚本会尝试 detach；
也可以临时用 `sudo` 运行。

---

## 支持的键码

* 基本 HID 键码 `0x04`–`0xE7`，即 `KC_A`、`KC_1`、`KC_ENT` 等。
* 修饰键 `0xE0`–`0xE7`：`KC_LCTL`、`KC_LSFT`、`KC_LGUI` 等。
* QMK 多媒体键 `0xA5`–`0xBE`（`KC_MUTE`、`KC_VOLU`、`KC_MPLY`、
  `KC_BRIU` 等），在 `CONSUMER_ENABLE 1` 时通过 Consumer Control 报告发送
  （3×4 板启用；4 键板为了 Windows 兼容性关闭）。
* 层键：
  * `MO(layer)`：当前 QMK `0x5220 + layer`，也兼容旧的 `0x5100 + layer`。
  * `TG(layer)`：当前 `0x5260 + layer`，也兼容旧的 `0x5200 + layer`。
* `QK_BOOTLOADER`：跳转 WCH bootloader。
* `QK_CLEAR_EEPROM`：恢复默认键位和 RGB 设置。
* `KC_NO` / `KC_TRNS`。

为兼容不同 VIA 版本，`0x5200`–`0x521F` 被当作 `TG` 处理；当前 QMK 的
`TO(layer)` 也落在这个区间，表现会接近“切换”而不是“独占切换”。

---

## VIA 协议实现

raw HID 报告固定 32 字节，输出端点 `0x03`，输入端点 `0x82`。
协议版本 `0x000D`。

| 命令 | 功能 |
|------|------|
| `0x01` | 协议版本 |
| `0x02` | uptime / layout / matrix state / firmware version / keycodes version |
| `0x03` | layout options / device indication（接受并忽略） |
| `0x04` / `0x05` | 读 / 写单个动态键位 |
| `0x06` | 恢复默认键位 |
| `0x07` / `0x08` / `0x09` | RGB 设置 / 读取 / 保存 |
| `0x0A` | 恢复默认键位 + RGB |
| `0x0B` | 跳转 bootloader |
| `0x0C`–`0x10` | 宏相关，返回空（本固件不支持宏） |
| `0x11` | 层数 |
| `0x12` / `0x13` | 键位缓冲区读 / 写 |

RGB 通道同时接受 `2`（`qmk_rgblight`）和 `3`（`qmk_rgb_matrix`），
value id：`1` 亮度、`2` 效果、`3` 速度、`4` 色相+饱和度。

更完整的时序、描述符和存储布局见 [`docs/protocol.md`](docs/protocol.md)。

---

## 实现说明

### USB

* EP0：控制传输，包含标准请求和 HID class 请求
* EP1 IN `0x81`：键盘 / 多媒体报告
* EP2 IN `0x82`：VIA raw HID IN
* EP3 OUT `0x03`：VIA raw HID OUT
* 报告描述符：
  * `CONSUMER_ENABLE 1`：report ID 1 键盘 + report ID 2 Consumer
  * `CONSUMER_ENABLE 0`：无 report ID 的标准 8 字节键盘（4 键板）
  * 另一个 HID 接口：usage page `0xFF60`，32 字节 raw HID
* 支持 boot protocol：`SET_PROTOCOL(0)` 后键盘报告不带 report ID，
  并停止发送多媒体报告。

### 矩阵扫描

* 每 1 ms 扫描一次；`DEBOUNCE_MS` 次连续相同状态才确认。
* 行推挽输出，逐行拉低；列准双向输入并打开上拉。
* 直连模式每个引脚独立上拉输入。

### RGB

* SPI0 工作在约 2.4–3 MHz。
* 每个 WS2812 位编码成 3 个 SPI 位：`0 -> 100`，`1 -> 110`。
* 发送一帧时短暂关闭中断，12 颗灯约 0.3 ms。
* 效果刷新周期 `RGB_UPDATE_MS`，默认 20 ms。

### Data Flash

| 偏移 | 内容 |
|------|------|
| `0x00`–`0x03` | `CQMK` |
| `0x04` | 布局版本 |
| `0x05` | 行数 |
| `0x06` | 列数 |
| `0x07` | 层数 |
| `0x08`… | 键位，按层/行/列排列，小端 16 位 |
| 键位后 1 字节 | 校验和 |
| `0x78`–`0x7F` | `RGB` + 亮度/色相/饱和/效果/速度 |

---

## 限制与注意事项

* **Data Flash 只有 128 字节**。键位多或层数多时请减少层数，或改用外部
  EEPROM。
* 没有宏、Tap Dance、Combo、旋钮、NKRO、鼠标键、音频等 QMK 功能。
* 矩阵没有内置去鬼键；多键同时按下请加二极管。
* `CONSUMER_ENABLE`：1 = 键盘 report ID + Consumer 多媒体；0 = 标准
  8 字节键盘（Windows 兼容性最好，4 键板使用）。
* 3×4 板的 RGB 使用 P1.5/P1.7；4 键板只使用 P1.5 做 bit-bang，
  P1.7 留给 But1。矩阵引脚和 VIA JSON 必须与实际配置一致。
* 发送 WS2812 帧时会短暂关闭中断。3×4 板 12 颗约 0.3 ms；4 键板 4 颗
  约 0.1 ms。灯珠越多，USB 抖动越明显，建议不超过 16–24 颗。
* 4 键板的 WS2812 是软件 bit-bang，时序按 12/16 MHz 校准；24 MHz 会
  在编译时被拒绝。3×4 板的 SPI 驱动可用于 12/16/24 MHz。
* 代码约 12 KB，用户区上限 14 KB（`0x3800`）。关闭 RGB 后约 9 KB。
* USB VID/PID：默认 4 键板为 `0x1209:0x5524`，3×4 板为
  `0x1209:0x5523`。`0x1209` 是 pid.codes 的开源硬件 VID；正式发布请按
  pid.codes 规则申请 PID，并同步修改 VIA JSON。

---

## 参考

* [ieiao/ch554_sdcc](https://github.com/ieiao/ch554_sdcc) — CH552/CH554 SDCC
  寄存器定义和 USB 设备例程写法参考
* [SoCXin/CH552](https://github.com/SoCXin/CH552) — CH552 资料汇总
* [QMK VIA 协议实现](https://github.com/qmk/qmk_firmware/blob/master/quantum/via.c)
* CH552/CH551 数据手册（WCH）
