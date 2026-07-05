"""
gen_gb2312_font.py — 生成 GB2312 16×16 点阵字库文件 (.bin + .h)

使用 Windows GDI (宋体) 渲染，无需第三方库。
输出符合 HZK16 格式：94 区 × 94 位 × 32 字节/字符 = 282752 字节。

用法:
  python gen_gb2312_font.py                          # 生成到 tools/
  python gen_gb2312_font.py --out-dir ./output       # 指定输出目录
  python gen_gb2312_font.py --font-name SimHei       # 指定字体 (默认 SimSun)

注意:
  需要写入 W25Q64 的，请用 gen_send_font.py (生成+发送) 或 send_font.bat
"""

import ctypes
import ctypes.wintypes
import os
import struct
import sys

# ──────────────────────────────────────────────────────
# 常量
# ──────────────────────────────────────────────────────
FONT_WIDTH      = 16
FONT_HEIGHT     = 16
ZONE_COUNT      = 94   # GB2312 每区 94 个码位
BYTES_PER_CHAR  = 32   # 16×16 = 32 字节
TOTAL_BYTES     = ZONE_COUNT * ZONE_COUNT * BYTES_PER_CHAR  # 282752

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# GDI 常量
DEFAULT_CHARSET   = 1   # GB2312_CHARSET
FW_NORMAL         = 400
OUT_TT_PRECIS     = 4
CLIP_DEFAULT_PRECIS = 0
PROOF_QUALITY     = 2
DEFAULT_PITCH     = 0
FF_DONTCARE       = 0


# ──────────────────────────────────────────────────────
# GDI 字模渲染器
# ──────────────────────────────────────────────────────

class GdiFontRenderer:
    """封装 Windows GDI 实现 GB2312 字符 → 16×16 点阵"""

    def __init__(self, font_name="SimSun"):
        self.font_name = font_name
        self.gdi32 = ctypes.windll.gdi32
        self.user32 = ctypes.windll.user32
        self._open()

    def _open(self):
        hwnd = self.user32.GetDesktopWindow()
        self.hdc = self.user32.GetDC(hwnd)
        self.hfont = self.gdi32.CreateFontA(
            -FONT_HEIGHT, 0, 0, 0, FW_NORMAL,
            0, 0, 0, DEFAULT_CHARSET,
            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            self.font_name.encode("ascii"),
        )
        if not self.hfont:
            raise RuntimeError(f"无法创建字体 '{self.font_name}'")
        self.gdi32.SelectObject(self.hdc, self.hfont)
        self.gdi32.SetTextAlign(self.hdc, 0x00000001 | 0x00000006)  # TA_LEFT | TA_BASELINE

    def close(self):
        if self.hfont:
            self.gdi32.DeleteObject(self.hfont)
            self.hfont = None
        if self.hdc:
            self.user32.ReleaseDC(0, self.hdc)
            self.hdc = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def render(self, gb_bytes: bytes) -> bytes:
        """
        渲染单个 GB2312 字符 (2 字节)，返回 32 字节 HZK16 格式。
        每字符 16 行 × 2 字节，高位在左。
        """
        width  = FONT_WIDTH * 2   # 留宽避免裁剪
        height = FONT_HEIGHT

        hbmp = self.gdi32.CreateBitmap(width, height, 1, 1, None)
        old_bmp = self.gdi32.SelectObject(self.hdc, hbmp)

        # 白色背景
        rect = ctypes.wintypes.RECT(0, 0, width, height)
        brush = self.gdi32.CreateSolidBrush(0x00FFFFFF)
        self.user32.FillRect(self.hdc, ctypes.byref(rect), brush)
        self.gdi32.DeleteObject(brush)

        # 黑色文字
        self.gdi32.SetTextColor(self.hdc, 0x00000000)
        self.gdi32.SetBkColor(self.hdc, 0x00FFFFFF)
        self.gdi32.TextOutA(self.hdc, 0, 0, gb_bytes, 2)

        # 读像素
        row_bytes = (width + 7) // 8
        bmp_bits = (ctypes.c_ubyte * (row_bytes * height))()

        bmi = (ctypes.c_ubyte * 44)()
        struct.pack_into("IiiHHIIiiII", bmi, 0,
                         40, width, -height, 1, 1, 0, 0, 0, 0, 0, 0)

        self.gdi32.GetDIBits(
            self.hdc, hbmp, 0, height,
            ctypes.c_void_p(ctypes.addressof(bmp_bits)),
            ctypes.c_void_p(ctypes.addressof(bmi)), 0,
        )

        self.gdi32.SelectObject(self.hdc, old_bmp)
        self.gdi32.DeleteObject(hbmp)

        # 转为 HZK16 格式：取左 16 列，16 行 × 2 字节
        font_data = bytearray(32)
        for r in range(16):
            for c in range(2):
                byte_val = 0
                for bit in range(8):
                    src_col = c * 8 + bit
                    src_byte = r * row_bytes + src_col // 8
                    src_mask = 1 << (7 - (src_col % 8))
                    if src_byte < len(bmp_bits) and (bmp_bits[src_byte] & src_mask):
                        byte_val |= 1 << (7 - bit)
                font_data[r * 2 + c] = byte_val
        return bytes(font_data)


# ──────────────────────────────────────────────────────
# 字库生成
# ──────────────────────────────────────────────────────

def char_is_valid_gb2312(zone: int, pos: int) -> bool:
    """判断 GB2312 区/位编码是否有效"""
    byte1 = 0xA1 + zone
    byte2 = 0xA1 + pos
    if not (0xA1 <= byte1 <= 0xF7 and 0xA1 <= byte2 <= 0xFE):
        return False
    # 符号区: 01-09 (zone 0-8), 汉字区: 16-87 (zone 15-86)
    return (0 <= zone <= 8) or (15 <= zone <= 86)


def generate_font(font_name="SimSun", progress_cb=None) -> bytearray:
    """生成完整 GB2312 16×16 字库，返回 282752 字节"""
    buffer = bytearray([0xFF] * TOTAL_BYTES)
    rendered = errors = skipped = 0

    with GdiFontRenderer(font_name) as renderer:
        for zone in range(ZONE_COUNT):
            for pos in range(ZONE_COUNT):
                idx = (zone * ZONE_COUNT + pos) * BYTES_PER_CHAR

                if not char_is_valid_gb2312(zone, pos):
                    skipped += 1
                    continue

                gb_bytes = bytes([0xA1 + zone, 0xA1 + pos])
                try:
                    gb_bytes.decode("gb2312")
                except UnicodeDecodeError:
                    errors += 1
                    continue

                buffer[idx:idx + 32] = renderer.render(gb_bytes)
                rendered += 1

            if progress_cb:
                progress_cb(zone, ZONE_COUNT)

    return buffer, rendered, skipped, errors


# ──────────────────────────────────────────────────────
# 文件输出
# ──────────────────────────────────────────────────────

def write_bin(data: bytes, path: str):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    return os.path.getsize(path)


def write_header(data: bytes, path: str, array_name="gb2312_hzk16"):
    """将 .bin 转为 C 头文件 (支持 Keil C51 __at 定位方式)"""
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)

    lines = [
        "/*",
        " * gb2312_font.h — 自动生成, 请勿手动编辑",
        f" * 大小: {len(data)} 字节",
        " * 格式: GB2312 HZK16 (94区 × 94位 × 32字节/字符)",
        " */",
        "#ifndef GB2312_FONT_H",
        "#define GB2312_FONT_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define GB2312_FONT_SIZE       {len(data)}",
        f"#define GB2312_ZONE_COUNT      {ZONE_COUNT}",
        f"#define GB2312_BYTES_PER_CHAR  {BYTES_PER_CHAR}",
        "",
        "/* 用于 __at(0x8000) 定位到 Flash 的数组 */",
        "",
    ]

    if len(data) <= 4096:
        # 小字库: 直接内联
        lines.append(f"const uint8_t {array_name}[{len(data)}] = {{")
        for i in range(0, len(data), 16):
            chunk = data[i:i + 16]
            hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
            comma = "," if i + 16 < len(data) else ""
            lines.append(f"    {hex_str}{comma}")
        lines.append("};")
    else:
        # 大字库: 生成数组不上头文件，只放 extern 声明
        lines.append(f"extern const uint8_t {array_name}[{len(data)}];")
        lines.append("")
        lines.append("/* 提示: 完整字库太大 (282KB), 建议用 .bin 刷到外部 Flash (W25Q64) */")
        lines.append("/* 此头文件用于 extern 声明, 实际 .bin 通过 tools 写入 */")

    lines.append("")
    lines.append(f"#endif /* GB2312_FONT_H */")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


# ──────────────────────────────────────────────────────
# CLI
# ──────────────────────────────────────────────────────

def print_progress(zone: int, total: int):
    bar_len = 40
    done = (zone + 1)
    filled = bar_len * done // total
    bar   = "#" * filled + "." * (bar_len - filled)
    pct   = int((zone + 1) / total * 100)
    print(f"\r  生成字库: [{bar}] {pct:>3}%  ({zone + 1}/{total} 区)", end="", flush=True)


def main():
    import argparse

    parser = argparse.ArgumentParser(description="GB2312 16×16 点阵字库生成器")
    parser.add_argument("--out-dir", default=SCRIPT_DIR,
                        help="输出目录 (默认 tools/)")
    parser.add_argument("--font-name", default="SimSun",
                        help="字体名称 (默认 SimSun 宋体)")
    parser.add_argument("--no-h", action="store_true",
                        help="不生成 C 头文件")

    args = parser.parse_args()

    print(f"GB2312 字库生成器")
    print(f"  字体: {args.font_name}  大小: {FONT_WIDTH}×{FONT_HEIGHT}")
    print(f"  总字符: {ZONE_COUNT} 区 × {ZONE_COUNT} 位 = {ZONE_COUNT * ZONE_COUNT}")
    print()

    buffer, rendered, skipped, errors = generate_font(args.font_name, print_progress)
    print()

    # 写 .bin
    bin_path = os.path.join(args.out_dir, "gb2312_hzk16.bin")
    size = write_bin(buffer, bin_path)
    print(f"\n  [OK] .bin: {bin_path}  ({size} 字节, {size / 1024:.1f} KB)")

    # 写 .h
    if not args.no_h:
        h_path = os.path.join(args.out_dir, "gb2312_font.h")
        write_header(buffer, h_path)
        h_size = os.path.getsize(h_path)
        print(f"  [OK] .h:   {h_path}  ({h_size} 字节)")

    print()
    print(f"  已渲染: {rendered}  跳过: {skipped}  未定义: {errors}")
    print()
    print("  提示: 将此 .bin 写入 W25Q64 后, 即可调用 OLED_ShowGB2312() 显示中文。")
    print("  如需同时写入 MCU, 请用: python gen_send_font.py --port COMx")


if __name__ == "__main__":
    main()
