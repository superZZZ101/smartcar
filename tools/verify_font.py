"""
verify_font.py — 独立校验 W25Q64 中字库数据的 CRC32

不下载、不写入，只让 MCU 读 Flash 算 CRC 发回来比对。

协议:
  PC → MCU: 'V' (1B) + addr (4B LE) + length (4B LE)
  MCU → PC: crc32 (4B LE)

依赖:
  pip install pyserial

用法:
  python verify_font.py COM4                         # 默认
  python verify_font.py COM4 --baud 38400             # 降低波特率
  python verify_font.py COM4 --bin custom_font.bin    # 指定 PC 端参考文件
  python verify_font.py COM4 --bin hzk16.bin --addr 0x1000 --len 65536
"""

import os
import struct
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# ──────────────────────────────────────────────────────
# CRC32 (和 MCU 端一致: STM32 软算或硬件 CRC)
# ──────────────────────────────────────────────────────

def crc32(data: bytes) -> int:
    c = 0xFFFFFFFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ (0xEDB88320 if c & 1 else 0)
    return c ^ 0xFFFFFFFF


# ──────────────────────────────────────────────────────
# 校验主逻辑
# ──────────────────────────────────────────────────────

def verify(port: str, baud: int, bin_path: str, flash_addr: int, length: int) -> bool:
    """连接 MCU, 让 MCU 读 W25Q64 算 CRC, PC 比对"""

    # 1. 加载 PC 端 .bin 并计算 CRC
    if not os.path.exists(bin_path):
        print(f"错误: {bin_path} 不存在")
        sys.exit(1)

    with open(bin_path, "rb") as f:
        pc_data = f.read()

    if length == 0:
        length = len(pc_data)

    pc_crc = crc32(pc_data[:length])
    print(f"字库文件: {bin_path}")
    print(f"  大小:   {len(pc_data)} 字节")
    print(f"  校验:   0x{flash_addr:08X} ~ 0x{flash_addr + length:08X}  ({length} 字节)")
    print(f"  PC CRC: 0x{pc_crc:08X}")
    print()

    # 2. 连接串口
    try:
        import serial
    except ImportError:
        print("错误: 需要 pyserial, 执行: pip install pyserial")
        sys.exit(1)

    try:
        ser = serial.Serial(port, baud, timeout=15)
    except serial.SerialException as e:
        print(f"\n[错误] 无法打开串口 '{port}'")
        print(f"  原因: {e}")
        print(f"  请检查串口号和连接")
        sys.exit(1)
    print(f"串口 {port} 已打开 ({baud} 8N1)")

    try:
        # 3. 发送校验命令
        print("发送校验命令...")
        ser.write(b'V')
        ser.write(struct.pack("<I", flash_addr))
        ser.write(struct.pack("<I", length))

        # 4. 读 MCU CRC (读回 282KB 约 45 秒)
        ser.timeout = 60
        raw = ser.read(4)
        ser.timeout = 5

        if len(raw) != 4:
            print(f"! MCU 无响应, 收到 {len(raw)} 字节")
            return False

        mcu_crc = struct.unpack("<I", raw)[0]
        diff = pc_crc ^ mcu_crc

        print()
        print("=" * 44)
        print("         校 验 结 果")
        print("=" * 44)
        print(f"  PC  CRC:  0x{pc_crc:08X}")
        print(f"  MCU CRC:  0x{mcu_crc:08X}")
        print(f"  XOR 差:   0x{diff:08X}")

        if mcu_crc == pc_crc:
            print()
            print("  [OK] 校验通过! W25Q64 数据完整")
            return True

        print()
        print("  [FAIL] 校验失败! 数据不一致")
        if diff & 0xFFFF0000 == 0:
            print("  → 差异集中在低 16 位, 可能是文件头部偏移了少量字节")
        elif diff & 0x0000FFFF == 0:
            print("  → 差异集中在高 16 位, 可能是长度或大段数据不对")
        print()
        print("  💡 建议: 降低波特率 (--baud 38400) 或检查 SPI 时序")
        return False

    finally:
        ser.close()


# ──────────────────────────────────────────────────────
# CLI
# ──────────────────────────────────────────────────────

def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="独立校验 W25Q64 中字库数据的 CRC",
    )
    parser.add_argument("port", help="串口号, 如 COM3")
    parser.add_argument("--baud", type=int, default=115200, help="波特率 (默认 115200)")
    parser.add_argument("--bin", default=None, help="字库 .bin 路径 (默认 tools/gb2312_hzk16.bin)")
    parser.add_argument("--addr", type=int, default=0, help="W25Q64 起始地址 (默认 0)")
    parser.add_argument("--len", type=int, default=0, help="校验长度 (默认自动从 .bin 读取)")

    args = parser.parse_args()

    if args.bin is None:
        args.bin = os.path.join(SCRIPT_DIR, "gb2312_hzk16.bin")

    ok = verify(args.port, args.baud, args.bin, args.addr, args.len)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
