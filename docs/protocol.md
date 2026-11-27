# USB / VIA 协议说明

## 1. USB 复合设备

| 项目 | 值 |
|------|----|
| VID:PID | 默认 3×4 板 `0x1209:0x5523`，4 键板 `0x1209:0x5524` |
| bcdUSB | USB 2.0 full speed |
| EP0 | 64 字节控制端点 |
| 接口 0 | HID 键盘 + Consumer Control |
| 接口 1 | HID raw vendor（VIA / VialRGB） |

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

### 接口 1（raw HID：VIA / VialRGB）

| 端点 | 方向 | 类型 | 包长 | 间隔 |
|------|------|------|------|------|
| `0x82` | IN | interrupt | 32（VIA / VialRGB） | 1 ms |
| `0x03` | OUT | interrupt | 32（VIA / VialRGB） | 1 ms |

HID report descriptor：

* usage page `0xFF60`
* usage `0x61`（top-level collection）
* input usage `0x62`，32 × 8 位
* output usage `0x63`，32 × 8 位

命令从 EP3 进入，固件把回复写到 EP2。默认 4 键固件（VIA + VialRGB）
和普通 3×4 VIA 固件都使用 32 字节报告；只有旧版 `OPENRGB_QMK=1`
改成 64 字节，见第 7 节。

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
| `0x04` firmware version | `0x00010002` |
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

在默认 4 键固件中，`channel` 为 `0x40`–`0x44` 时会被解释为
VialRGB 子命令，而不是 VIA 自定义通道；`1`/`2`/`3` 通道保持不变，
详见第 6 节。3×4 固件没有 VialRGB，这些通道仍按 VIA 自定义通道处理。

`0x09` 保存命令在 VIA 和 VialRGB 两种形式下都会保存同一份 RGB 配置。

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
| `0x04` | 布局版本（当前 3） |
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

---

## 6. VIAL / VIALRGB（默认 4 键固件）

默认 `make` 的 4 键固件包含 OpenRGB 的 **QMK VialRGB** 驱动所需的
最小 Vial/VialRGB 命令，不需要额外构建选项。raw HID 仍为 32 字节，
动态键位命令完全不变。3×4 固件因空间不足不包含 VialRGB。

### 6.1 OpenRGB 检测顺序

```text
TX [01 ...]              -> RX [01 00 0D ...]    VIA 协议 0x000D
TX [FE 00 ...]           -> RX [04 00 00 00 ...] Vial 协议 4
                                  [UID 8 bytes]
                                  [01]             VialRGB 支持标志
TX [08 40 ...]           -> RX [08 40 01 00 FF]  VialRGB 协议 1，最大亮度 255
TX [08 42 00 00 ...]     -> RX [08 42 01 00 02 00 FF FF ...]
```

OpenRGB 要求 VIA 协议 ≥ 9、Vial 协议 ≥ 4，且 VialRGB 标志 bit 0 为 1。
本固件返回 VIA `0x000D`、Vial `4`、VialRGB `1`。

### 6.2 VialRGB 读取（VIA 命令 `0x08`）

请求格式：

```text
TX: [08 subcmd args...]
RX: [08 subcmd payload...]
```

OpenRGB 从 `data[2]` 开始读取 payload。

| subcmd | 含义 | 响应 |
|--------|------|------|
| `0x40` | 信息 | `d[2..3]` VialRGB 版本 1（LE），`d[4]` 最大亮度 255 |
| `0x41` | 当前模式 | `d[2..3]` 模式 ID（LE），`d[4]` speed，`d[5]` hue，`d[6]` sat，`d[7]` value |
| `0x42` | 支持的效果 | 请求 `d[2..3]` 为“大于此 ID”；返回若干 16 位 LE ID，未用位置填 `0xFFFF` |
| `0x43` | LED 数量 | `d[2..3]` = `WS2812_LED_COUNT`（LE） |
| `0x44` | LED 信息 | 请求 `d[2..3]` 为索引；响应 `d[2]` x、`d[3]` y、`d[4]` flags、`d[5]` row、`d[6]` col、`d[7]` valid |

当前公开的效果 ID：

| ID | OpenRGB 名称 | 内部实现 |
|----|--------------|----------|
| `1` | Direct | 逐灯 HSV |
| `2` | Static | 单色效果（内部效果 1） |

只公开 Direct 和 Static 是为了让 OpenRGB 的模式索引始终有效；其他
内置动画仍可从 VIA 的灯光页设置。

### 6.3 VialRGB 写入（VIA 命令 `0x07`）

设置模式：

```text
TX: [07 41 mode_lo mode_hi speed hue sat value ...]
```

* `mode = 0`：关闭灯光
* `mode = 1`：Direct 模式
* `mode = 2`：Static 模式
* 其他值按 Static 处理

Direct 逐灯快速设置：

```text
TX: [07 42 start_lo start_hi count h0 s0 v0 h1 s1 v1 ...]
```

* `start` 是起始 LED 索引（LE）
* `count` 是本包 LED 数，最多 9 颗（27 字节 HSV）
* 颜色是 HSV，不是 RGB；固件转换后写入 `led_buf`
* 收到该命令会自动进入 Direct 模式

保存：

```text
TX: [09 ...]
```

把当前亮度、效果、速度、颜色写入 Data Flash 的 RGB 区。

### 6.4 与 VIA 共存

* `0x01`–`0x06`、`0x11`–`0x13` 等 VIA 动态键位命令保持不变。
* OpenRGB 只使用 `0x07`/`0x08` 的 `0x40`–`0x44` 子命令和 `0xFE`。
* VIA 仍使用 `0x07`/`0x08` 的通道 `1`/`2`/`3` 控制灯光。
* 键位保存在 Data Flash，OpenRGB 不会修改它。

> 4 键板默认键位为 `1 2 3 4`，因此 OpenRGB 中会列出 4 颗灯。固件同时
> 支持 VIA 和 VialRGB，但二者共用一个 raw HID 接口；Windows 下多个
> 程序同时读取可能互相抢包。添加 OpenRGB 设备时先关闭 VIA，平时尽量
> 只让一个程序主动刷新。
>
> OpenRGB 的 VialRGB 驱动不会扫描任意 VID/PID，只检查
> `OpenRGB.json` 中 `QMKVialRGBDevices` 已登记的设备和内置白名单。
> 0x1209:0x5524 需要第一次手动添加，或运行
> `tools/openrgb_register_vialrgb.py` 写入配置；之后即可自动识别。

---

## 7. 旧版 QMK-OpenRGB（OPENRGB_QMK=1）

`make OPENRGB_QMK=1` 保留原来的 QMK-OpenRGB revision D：

* raw HID 报告 64 字节，主机侧 65 字节（Report ID 0 + 64 数据）
* usage page `0xFF60`、usage `0x61`
* 协议版本 `0x0D`
* 命令 `0x01`–`0x09`
* 只暴露 Direct 模式
* 排除 `via.c`，不能与 VIA 同时使用
* 4 键板输出 `ch552_qmk_4key_openrgb_qmk.hex`
* 3×4 板输出 `ch552_qmk_openrgb_qmk.hex`

响应最后两字节为状态（50 = 成功）和 `100`（End Of Message）。
