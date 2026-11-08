# MSPM0G3507 循迹小车

基于 TI **MSPM0G3507**（Cortex-M0+，32 MHz）的双轮循迹小车固件：8 路灰度传感器寻线，
编码器闭环测速，转向 PID + 速度环 PI 串级控制，0.96″ OLED 板载菜单在线调参，
参数存于板载 W25Q64 Flash（A/B 双版本槽），掉电不丢。

![MCU](https://img.shields.io/badge/MCU-MSPM0G3507-blue)
![Toolchain](https://img.shields.io/badge/toolchain-Keil%20MDK%20%7C%20tiarmclang-green)
![License](https://img.shields.io/badge/license-MIT-yellow)

## 功能

| 功能 | 说明 |
| --- | --- |
| 循迹 | 8 路灰度阵列（3 线地址复用），加权求线位置，实测只用中间 6 路 |
| 转向控制 | 位置式 PID，微分项带一阶低通（0.6/0.4），积分限幅 ±100 |
| 速度控制 | 左右轮独立速度环 PI，目标 + 修正量输出到 PWM |
| 弯道自适应 | 由 PID 输出算曲率，转弯时按曲率降速，并按轮距分配左右轮速差 |
| 丢线找回 | 记忆最后一次有效位置，向该侧差速转向；超过 2 s 停车 |
| 在线调参 | OLED 菜单改 P/I/D、速度环 PI、PWM 开环，边跑边调，即时生效 |
| 参数持久化 | W25Q64 双版本槽（A/B）保存/载入，可对比两套参数 |
| 中文显示 | GB2312 16×16 字库存 W25Q64，ASCII 8×16 字库编译进固件 |

## 硬件

| 部件 | 型号 / 说明 |
| --- | --- |
| 主控 | MSPM0G3507（LP-MSPM0G3507 LaunchPad 或同类最小系统） |
| 电机 | 双直流减速电机 + 霍尔编码器，AB 相正交解码 |
| 电机驱动 | 双 H 桥模块，方向由 4 根 GPIO 控制，调速走 TIMG7 双路 PWM |
| 循迹传感器 | 8 路灰度/红外阵列，3 位地址线 + 1 位数据线复用读取 |
| 显示 | 0.96″ 128×64 I2C OLED（SSD1306/SH1106，地址 0x78） |
| 存储 | W25Q64（8 MB SPI NOR）：字库 + 参数 |
| 交互 | UP / DOWN / ENTER 三按键 + 1 个 LED |
| 调试串口 | UART0，115200 8N1（字库下发与 PID 调试打印） |

完整引脚表见 [docs/hardware.md](docs/hardware.md)。

## 软件架构

```text
                    ┌──────────────┐
   8 路灰度 ───────►│ bsp_IR_io    │ 地址复用扫描 → 传感器字节
                    └──────┬───────┘
                           ▼
                    ┌──────────────┐  线位置(mm)
                    │line_following│◄─────────────┐
                    │  Calculate_  │              │
                    │Line_Position │              │
                    └──────┬───────┘              │
                           ▼ 位置式 PID           │
                    ┌──────────────┐              │
                    │Wheelbase_    │  曲率→降速    │
                    │Control       │  + 轮距分配   │
                    └──────┬───────┘              │
                           ▼ 目标轮速              │
                    ┌──────────────┐              │
                    │ 速度环 PI     │◄──────┐      │
                    └──────┬───────┘       │      │
                           ▼ PWM           │ 编码器│
                    ┌──────────────┐  ┌────┴─────┐│
                    │  PWM / 电机   │  │ Encode   ││
                    └──────────────┘  └──────────┘│
                                                  │
   按键 ──► oled_menu ──► OLED 显示 / 参数读写 ────┘
              │
              └──► W25Q64（字库 + 参数 A/B 槽）
```

| 模块 | 文件 | 职责 |
| --- | --- | --- |
| 主程序 / 菜单项 | `empty.c` | 菜单树定义、各调参回调、`main()` 循环 |
| 循迹控制 | `keil/Core/Src/line_following.c` | 线位置解算、PID、速度环、丢线找回 |
| 传感器驱动 | `keil/Core/Src/bsp_IR_io.c` | 地址复用扫描、极性处理、屏上状态显示 |
| 编码器 | `keil/Core/Src/Encode.c` | GPIO 中断四倍频计数，按周期换算速度 |
| 电机输出 | `keil/Core/Src/PWM.c` | 方向 GPIO + TIMG7 比较值，限幅与刹车 |
| 菜单框架 | `keil/Core/Src/oled_menu.c` | 页式翻页、子菜单栈、按键回调 |
| 显示驱动 | `keil/Core/Src/oled.c` | 软件 I2C、GB2312 字模按需从 Flash 读取 |
| 参数存储 | `keil/Core/Src/param.c` | W25Q64 扇区读写、magic 校验、默认值 |
| 字库下发 | `keil/Core/Src/font_receiver.c` | UART 协议 → 分块写 W25Q64 + CRC 校验 |

## 控制算法要点

**线位置解算**：8 路传感器物理间距 11 mm，以车体中心为原点取
`[-38.5, -27.5, -16.5, -5.5, 5.5, 16.5, 27.5, 38.5]` mm。仅中间 6 路参与加权，
先算平均值，再与左右边界均值按 `0.3 : 0.7` 融合，抑制单点抖动。
无有效传感器时返回 `-1000` 作为丢线标志。

**转向 PID**：目标线位置恒为 0，误差即线位置。微分项做一阶低通
（本次 0.6 + 上次 0.4），避免灰度跳变引起微分冲击；积分限幅 ±100，输出限幅 800。

**速度环**：`输出 = 目标轮速 + Kp_spd·e + Ki_spd·∫e`，积分限幅 ±1e5，
负值一律夹到 0（本车不主动倒车）。编码器在 10 ms 控制周期内累加计数，
乘标定系数 4.557 得到速度量纲。

**限速**：编码器速度超过 `LINE_SPEED_LIMIT`(600) 时，按超出量的 3 倍回拉，
保证超速时主动减速而不是硬顶。

**丢线**：连续丢线期间朝最后有效位置一侧以 `BASE_SPEED×0.7 / ×0.1` 差速寻找，
超过 2 s 仍未找回则 `PWM_Run(0,0)` 停车。

## 参数存储布局（W25Q64）

| 地址 | 用途 |
| --- | --- |
| `0x000000` | GB2312 16×16 字库（282,752 字节） |
| `0x050000` | 当前参数（`ParamBlock`，magic `0x50505253`） |
| `0x051000` | 版本槽 A |
| `0x052000` | 版本槽 B |

`ParamBlock` 含 `kp / ki / kd / kp_spd / ki_spd`。载入时校验 magic，不匹配则保留当前值。

## 板载菜单

```text
run       循迹运行（进入后按 ENTER 退出）
PID       ├ P / I / D 增量调节（步长循环切换）
          ├ step  步长：±0.0025 / ±0.02 / ±0.1 / ±1.0
          ├ CLEAR 清零
          └ back
spdPI     ├ Sp / Si 速度环调节
          ├ step  步长：±0.005 / ±0.02 / ±0.1 / ±1.0
          ├ CLEAR
          └ back
PI tune   左右轮独立阶梯调速，观察编码器跟随（ENTER 切换 L/R/退出）
IR        8 路传感器实时状态
encode    左右编码器速度
pwm       ├ L / R 开环给 PWM
          ├ step:±100 调节方向
          ├ CLEAR
          └ BACK
lost:0/1  丢线找回逻辑开关
ver       ├ save A / save B 存当前参数
          ├ load A / load B 载入
          └ back
```

所有调节立即写回 W25Q64，掉电保存。

## 编译与烧录

**前置**：TI MSPM0 SDK（2.x，含 DriverLib）、Keil MDK 5 + MSPM0 器件包
（ARMClang V6）；或使用 SDK 自带的 tiarmclang / GCC 工具链。

Keil 工程：

```text
keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx
```

打开 → Rebuild (F7) → Download (F8)。

SysConfig 配置在 `empty.syscfg`，改了引脚或外设后用 SysConfig 重新生成
`ti_msp_dl_config.c/h`。仓库中另有 `gcc/`、`iar/`、`ticlang/` 三套工程描述文件。

**字库**：中文字模不在固件里，运行时从 W25Q64 的 `0x000000` 按需读取，
所以 Flash 为空芯片必须先写入一次字库。注意：`font_receiver.c` 已实现完整的
UART 下发 + CRC 校验协议，但**当前主程序没有把它挂到菜单或启动流程里**
（见“已知限制”）。给空片烧字库的做法是临时在 `main()` 里加一行：

```c
FontReceiver_Run(GB2312_FONT_FLASH_ADDR);   /* 阻塞，PC 端发起下载 */
```

编译烧录后执行：

```bat
cd tools
send_font.bat COM4
```

脚本生成 GB2312 字库、经 UART 下发，由 MCU 写入 W25Q64，全程约 30 s，结束自动 CRC 校验。
完成后再把那一行去掉、重新烧常规固件即可；字库常驻 Flash，不会丢。
详见 [tools/readme.txt](tools/readme.txt)。

## 目录结构

```text
smartcar/
├── empty.c                 主程序与菜单项
├── empty.syscfg            SysConfig 外设/引脚配置
├── ti_msp_dl_config.c/h    由 SysConfig 生成
├── keil/
│   ├── empty_LP_MSPM0G3507_nortos_keil.uvprojx
│   └── Core/
│       ├── Inc/  OLED.h  oled_menu.h  w25q64.h  key.h  PWM.h  Encode.h
│       │         line_following.h  bsp_IR_io.h  param.h  ...
│       └── Src/  oled.c  oled_menu.c  w25q64.c  key.c  PWM.c  Encode.c
│                 line_following.c  bsp_IR_io.c  param.c  ...
├── gcc/  iar/  ticlang/    其他工具链工程
├── tools/                  字库生成/下发/校验脚本
└── docs/                   硬件接线与调参说明
```

## 已知限制

- 第 1 路灰度传感器在实车上常亮（硬件损坏），由 `BROKEN_SENSOR_MASK` 屏蔽，
  线位置解算也只取中间 6 路；换传感器后应同步修改这两个地方。
- 控制周期宏 `CTRL_INTERVAL_MS` 当前为 10 ms，但 `Line_Following_Update()`
  的注释仍写着 20 ms，以宏定义为准。
- 轮距 `WHEELBASE_MM`(135) 与传感器间距为固定宏，换车体需重新标定。
- 编码器标定系数 4.557 依赖 10 ms 周期与轮径，改周期需重算。
- 未做电池电压监测与低压保护。
- `font_receiver.c` 的字库下发入口 `FontReceiver_Run()` 未接入菜单，属于
  “能编译、未挂载”的状态；空片第一次烧字库需按上文临时加一行。
- `keil/Core/Inc/strings.h` 是早期菜单 Demo 留下的 GB2312 字符串宏，当前无人包含，
  属于死代码；因 `uvprojx` 仍引用它，暂未删除。

## 调参

见 [docs/tuning.md](docs/tuning.md)：调参顺序、步长选择、一组实测可用参数、常见故障排查。

## 许可

[MIT](LICENSE)。`ti_msp_dl_config.c/h` 由 TI SysConfig 生成，版权归
Texas Instruments（BSD-3-Clause），保留其原始声明。
