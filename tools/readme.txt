============================================================
  OLED 字库工具 — GB2312 16×16 点阵字库生成/写入/校验
============================================================

本项目使用 Windows GDI (宋体) 生成 GB2312 全字库 16×16 点阵，
供 MCU (MSPM0) + OLED + W25Q64 方案显示中文字符。

文件结构
--------
  tools/
    readme.txt          ← 本文件
    gen_gb2312_font.py  ← 仅生成字库 (.bin + .h)
    gen_send_font.py    ← 生成字库 + UART 写入 W25Q64 (主工具)
    send_font.bat       ← 一键发送脚本 (调用 gen_send_font.py)
    verify_font.py      ← 独立校验 W25Q64 中字库 CRC
    gb2312_hzk16.bin    ← 生成的字库二进制 (282752 字节)
    gb2312_font.h       ← 生成的 C 头文件 (嵌入式编译用)

工作流程
--------
第 1 次刷固件:
  1. Keil 编译 → 下载固件到板子
  2. 下载字库到 W25Q64:
      发送  			send_font.bat COM4
     生成并发送		python gen_send_font.py --port COM4)
  3. 脚本会自动进行 CRC 校验

后续只更新字库:
  send_font.bat COM4

只想校验 (不写入):
  python verify_font.py COM4

调试说明
--------
  - 波特率默认 115200，可用 --baud 参数调整
  - W25Q64 擦除约需 28 秒，脚本会等待
  - 如果经常校验失败，尝试降低波特率:
        send_font.bat COM4 38400
  - 校验不通过时，脚本会显示 PC CRC 和 MCU CRC 的 XOR 差

MCU 端接口
-----------
  // 接收字库
  FontWriter_ReceiveAndWrite(0);

  // 设置字库基地址
  OLED_SetFontFlashBase(0);

  // 显示中文
  OLED_ShowChineseString(x, y, (const uint8_t*)"你好");
