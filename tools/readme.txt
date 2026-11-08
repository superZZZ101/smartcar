============================================================
  OLED 字库工具 — GB2312 16×16 + ASCII 8×16 点阵 生成/写入/校验
============================================================

本项目使用 Windows GDI (宋体) 生成点阵字库，供 MSPM0 + OLED + W25Q64
方案显示中文与半角 ASCII。

文件结构
--------
  tools/
    readme.txt          本文件
    gen_gb2312_font.py  仅生成 GB2312 字库（.bin + .h）
    gen_send_font.py    生成字库 + UART 写入 W25Q64（主工具）
    send_font.bat       一键发送（调用 gen_send_font.py）
    verify_font.py      独立校验 W25Q64 中字库的 CRC32
    gen_strings.py      把中文字符串转成 GB2312 十六进制宏（strings.h）
    gen_ascii_8x16.py   生成 8×16 半角 ASCII 点阵头文件 ascii_8x16_font.h
    scripts.txt         字库需覆盖的中文字符样本

  生成物（已在 .gitignore 中忽略，按需重新生成）:
    gb2312_hzk16.bin    HZK16 格式字库，282752 字节
    gb2312_font.h       同样的字库，C 头文件形式

  ASCII 头文件直接编译进固件，已随仓库提供：
    ../keil/Core/Inc/ascii_8x16_font.h

工作流程
--------
第 1 次刷固件（空片，W25Q64 里还没有字库）:
  1. 在 main() 里临时加入并编译烧录:
        FontReceiver_Run(GB2312_FONT_FLASH_ADDR);   // 阻塞等待 PC 下发
     说明: 当前常规固件没有把这个入口挂到菜单或启动流程里，
           所以必须先临时加这一行，否则 send_font.bat 无响应。
  2. 下载字库到 W25Q64:
       一键           send_font.bat COM4
       生成并发送     python gen_send_font.py --port COM4
  3. 脚本自动进行 CRC 校验
  4. 去掉那一行，重新烧常规固件（字库常驻 Flash，不会丢）

后续只更新字库:
  send_font.bat COM4

只想校验 (不写入):
  python verify_font.py COM4

调试说明
--------
  - 波特率默认 115200，可用 --baud 参数调整
  - W25Q64 擦除约需 28 秒，脚本会等待
  - 校验经常失败时降低波特率:
        send_font.bat COM4 38400
  - 校验不通过时，脚本会显示 PC 与 MCU 两端 CRC 的 XOR 差

ASCII 字库
----------
  重新生成（覆盖驱动目录中的头文件）:
    python gen_ascii_8x16.py --out-dir ../keil/Core/Inc

  需要 Windows 环境（依赖 GDI + 宋体）。仓库中的
  keil/Core/Inc/ascii_8x16_font.h 由该脚本原样生成。

MCU 端接口
-----------
  // 接收字库（阻塞，直到 CRC 校验完成）
  FontReceiver_Run(GB2312_FONT_FLASH_ADDR);

  // 只校验，不写入
  FontReceiver_VerifyFlash();

  // 设置字库基地址（默认即 0）
  OLED_SetFontFlashBase(GB2312_FONT_FLASH_ADDR);

  // 显示中文
  OLED_ShowChineseString(x, y, (const uint8_t*)"你好");
