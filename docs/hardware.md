# 硬件接线

以下引脚全部来自 `empty.syscfg` / `ti_msp_dl_config.h`，与代码一致。
“封装脚”指 LP-MSPM0G3507（LQFP-64）的物理管脚号，方便对照原理图。

## 总表

| 功能 | 信号 | MCU 引脚 | 引脚复用号 | 封装脚 | 备注 |
| --- | --- | --- | --- | --- | --- |
| 左电机 PWM | WHEEL_C0 | PA26 | PINCM59 | — | TIMG7 CCP0 |
| 右电机 PWM | WHEEL_C1 | PA24 | PINCM54 | — | TIMG7 CCP1 |
| 左电机方向 | IN1_1 | PA14 | PINCM36 | 7 | H 桥 IN1 |
| | IN1_2 | PA15 | PINCM37 | 8 | H 桥 IN2 |
| 右电机方向 | IN2_1 | PA16 | PINCM38 | 9 | H 桥 IN3 |
| | IN2_2 | PA17 | PINCM39 | 10 | H 桥 IN4 |
| 左编码器 | LEFT_A | PA0 | PINCM1 | 33 | 中断源 |
| | LEFT_B | PA1 | PINCM2 | 34 | 方向判据 |
| 右编码器 | RIGHT_A | PA8 | PINCM19 | 54 | 中断源 |
| | RIGHT_B | PA9 | PINCM20 | 55 | 方向判据 |
| 灰度阵列 | AD0 | PA31 | PINCM6 | 39 | 输出 |
| | AD1 | PB4 | PINCM17 | 52 | 输出 |
| | AD2 | PB5 | PINCM18 | 53 | 输出 |
| | OUT | PA28 | PINCM3 | 35 | 输入 |
| OLED | SCL | PB2 | PINCM15 | 50 | 软件 I2C |
| | SDA | PB3 | PINCM16 | 51 | 软件 I2C |
| W25Q64 | CS | PA27 | PINCM60 | 31 | 软件 SPI |
| | CLK | PB25 | PINCM56 | 27 | |
| | DI (MOSI) | PB20 | PINCM48 | 19 | |
| | DO (MISO) | PA25 | PINCM55 | 26 | |
| 按键 | UP | PB8 | PINCM25 | 60 | 内部上拉 |
| | DOWN | PB6 | PINCM23 | 58 | 内部上拉 |
| | ENTER | PB7 | PINCM24 | 59 | 内部上拉 |
| LED | LIGHT | PB22 | PINCM50 | 21 | |
| 调试串口 | UART0 TX | PA10 | PINCM21 | — | 115200 8N1 |
| | UART0 RX | PA11 | PINCM22 | — | |

## 电机驱动

`PWM_Run()` 输出的是“占空比反相”的比较值：`compare = 1000 - |speed|`，
即比较值越小电机越快；这是为兼容所用 H 桥模块的低电平有效输入。
若换用高电平有效的驱动板，需把 `PWM.c` 中两处 `MAX_SPEED - abs(...)` 改为 `abs(...)`。

方向真值（以左轮为例）：

| 目标 | IN1_1 | IN1_2 | 结果 |
| --- | --- | --- | --- |
| 正转 | 1 | 0 | 前进 |
| 反转 | 0 | 1 | 后退 |
| 停 | 0 | 0 | 滑行（`PWM_stop()` 用此状态） |

右轮方向极性相反（`IN2_1=0, IN2_2=1` 为正转），因为电机左右镜像安装。
实际接线若左右都朝同一方向转，交换对应 IN 两根线或改 `PWM_Run()` 中的极性。

`PWM_sleep()` 会先把比较值拉满（占空比 0）再清方向脚，避免上电瞬间抽动。

## 灰度阵列

传感器是**地址复用**的：3 根地址线选通一路，1 根数据线回读，8 路轮流读一遍。
每次切换地址后需约 50 µs 稳定时间，`bsp_IR_io.c` 用空循环实现。

- 极性：`GRAY_ACTIVE_LEVEL = 0`，低电平表示压到黑线。若发现黑白反了，改成 1。
- 位序：`addr=0` 对应最左一路，`data` 的 bit0 即第 1 路。
- **第 1 路（addr=0）在实车上常亮**，由 `BROKEN_SENSOR_MASK` 屏蔽；换新传感器后
  记得去掉这个掩码，并把 `Calculate_Line_Position()` 的循环范围改回 `0..7`。
- 阵列间距 11 mm，`SENSOR_POSITIONS[]` 以车体中心为原点，共 8 个位置。

## OLED

软件 I2C，SCL=PB2、SDA=PB3，默认地址 `0x78`（7 位地址 0x3C）。
上拉电阻 4.7 kΩ 到 3.3 V；模块自带时可不加。

中文字模不存在固件里，而是运行时从 W25Q64 的 `0x000000` 按 GB2312 编码取 32 字节点阵，
因此**首次使用必须先用 `tools/send_font.bat` 烧字库**，否则只显示 ASCII。
注意当前固件未把 `FontReceiver_Run()` 接进菜单，空片烧字库需按
[tools/readme.txt](../tools/readme.txt) 的说明临时加一行入口。

## W25Q64

软件 SPI（GPIO 位操作），CS 低有效。时钟极性：空闲低、上升沿采样。
字库与参数的分区见 README 的“参数存储布局”。

## 供电

- MCU 与传感器 3.3 V；电机驱动与电机建议独立电池供电。
- 电机电源与逻辑电源共地，否则灰度读数会漂。
- 编码器是开漏/推挽输出均可，代码用内部上拉读取。

## 上电顺序

1. 先接好地线与 3.3 V，确认 OLED 能点亮。
2. 烧固件 → 烧字库。
3. 架空车轮，进 `pwm` 菜单给小值，确认两轮转向与预期一致。
4. 再放到赛道上调参（见 [tuning.md](tuning.md)）。
