"""
gen_ascii_8x16_font.py — 生成 8×16 半角 ASCII 点阵字库 C 头文件

使用 Windows GDI 渲染 ASCII 0x20-0x7E，输出为 C 数组 (8×16 = 16 字节/字符)
可直接嵌入 oled.c

用法:
  python gen_ascii_8x16_font.py                    # 输出到 tools/
  python gen_ascii_8x16_font.py --font-name Consolas  # 指定字体
  python gen_ascii_8x16_font.py --out-dir ../keil/Core/Inc  # 指定输出目录
"""

import ctypes
import ctypes.wintypes
import os
import struct
import argparse

FONT_WIDTH = 8
FONT_HEIGHT = 16
BYTES_PER_CHAR = 16  # 8×16 = 16 字节
FIRST_CHAR = 0x20
LAST_CHAR = 0x7E
CHAR_COUNT = LAST_CHAR - FIRST_CHAR + 1  # 95

DEFAULT_CHARSET = 0  # ANSI_CHARSET for ASCII
FW_NORMAL = 400
OUT_TT_PRECIS = 4
CLIP_DEFAULT_PRECIS = 0
PROOF_QUALITY = 2
DEFAULT_PITCH = 0
FF_DONTCARE = 0


class GdiAsciiRenderer:
    def __init__(self, font_name="Consolas"):
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
        self.gdi32.SetTextAlign(self.hdc, 0x00000001 | 0x00000006)

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

    def render(self, ch: int) -> bytes:
        """渲染单个 ASCII 字符，返回 16 字节 (8×16)"""
        width = FONT_WIDTH * 2  # 留宽避免剪裁
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
        self.gdi32.TextOutA(self.hdc, 0, 0, chr(ch).encode("ascii"), 1)

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

        # 转为 8×16: 取左 8 列，16 行 × 1 字节
        font_data = bytearray(16)
        for r in range(16):
            byte_val = 0
            for bit in range(8):
                src_col = bit
                src_byte = r * row_bytes + src_col // 8
                src_mask = 1 << (7 - (src_col % 8))
                if src_byte < len(bmp_bits) and (bmp_bits[src_byte] & src_mask):
                    byte_val |= 1 << (7 - bit)
            font_data[r] = byte_val
        return bytes(font_data)


def generate_font(font_name="Consolas", progress_cb=None) -> bytearray:
    """生成 ASCII 8×16 字库 (95 字符 × 16 字节 = 1520 字节)"""
    buffer = bytearray(CHAR_COUNT * BYTES_PER_CHAR)

    with GdiAsciiRenderer(font_name) as renderer:
        for i, code in enumerate(range(FIRST_CHAR, LAST_CHAR + 1)):
            idx = i * BYTES_PER_CHAR
            buffer[idx:idx + BYTES_PER_CHAR] = renderer.render(code)
            if progress_cb:
                progress_cb(i, CHAR_COUNT)

    return buffer


def write_c_array(data: bytes, path: str, array_name="ascii_8x16_font"):
    """将字库写出为 C 头文件"""
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)

    lines = [
        "/*",
        " * ascii_8x16_font.h — 自动生成, 8x16 半角 ASCII 点阵字库",
        f" * 字符: 0x{FIRST_CHAR:02X} ~ 0x{LAST_CHAR:02X} ({CHAR_COUNT} 个)",
        f" * 大小: {len(data)} 字节 ({len(data)//1024:.1f} KB)",
        f" * 格式: 每个字符 {BYTES_PER_CHAR} 字节 (8×16 逐行, MSB 左)",
        " * 字体: 使用 Windows GDI 渲染",
        " */",
        "#ifndef ASCII_8X16_FONT_H",
        "#define ASCII_8X16_FONT_H",
        "",
        "#include <stdint.h>",
        "",
        f"#define ASCII_8X16_FIRST_CHAR   0x{FIRST_CHAR:02X}",
        f"#define ASCII_8X16_LAST_CHAR    0x{LAST_CHAR:02X}",
        f"#define ASCII_8X16_CHAR_COUNT   {CHAR_COUNT}",
        f"#define ASCII_8X16_BYTES_PER_CHAR {BYTES_PER_CHAR}",
        "",
        f"static const uint8_t {array_name}[{len(data)}] = {{",
    ]

    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
        # 每 20 个字符加个注释
        char_idx = i // 16
        if char_idx < CHAR_COUNT:
            ch = chr(FIRST_CHAR + char_idx)
            if ch == '\\':
                ch_escaped = '\\\\'
            elif ch == '"':
                ch_escaped = '\\"'
            elif ch == '\'':
                ch_escaped = "\\'"
            elif ch == '\x20':
                ch_escaped = 'SP'
            elif ch == '\x7f':
                ch_escaped = 'DEL'
            else:
                ch_escaped = ch
            comment = f"  /* '{ch_escaped}' 0x{FIRST_CHAR + char_idx:02X} */"
        else:
            comment = ""
        comma = "," if i + 16 < len(data) else ""
        lines.append(f"    {hex_str}{comma}{comment}")

    lines.append("};")
    lines.append("")
    lines.append("#endif /* ASCII_8X16_FONT_H */")
    lines.append("")

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description="8×16 半角 ASCII 点阵字库生成器")
    parser.add_argument("--font-name", default="Consolas",
                        help="字体名称 (默认 Consolas, 推荐 Consolas/Lucida Console/Courier New)")
    parser.add_argument("--out-dir", default=None,
                        help="输出目录 (默认当前目录)")

    args = parser.parse_args()

    if args.out_dir:
        out_dir = args.out_dir
    else:
        out_dir = os.path.dirname(os.path.abspath(__file__))

    print(f"ASCII 8×16 字库生成器")
    print(f"  字体: {args.font_name}")
    print(f"  字符: {CHAR_COUNT} 个 (0x{FIRST_CHAR:02X}~0x{LAST_CHAR:02X})")
    print()

    def progress(i, total):
        if i % 10 == 0 or i == total - 1:
            bar_len = 30
            filled = bar_len * (i + 1) // total
            bar = "#" * filled + "." * (bar_len - filled)
            pct = int((i + 1) / total * 100)
            print(f"\r  渲染: [{bar}] {pct:>3}%  ({i+1}/{total})", end="", flush=True)

    data = generate_font(args.font_name, progress)
    print()

    out_path = os.path.join(out_dir, "ascii_8x16_font.h")
    write_c_array(data, out_path)
    size_kb = len(data) / 1024
    print(f"\n  [OK] {out_path}  ({len(data)} 字节, {size_kb:.1f} KB)")
    print(f"  提示: 在 oled.c 中 #include 此文件并使用 ascii_8x16_font[] 字库")


if __name__ == "__main__":
    main()
