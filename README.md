# OLED 128x64 中文菜单 (MSPM0G3507 + W25Q64 GB2312 字库)

MSPM0G3507 驱动 I2C OLED (SSD1306/SH1106) 显示多级中文菜单，所有字符（含 ASCII）均使用 W25Q64 中存储的 GB2312 16×16 点阵字库渲染。

## 目录结构

```
oled-menu/
├── empty.c              # 主程序 (10 个主菜单 × 每个 2 个功能)
├── empty.syscfg         # SysConfig 引脚配置
├── ti_msp_dl_config.c/h # 由 SysConfig 自动生成
│
├── keil/                # Keil MDK 工程 (ARMClang V6.16)
│   ├── empty_LP_MSPM0G3507_nortos_keil.uvprojx
│   ├── startup_mspm0g350x_uvision.s
│   ├── mspm0g3507.sct
│   └── Core/
│       ├── Inc/
│       │   ├── OLED.h         # OLED 驱动头文件 (纯 GB2312 字库)
│       │   ├── oled_menu.h    # 多级菜单框架
│       │   ├── w25q64.h       # W25Q64 SPI 驱动
│       │   ├── key.h          # 三按键 (UP/DOWN/ENTER)
│       │   ├── delay.h        # 毫秒延时
│       │   └── led.h          # LED 控制
│       └── Src/
│           ├── oled.c         # OLED I2C 驱动 (GB2312 字模渲染)
│           ├── oled_menu.c    # 菜单逻辑 (页式翻页, 子菜单栈)
│           ├── w25q64.c       # W25Q64 驱动 (GPIO 模拟 SPI)
│           ├── key.c          # 按键扫描 (消抖, 边沿触发)
│           ├── delay.c        # 延时
│           ├── led.c          # LED 控制
│           ├── uart.c         # UART 通信
│           └── font_receiver.c# 字库接收写入 W25Q64
│
├── gcc/ / iar/ / ticlang/     # 其他工具链
└── tools/                     # 字库生成/刷写工具
    ├── send_font.bat          # 一键烧录字库
    ├── gen_gb2312_font.py     # 生成 GB2312 字库
    ├── gen_send_font.py       # 生成 + 串口发送
    ├── verify_font.py         # CRC 校验
    └── gb2312_hzk16.bin       # GB2312 16x16 点阵字库二进制
```

## ⚠ 编码警告

**`empty.c` 必须以 GB2312 / GBK 编码保存！**

文件中直接包含 GB2312 编码的中文字符串（如 `"返回"`、`"PID设置"`）。如果以 UTF-8 保存，编译器会将中文字符编码成 UTF-8 字节序列，运行时 OLED 无法正确识别，显示为乱码。

### Keil MDK 设置
菜单 `Edit` → `Configuration` → `Editor` → `Encoding` 选择:
- `Chinese Simplified (GB2312)` — 适用于 GB2312 编码文件
- 或 `Chinese Simplified (GB2312) - Codepage 936`

### VS Code 设置
底部状态栏点击 `UTF-8` → `Save with Encoding` → `Chinese Simplified (GB2312)`

或 `settings.json`:
```json
"files.encoding": "gb2312"
```

### 验证
正确的 GB2312 编码下，`"返回"` = `\xB7\xB5\xBB\xD8` (4 字节)。  
如果在十六进制编辑器看到 `\xE8\xBF\x94\xE5\x9B\x9E` (6 字节) 则是 UTF-8，OLED 会显示乱码。

文件头部包含 `#pragma clang diagnostic ignored "-Winvalid-source-encoding"`，  
ARMClang V6 在 GB2312 编码下会警告 "源文件编码无效"，此 pragma 抑制该警告。

---

## 硬件连接

| W25Q64  | MCU      | OLED I2C | MCU      | 按键   | MCU     | LED  | MCU     |
|---------|----------|----------|----------|--------|---------|------|---------|
| CS      | PA.27    | SCL      | PB.2     | UP     | PB.8    | LED  | PB.22   |
| CLK     | PB.25    | SDA      | PB.3     | DOWN   | PB.7    |      |         |
| DI      | PB.20    |          |          | ENTER  | PB.6    |      |         |
| DO      | PA.25    |          |          |        |         |      |         |

按键内部上拉，按下=低电平。OLED I2C 地址 0x3C (写地址 0x78)。

## 首次使用

### 1. 编码设置
将 `empty.c` 以 **GB2312 编码** 保存（见上方说明）。

### 2. 编译下载
用 Keil MDK 打开 `keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`，编译 (F7) 并下载 (F8) 到 MCU。

### 3. 烧录字库

首次使用必须将 GB2312 字库写入 W25Q64：

```bat
cd tools
send_font.bat COM4
```

将 `COM4` 替换为实际串口号。擦除约需 28 秒，完成后自动校验。

也可手动运行:
```bat
python gen_send_font.py --port COM4
```

### 4. 运行
重新上电或按复位，OLED 显示中文主菜单。UP/DOWN 切换，ENTER 进入。

## 菜单操作

```
主菜单 (OLED 显示):
  > PID设置          ← 当前选中项
    八路巡线显示
    小车电机
    舵机
    ...

按 ENTER 进入子菜单:
  > P                ← PID设置子菜单
    I
    D
    返回

选中 "P" → ENTER → LED 闪烁 1 次
选中 "I" → ENTER → LED 闪烁 2 次
选中 "D" → ENTER → LED 闪烁 3 次
选中 "返回" → 回到主菜单
```

## 字库说明

- **格式:** HZK16 (94 区 × 94 位 × 32 字节/字符)
- **大小:** 282,752 字节 (~276 KB)
- **写入位置:** W25Q64 起始地址 0x000000
- **编码:** GB2312 (双字节, 区码 A1-F7, 位码 A1-FE)
- **字体:** 宋体 (SimSun), 16×16 点阵

## 引脚配置 (SysConfig)

通过 `empty.syscfg` 配置，如需修改引脚请在 SysConfig 中更改并重新生成。

关键 GPIO 分组:
- `OLED` — PB2(SCL), PB3(SDA), GPIO 模拟 I2C
- `KEY` — PB8(UP), PB7(DOWN), PB6(ENTER), 内部上拉
- `W25Q64` — CS=PA27, CLK=PB25, DI=PB20, DO=PA25
- `LED` — PB22
- `UART0` — PA10(TX), PA11(RX)

---

## 依赖

- **MCU:** TI MSPM0G3507 (Cortex-M0+)
- **SDK:** MSPM0 SDK 2.10.00.04
- **Toolchain:** Keil MDK (ARMClang V6.16)
- **Display:** 128×64 I2C OLED (SSD1306/SH1106)
- **Flash:** Winbond W25Q64JV (8MB SPI NOR)
