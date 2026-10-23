# USB / VIA 协议说明

## 1. USB 复合设备

| 项目 | 值 |
|------|----|
| VID:PID | 默认 3×4 板 `0x1209:0x5523`，4 键板 `0x1209:0x5524` |
| bcdUSB | USB 2.0 full speed |
| EP0 | 64 字节控制端点 |
| 接口 0 | HID 键盘 + Consumer Control |
| 接口 1 | HID raw vendor（VIA） |

### 接口 0

| 端点 | 方向 | 类型 | 包长 | 间隔 |
|------|------|------|------|------|
| `0x81` | IN | interrupt | 9（`CONSUMER_ENABLE 1`）/ 8（`CONSUMER_ENABLE 0`） | 1 ms |

`CONSUMER_ENABLE 1` 时 HID report descriptor 中有两个 report ID：

* **Report ID 1**：键盘
  * byte 0：修饰键位图（LCtrl/LShift/LAlt/LGUI/RCtrl/RShift/RAlt/RGUI）
  * byte 1：保留
  * byte 2–7：最多 6 个 HID keyboard usage
* **Report ID 2**：Consumer Control
  * 16 位 usage，小端发送：`[0x02, usage_lo, usage_hi]`

Report ID 1 还包含标准的 1 字节 LED 输出报告（Caps/Num/Scroll 等）。
固件接受并忽略该报告，它的存在是为了让描述符符合标准 boot keyboard。

`CONSUMER_ENABLE 0`（4 键板）时移除 report ID 和 Consumer 集合，
只发送固定 8 字节键盘报告，EP1 最大包长 8。这是 Windows 兼容性最好的形式。

boot protocol（`SET_PROTOCOL(0)`）下只发送 8 字节键盘报告，不带 report ID；
Consumer 报告暂停发送。

### 接口 1（VIA raw HID）

| 端点 | 方向 | 类型 | 包长 | 间隔 |
|------|------|------|------|------|
| `0x82` | IN | interrupt | 32 | 1 ms |
| `0x03` | OUT | interrupt | 32 | 1 ms |

HID report descriptor：

* usage page `0xFF60`
* usage `0x61`（top-level collection）
* input usage `0x62`，32 × 8 位
* output usage `0x63`，32 × 8 位

VIA 发来的 32 字节命令从 EP3 进入，固件把回复写到 EP2。

## 2. VIA 命令

所有多字节字段在命令负载中按 VIA 的约定排列。下面用 `d[]` 表示 32 字节
报告。

### 0x01 协议版本

```
TX: [01 ...]
RX: [01 00 0D ...]      // 0x000D
```

### 0x02 读取键盘值

```
TX: [02 value_id ...]
```

| value_id | 回复 |
|----------|------|
| `0x01` uptime | `d[2..5]` 为 32 位毫秒（高字节在前） |
| `0x02` layout options | 0 |
| `0x03` matrix state | `d[2]` 原样回显 offset，`d[3]` 起为行位图，bit=1 表示按下 |
| `0x04` firmware version | `0x00010000` |
| `0x06` keycodes version | `0x00000009` |

### 0x03 设置键盘值

接受 layout options（`value_id 0x02`）和 device indication（`0x05`），
其他值返回 `0xFF`（unhandled）。

### 0x04 / 0x05 动态键位

```
读: TX [04 layer row col ...]
    RX [04 ... hi lo ...]

写: TX [05 layer row col hi lo ...]
```

键位是 16 位 QMK keycode，高字节在前。固件也会接受字节交换的写入
（例如 `0x5900` → `0x0059`、`0x0400` → `0x0004`），以兼容部分
Windows VIA/HID 路径；读取始终返回标准高字节在前格式。

### 0x06 键位复位

把动态键位恢复为 `keyboard_config.h` 中的 `DEFAULT_KEYMAP`。

### 0x07 / 0x08 / 0x09 RGB

```
设置: TX [07 channel value_id value ...]
读取: TX [08 channel value_id ...]
保存: TX [09 channel ...]
```

`channel`：

* `2` = `qmk_rgblight`
* `3` = `qmk_rgb_matrix`
* `1` = `qmk_backlight`（也映射到同一套 RGB）

`value_id`：

| id | 含义 | 数据 |
|----|------|------|
| `1` | 亮度 | `value` 0–255 |
| `2` | 效果 | `0` = 关闭，其他为效果编号 |
| `3` | 速度 | 0–255 |
| `4` | 颜色 | `value` = hue，下一字节 = saturation |

读取时结果写在 `d[3]`（颜色会再写 `d[4]`）。

### 0x0A / 0x0B

* `0x0A`：恢复键位和 RGB 默认值。
* `0x0B`：跳转 `0x3800`（WCH ISP bootloader）。

### 0x0C–0x10 宏

本固件不支持宏：

* `0x0C` 宏数量 = 0
* `0x0D` 宏缓冲区大小 = 0
* `0x0E/0x0F` 缓冲区读写被忽略
* `0x10` 宏复位

### 0x11 层数

```
RX: [11 layer_count ...]
```

### 0x12 / 0x13 键位缓冲区

```
读: TX [12 offset_hi offset_lo size ...]
    RX [12 ... data...]       // 从 d[4] 开始

写: TX [13 offset_hi offset_lo size data...]
```

缓冲区布局与 QMK 动态键位 EEPROM 一致：

```
offset = ((layer * KEY_COUNT) + key_index) * 2
```

16 位 keycode 按小端存放（低字节在前）。`size` 最大 28。

## 3. 键位编码

使用 QMK 的 16 位数值：

* `0x0000` = `KC_NO`
* `0x0001` = `KC_TRNS`
* `0x0004`–`0x00E7` = HID Keyboard/Keypad usage
* `0x00E0`–`0x00E7` = 修饰键
* `0x00A5`–`0x00BE` = QMK 多媒体键，发到 Consumer 报告
* `MO(layer)`：`0x5220 | layer`（兼容 `0x5100 | layer`）
* `TG(layer)`：`0x5260 | layer`（兼容 `0x5200 | layer`）
* `0x7C00` = bootloader
* `0x7C03` = 恢复默认

## 4. Data Flash 布局

CH552 的 Data Flash 地址范围 `0xC000`–`0xC0FF`，只有偶地址有效，
实际容量 128 字节。固件把字节偏移 `N` 写到 `0xC000 + (N << 1)`。

| 偏移 | 内容 |
|------|------|
| `0x00`–`0x03` | `CQMK` |
| `0x04` | 布局版本（当前 2） |
| `0x05` | `MATRIX_ROWS` |
| `0x06` | `MATRIX_COLS` |
| `0x07` | `DYNAMIC_KEYMAP_LAYERS` |
| `0x08`… | 键位，层主序、行主序、列主序、小端 |
| 键位后 1 字节 | 所有键位字节的 8 位和 |
| `0x78`–`0x7A` | `RGB` |
| `0x7B` | 亮度 |
| `0x7C` | hue |
| `0x7D` | saturation |
| `0x7E` | 效果 |
| `0x7F` | 速度 |

上电时如果头、行列数、层数或校验和不匹配，固件会加载默认键位并写回。

## 5. WS2812 时序

SPI0 配置为 master、mode 0、MSB first：

```
Fspi = Fsys / (2 * SPI0_CK_SE)
SPI0_CK_SE = Fsys / 4.8 MHz
```

| Fsys | SPI0_CK_SE | Fspi | 1 个 WS2812 位（3 个 SPI 位） |
|------|------------|------|-------------------------------|
| 12 MHz | 2 | 3.0 MHz | 1.0 µs |
| 16 MHz | 3 | 2.67 MHz | 1.125 µs |
| 24 MHz | 5 | 2.4 MHz | 1.25 µs |

编码：

* WS2812 `0` → SPI `100`（高 1/3）
* WS2812 `1` → SPI `110`（高 2/3）

每个 LED 的 3 个字节扩展成 9 个 SPI 字节。发送时关闭中断，帧结束后
再保持 MOSI 低至少 50 µs 作为 reset。

4 键板使用 GPIO bit-bang 驱动（`WS2812_DRIVER_BITBANG`），只占 P1.5。
软件循环按 12/16 MHz 校准，每个 WS2812 位约 0.8 µs，24 MHz 会在编译
时报错。两种驱动的 VIA/HID 协议完全相同，只有 RGB 输出方式不同。
