"""
gen_ascii_8x16_v2.py — 生成 8×16 半角 ASCII 点阵字库 (C 头文件)
方案: 用 GDI CreateFontIndirect + CreateCompatibleBitmap + GetPixel 逐个像素读取
"""

import ctypes
import ctypes.wintypes
import os
import struct

FONT_WIDTH = 8
FONT_HEIGHT = 16
BYTES_PER_CHAR = 16
FIRST_CHAR = 0x20
LAST_CHAR = 0x7E
CHAR_COUNT = LAST_CHAR - FIRST_CHAR + 1

gdi32 = ctypes.windll.gdi32
user32 = ctypes.windll.user32

def render_char(font_name, ch):
    """Render a single ASCII char to 8x16 bitmap using GDI, return 16 bytes"""
    hwnd = user32.GetDesktopWindow()
    hdc = user32.GetDC(hwnd)
    
    # Create an in-memory DC
    mem_dc = gdi32.CreateCompatibleDC(hdc)
    
    # Create a 16x16 bitmap (extra width to avoid clipping)
    bmp_width = 16
    bmp_height = 16
    hbmp = gdi32.CreateCompatibleBitmap(hdc, bmp_width, bmp_height)
    old_bmp = gdi32.SelectObject(mem_dc, hbmp)
    
    # Create font
    hfont = gdi32.CreateFontA(
        16, 8, 0, 0, 400, 0, 0, 0,
        0,          # ANSI_CHARSET
        4,          # OUT_TT_PRECIS
        0,          # CLIP_DEFAULT_PRECIS
        2,          # PROOF_QUALITY
        0,          # FIXED_PITCH
        font_name.encode('ascii')
    )
    old_font = gdi32.SelectObject(mem_dc, hfont)
    
    # White background
    rect = ctypes.wintypes.RECT(0, 0, bmp_width, bmp_height)
    brush = gdi32.CreateSolidBrush(0x00FFFFFF)
    user32.FillRect(mem_dc, ctypes.byref(rect), brush)
    gdi32.DeleteObject(brush)
    
    # Black text, left+top alignment
    gdi32.SetTextAlign(mem_dc, 0x00000001 | 0x00000000)  # TA_LEFT | TA_TOP
    gdi32.SetTextColor(mem_dc, 0x00000000)
    gdi32.SetBkColor(mem_dc, 0x00FFFFFF)
    
    # Draw
    ret = gdi32.TextOutA(mem_dc, 0, 0, bytes([ch]), 1)
    
    # Read pixels - check every pixel
    result = bytearray(16)
    for row in range(16):
        byte_val = 0
        for col in range(8):
            pixel = gdi32.GetPixel(mem_dc, col, row)
            # pixel is 0x00BBGGRR, black = 0x00000000, white = 0x00FFFFFF
            if pixel == 0:
                byte_val |= (1 << (7 - col))
        result[row] = byte_val
    
    # Cleanup
    gdi32.SelectObject(mem_dc, old_font)
    gdi32.DeleteObject(hfont)
    gdi32.SelectObject(mem_dc, old_bmp)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(mem_dc)
    user32.ReleaseDC(hwnd, hdc)
    
    return bytes(result)


def generate_font(font_name):
    buffer = bytearray(CHAR_COUNT * BYTES_PER_CHAR)
    for i, code in enumerate(range(FIRST_CHAR, LAST_CHAR + 1)):
        idx = i * BYTES_PER_CHAR
        buffer[idx:idx + BYTES_PER_CHAR] = render_char(font_name, code)
        pct = int((i + 1) / CHAR_COUNT * 100)
        bar_len = 30
        filled = bar_len * (i + 1) // CHAR_COUNT
        bar = "#" * filled + "." * (bar_len - filled)
        print(f"\r  [{bar}] {pct:>3}%  ({i+1}/{CHAR_COUNT})", end="", flush=True)
    print()
    return buffer


def write_c_array(data, path, array_name="ascii_8x16_font"):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    
    lines = [
        "/*",
        " * ascii_8x16_font.h — 自动生成, 8x16 半角 ASCII 点阵字库",
        f" * 字符: 0x{FIRST_CHAR:02X} ~ 0x{LAST_CHAR:02X} ({CHAR_COUNT} 个)",
        f" * 大小: {len(data)} 字节 ({len(data)//1024:.1f} KB)",
        " * 格式: 每个字符 16 字节 (8×16 逐行, MSB 左)",
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
        char_idx = i // 16
        ch = chr(FIRST_CHAR + char_idx)
        if ch == '\\':
            escaped = '\\\\\\\\'
        elif ch == '"':
            escaped = '\\\\\"'
        elif ch == '\'':
            escaped = "\\\\'"
        else:
            escaped = ch
        comment = f"  /* '{escaped}' 0x{FIRST_CHAR + char_idx:02X} */"
        comma = "," if i + 16 < len(data) else ""
        lines.append(f"    {hex_str}{comma}{comment}")
    
    lines.append("};")
    lines.append("")
    lines.append("#endif /* ASCII_8X16_FONT_H */")
    lines.append("")
    
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--font-name", default="Consolas")
    parser.add_argument("--out-dir", default=None)
    args = parser.parse_args()
    
    out_dir = args.out_dir or os.path.dirname(os.path.abspath(__file__))
    
    print(f"ASCII 8×16 字库生成器 (v2 — GetPixel)")
    print(f"  字体: {args.font_name}")
    print()
    
    data = generate_font(args.font_name)
    
    out_path = os.path.join(out_dir, "ascii_8x16_font.h")
    write_c_array(data, out_path)
    print(f"\n[OK] {out_path} ({len(data)} 字节, {len(data)/1024:.1f} KB)")


if __name__ == "__main__":
    main()
