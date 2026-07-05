"""
gen_send_font.py — GB2312 16×16 字库生成 + UART 写入 W25Q64

功能:
  1. 使用 Windows GDI (宋体) 生成 GB2312 全字库 (支持符号+汉字)
  2. 通过 UART 写入 MCU → W25Q64
  3. CRC32 校验确认数据完整

用法:
  # 只生成字库文件
  python gen_send_font.py --gen-only

  # 生成并通过 COM3 写入
  python gen_send_font.py --port COM3

  # 跳过生成, 直接发送已有 .bin
  python gen_send_font.py --port COM3 --bin-only tools/gb2312_hzk16.bin

协议 (MCU 端):
  SYNC  : EB 90 EB 90  (4 字节)
  LENGTH: 4 字节小端, 字库总字节数
  ACK   : 06            (MCU 确认长度)
  WAIT  : 06            (MCU 擦除完成)
  DATA  : 256 字节/块, 每块后 MCU 回 06
  DONE  : 00            (写入完成)
  CRC   : 4 字节小端, 全字库 CRC32
  RESULT: 06=通过  /  失败=回传 MCU CRC (4 字节)

依赖:
  pip install pyserial

兼容: Windows Only (使用 GDI)
"""

import ctypes
import ctypes.wintypes
import os
import struct
import sys
import time

# ── 路径 ──
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(SCRIPT_DIR) if os.path.basename(SCRIPT_DIR) == "tools" else SCRIPT_DIR

# ── 字库参数 ──
ZONE_COUNT      = 94
BYTES_PER_CHAR  = 32   # 16×16 = 32 字节
TOTAL_BYTES     = ZONE_COUNT * ZONE_COUNT * BYTES_PER_CHAR  # 282752

# ── 通讯协议 ──
CMD_SYNC   = b'\xEB\x90\xEB\x90'
ACK        = b'\x06'
NAK        = b'\x15'
DONE       = b'\x00'
BLOCK_SIZE = 256


# ════════════════════════════════════════════════════════
# GDI 字模渲染
# ════════════════════════════════════════════════════════

class FontRenderer:
    """Windows GDI GB2312 字符 → 16×16 单色点阵"""

    def __init__(self, font_name="SimSun"):
        gdi32 = ctypes.windll.gdi32
        user32 = ctypes.windll.user32

        # 内存 DC, 不依赖桌面窗口
        self._hdc = gdi32.CreateCompatibleDC(None)
        self._hfont = gdi32.CreateFontA(
            -16, 0, 0, 0, 400, 0, 0, 0, 1, 4, 0, 2, 0,
            font_name.encode("ascii"),
        )
        gdi32.SelectObject(self._hdc, self._hfont)
        gdi32.SetTextAlign(self._hdc, 0x00000000)   # TA_LEFT | TA_TOP
        self._gdi32 = gdi32
        self._user32 = user32

    def close(self):
        if self._hfont:
            self._gdi32.DeleteObject(self._hfont)
            self._hfont = None
        if self._hdc:
            self._gdi32.DeleteDC(self._hdc)
            self._hdc = None

    def render(self, gb_bytes: bytes) -> bytes:
        """渲染 1 个 GB2312 字符 → 32 字节 HZK16 格式"""
        w, h = 32, 16

        # 用 CreateDIBSection 创建可直接访问的 DIB
        bmi_buf = (ctypes.c_ubyte * 44)()
        struct.pack_into("IiiHHIIiiII", bmi_buf, 0,
                         40, w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
        pbits = ctypes.c_void_p()
        hbmp = self._gdi32.CreateDIBSection(
            self._hdc,
            ctypes.c_void_p(ctypes.addressof(bmi_buf)),
            0, ctypes.byref(pbits), None, 0)
        old = self._gdi32.SelectObject(self._hdc, hbmp)

        # 白色背景 + 黑色文字
        rect = ctypes.wintypes.RECT(0, 0, w, h)
        brush = self._gdi32.CreateSolidBrush(0x00FFFFFF)
        self._user32.FillRect(self._hdc, ctypes.byref(rect), brush)
        self._gdi32.DeleteObject(brush)
        self._gdi32.SetTextColor(self._hdc, 0x00000000)
        self._gdi32.SetBkColor(self._hdc, 0x00FFFFFF)
        self._gdi32.TextOutA(self._hdc, 0, 0, gb_bytes, 2)

        # 直接从 DIB section 读像素 (32bpp)
        bits32 = ctypes.cast(pbits, ctypes.POINTER(
            ctypes.c_ubyte * (w * h * 4))).contents

        # 转为 1bpp HZK16 格式 (16x16, 取左 16 列)
        out = bytearray(32)
        for r in range(16):
            for c in range(2):  # 左半 + 右半
                byte_val = 0
                for bit in range(8):
                    col = c * 8 + bit
                    idx = (r * w + col) * 4
                    # 判断像素是否非白 (= 黑色文字)
                    if (bits32[idx + 2] < 128):  # R < 128 判为黑
                        byte_val |= 1 << (7 - bit)
                out[r * 2 + c] = byte_val

        self._gdi32.SelectObject(self._hdc, old)
        self._gdi32.DeleteObject(hbmp)

        return bytes(out)


# ════════════════════════════════════════════════════════
# CRC32
# ════════════════════════════════════════════════════════

def crc32(data: bytes) -> int:
    c = 0xFFFFFFFF
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ (0xEDB88320 if c & 1 else 0)
    return c ^ 0xFFFFFFFF


# ════════════════════════════════════════════════════════
# UART 通讯
# ════════════════════════════════════════════════════════

class FontWriter:
    """通过 UART 发送字库到 MCU, 写入 W25Q64"""

    def __init__(self, port: str, baud: int = 115200):
        import serial
        try:
            self._ser = serial.Serial(port, baud, timeout=5)
        except serial.SerialException as e:
            print(f"\n[错误] 无法打开串口 '{port}'")
            print(f"  原因: {e}")
            print(f"  请检查:")
            print(f"    1. 串口号是否正确 (当前: {port})")
            print(f"    2. 板子已上电且 USB 已连接")
            print(f"    3. 没有其他程序占用该串口")
            print()
            avail = [f"COM{x}" for x in range(1, 21)]
            import glob
            try:
                found = glob.glob("COM[0-9]*") or glob.glob("\\\\.\\COM[0-9]*")
                if found:
                    print(f"  可用串口: {', '.join(sorted(found))}")
            except Exception:
                pass
            sys.exit(1)
        self._port = port
        self._baud = baud

    def close(self):
        if self._ser and self._ser.is_open:
            self._ser.close()

    # ── 底层收发 ──

    def _read_ack(self, timeout=5, label="ACK") -> bool:
        self._ser.timeout = timeout
        data = self._ser.read(1)
        self._ser.timeout = 5
        if data == ACK:
            return True
        print(f"  [!] 未收到 {label}, 收到: {data.hex() if data else '(空)'}")
        return False

    # ── 协议流程 ──

    def send_font(self, data: bytes) -> bool:
        """发送字库到 MCU, 返回成功/失败"""
        total = len(data)
        blocks = (total + BLOCK_SIZE - 1) // BLOCK_SIZE

        print(f"  发送 {total} 字节 ({blocks} 块) → {self._port} @ {self._baud}")

        # 0. 清空 RX 缓冲区, 避免残留数据干扰协议
        self._ser.reset_input_buffer()

        # 1. 同步头
        self._ser.write(CMD_SYNC)
        time.sleep(0.1)

        # 2. 长度 (4 字节小端)
        self._ser.write(struct.pack("<I", total))
        if not self._read_ack(5, "长度确认"):
            return False
        print("  ~ 长度确认, 等待擦除 (~28s)...")

        # 3. 等待擦除完成 (W25Q64 擦除需要较长时间)
        if not self._read_ack(60, "擦除完成"):
            return False
        print("  ~ 擦除完成, 开始发送数据...")

        # 4. 分块发送 + 回读校验
        t0 = time.time()
        for i in range(blocks):
            chunk = data[i * BLOCK_SIZE: (i + 1) * BLOCK_SIZE]
            self._ser.write(chunk)

            # 接收 MCU 回读的 256 字节
            self._ser.timeout = 10
            readback = self._ser.read(len(chunk))
            self._ser.timeout = 5

            if len(readback) != len(chunk):
                print(f"  [FAIL] 块 {i}: 回读不完整, 期望 {len(chunk)} 字节, 收到 {len(readback)}")
                # 告诉 MCU 失败
                self._ser.write(NAK)
                return False

            # PC 端比对 — 先扫完整块再统一报告
            errors = []
            for j in range(len(chunk)):
                if chunk[j] != readback[j]:
                    abs_addr = i * BLOCK_SIZE + j
                    zone = (abs_addr // BYTES_PER_CHAR) // ZONE_COUNT
                    pos  = (abs_addr // BYTES_PER_CHAR) % ZONE_COUNT
                    char_off = abs_addr % BYTES_PER_CHAR
                    errors.append((j, chunk[j], readback[j], abs_addr, zone, pos, char_off))

            if errors:
                for j, exp, got, abs_addr, zone, pos, char_off in errors[:20]:
                    print(f"  [FAIL] 块 {i}, 字节 {j}: 期望 0x{exp:02X}, 收到 0x{got:02X}")
                    print(f"         绝对地址 0x{abs_addr:05X} (区{zone}位{pos} 字内偏移{char_off})")
                if len(errors) > 20:
                    print(f"         ... 还有 {len(errors) - 20} 个错误")
                print(f"  [FAIL] 块 {i}: 共 {len(errors)}/{len(chunk)} 字节错误")
                self._ser.write(NAK)
                return False

            # 校验通过
            self._ser.write(ACK)

            if (i + 1) % 100 == 0 or i == blocks - 1:
                sent = min((i + 1) * BLOCK_SIZE, total)
                pct  = sent * 100 // total
                elapsed = time.time() - t0
                rate = sent / elapsed / 1024 if elapsed > 0 else 0
                print(f"    进度: {i + 1}/{blocks} 块 ({sent}/{total} 字节, {pct}%, {rate:.0f} KB/s)")

        # 5. 写入完成标志
        done = self._ser.read(1)
        if done == DONE:
            print("  ~ W25Q64 写入完成!")
            return True
        else:
            print(f"  [!] 写入完成标志异常: {done.hex() if done else '(空)'}")
            return False

    def verify_crc(self, pc_crc: int) -> bool:
        """发送 PC CRC, 接收 MCU 校验结果"""
        print(f"  发送校验 CRC: 0x{pc_crc:08X}")

        self._ser.write(struct.pack("<I", pc_crc))
        self._ser.timeout = 60   # SPI 读回 282KB 需要约 45 秒
        resp = self._ser.read(1)
        self._ser.timeout = 5

        if resp == ACK:
            print("  ~ 校验通过! W25Q64 数据完整")
            return True

        # 失败: 读 MCU CRC
        mcu_raw = self._ser.read(4)
        self._ser.timeout = 5
        if len(mcu_raw) == 4:
            mcu = struct.unpack("<I", mcu_raw)[0]
            diff = pc_crc ^ mcu
            print(f"  x 校验失败")
            print(f"    PC  CRC:  0x{pc_crc:08X}")
            print(f"    MCU CRC:  0x{mcu:08X}")
            print(f"    XOR 差:   0x{diff:08X}")
        else:
            print(f"  x 校验失败 (MCU 无 CRC 回传)")
        return False


# ════════════════════════════════════════════════════════
# 字库生成 (复用 gen_gb2312_font.py 逻辑)
# ════════════════════════════════════════════════════════

def char_is_valid(zone: int, pos: int) -> bool:
    byte1, byte2 = 0xA1 + zone, 0xA1 + pos
    if not (0xA1 <= byte1 <= 0xF7 and 0xA1 <= byte2 <= 0xFE):
        return False
    return (0 <= zone <= 8) or (15 <= zone <= 86)


def generate_font_impl(font_name="SimSun", progress=None) -> bytearray:
    """生成完整字库, 返回 bytearray"""
    buf = bytearray([0xFF] * TOTAL_BYTES)
    rendered = errors = skipped = 0
    r = FontRenderer(font_name)
    try:
        for zone in range(ZONE_COUNT):
            for pos in range(ZONE_COUNT):
                idx = (zone * ZONE_COUNT + pos) * BYTES_PER_CHAR
                if not char_is_valid(zone, pos):
                    skipped += 1
                    continue
                gb = bytes([0xA1 + zone, 0xA1 + pos])
                try:
                    gb.decode("gb2312")
                except UnicodeDecodeError:
                    errors += 1
                    continue
                buf[idx: idx + 32] = r.render(gb)
                rendered += 1
            if progress:
                progress(zone, ZONE_COUNT)
    finally:
        r.close()
    return buf, rendered, skipped, errors


# ════════════════════════════════════════════════════════
# CLI
# ════════════════════════════════════════════════════════

def progress_bar(zone: int, total: int):
    bar_len = 40
    done = zone + 1
    filled = bar_len * done // total
    bar = "#" * filled + "." * (bar_len - filled)
    pct = done * 100 // total
    print(f"\r  生成字库: [{bar}] {pct:>3}% ({done}/{total} 区)", end="", flush=True)


def main():
    import argparse

    parser = argparse.ArgumentParser(description="GB2312 字库生成 + UART 写入工具")
    parser.add_argument("--port", help="串口号, 如 COM3")
    parser.add_argument("--baud", type=int, default=115200, help="波特率 (默认 115200)")
    parser.add_argument("--bin-path", default=os.path.join(SCRIPT_DIR, "gb2312_hzk16.bin"),
                        help="输出 .bin 路径")
    parser.add_argument("--h-path", default=os.path.join(SCRIPT_DIR, "gb2312_font.h"),
                        help="输出 .h 头文件路径")
    parser.add_argument("--font-name", default="SimSun", help="字体名称 (默认 SimSun)")
    parser.add_argument("--gen-only", action="store_true", help="仅生成字库, 不发送")
    parser.add_argument("--bin-only", type=str, metavar="FILE",
                        help="跳过生成, 直接发送指定 .bin 文件")
    parser.add_argument("--flash-addr", type=int, default=0,
                        help="W25Q64 字库起始地址 (默认 0)")
    args = parser.parse_args()

    # ── 步骤 1: 装载/生成字库 ──

    if args.bin_only:
        path = args.bin_only
        if not os.path.exists(path):
            print(f"错误: {path} 不存在")
            sys.exit(1)
        with open(path, "rb") as f:
            font_data = f.read()
        print(f"已加载字库: {path}  ({len(font_data)} 字节)")
    else:
        print(f"正在生成 GB2312 16×16 字库 (字体: {args.font_name})...")
        font_data, rendered, skipped, err = generate_font_impl(args.font_name, progress_bar)
        print()
        print(f"  已渲染: {rendered}  跳过: {skipped}  未定义: {err}")
        print()

        # 写 .bin
        os.makedirs(os.path.dirname(args.bin_path) or ".", exist_ok=True)
        with open(args.bin_path, "wb") as f:
            f.write(font_data)
        sz = os.path.getsize(args.bin_path)
        print(f"  [OK] {args.bin_path}  ({sz} 字节, {sz / 1024:.1f} KB)")

        # 写 .h (extern 声明)
        os.makedirs(os.path.dirname(args.h_path) or ".", exist_ok=True)
        with open(args.h_path, "w", encoding="utf-8") as f:
            f.write(f"/* 自动生成: GB2312 16×16 字库 */\n")
            f.write(f"/* 来源: {os.path.basename(args.bin_path)} ({len(font_data)} 字节) */\n")
            f.write(f"#ifndef GB2312_FONT_H\n#define GB2312_FONT_H\n\n")
            f.write(f"#define GB2312_FONT_SIZE       {len(font_data)}\n")
            f.write(f"#define GB2312_ZONE_COUNT      {ZONE_COUNT}\n")
            f.write(f"#define GB2312_BYTES_PER_CHAR  {BYTES_PER_CHAR}\n\n")
            f.write(f"extern const unsigned char gb2312_hzk16[{len(font_data)}];\n")
            f.write(f"\n#endif\n")
        print(f"  [OK] {args.h_path}")

    # ── 步骤 2: 发送 ──

    if args.gen_only or not args.port:
        print()
        print("提示: 添加 --port COMx 可将字库写入 W25Q64")
        print(f"  MCU 端调用 FontWriter_ReceiveAndWrite({args.flash_addr})")
        return

    print(f"\n通过 UART 写入 W25Q64 (地址 {args.flash_addr})...")
    writer = FontWriter(args.port, args.baud)
    try:
        ok = writer.send_font(font_data)
        if not ok:
            sys.exit(1)

        pc_crc = crc32(font_data)
        writer.verify_crc(pc_crc)

        print(f"\n完成! 字库已写入 W25Q64 @ 0x{args.flash_addr:X}")
        print("代码示例:")
        print(f'  OLED_SetFontFlashBase({args.flash_addr});')
        print('  OLED_ShowChineseString(0, 0, (const uint8_t*)"设置");')
    finally:
        writer.close()


if __name__ == "__main__":
    main()
