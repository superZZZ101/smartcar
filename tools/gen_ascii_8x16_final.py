"""
gen_ascii_8x16_final.py — 生成 8×16 半角 ASCII 点阵字库 (C 头文件)

方案: GDI + SimSun 16px + 边界框 → 垂直居中放入 16 行单元格
"""

import ctypes
import ctypes.wintypes
import os

BYTES_PER_CHAR = 16
FIRST_CHAR = 0x20
LAST_CHAR = 0x7E
CHAR_COUNT = LAST_CHAR - FIRST_CHAR + 1

gdi32 = ctypes.windll.gdi32
user32 = ctypes.windll.user32


def render_char_pixels(ch):
    """Render ASCII char with SimSun 16px, return pixel array and bbox"""
    hwnd = user32.GetDesktopWindow()
    hdc = user32.GetDC(hwnd)
    mem_dc = gdi32.CreateCompatibleDC(hdc)

    w, h = 64, 32
    hbmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(mem_dc, hbmp)

    # White background
    gdi32.PatBlt(mem_dc, 0, 0, w, h, 0x00FFFFFF)

    # SimSun 16px, ANSI charset
    hfont = gdi32.CreateFontA(16, 8, 0, 0, 400, 0, 0, 0,
        0, 4, 0, 2, 0, b'SimSun')
    gdi32.SelectObject(mem_dc, hfont)
    gdi32.SetTextAlign(mem_dc, 0x00000001 | 0x00000000)
    gdi32.SetTextColor(mem_dc, 0x00000000)
    gdi32.SetBkColor(mem_dc, 0x00FFFFFF)
    gdi32.TextOutA(mem_dc, 0, 0, bytes([ch]), 1)

    # Read pixels
    pixels = [[0] * w for _ in range(h)]
    for row in range(h):
        for col in range(w):
            if gdi32.GetPixel(mem_dc, col, row) == 0:
                pixels[row][col] = 1

    # Find bbox
    x1, y1, x2, y2 = w, h, 0, 0
    for row in range(h):
        for col in range(w):
            if pixels[row][col]:
                x1 = min(x1, col)
                y1 = min(y1, row)
                x2 = max(x2, col)
                y2 = max(y2, row)

    gdi32.DeleteObject(hfont)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(mem_dc)
    user32.ReleaseDC(hwnd, hdc)

    if x2 < x1:
        return pixels, None  # no content
    return pixels, (x1, y1, x2, y2)


def extract_8x16(pixels, bbox):
    """Extract 8x16 font data from pixels, vertically centered in 16 rows"""
    if bbox is None:
        return bytes(16)  # all zeros for space/unprintable

    x1, y1, x2, y2 = bbox
    bw = x2 - x1 + 1
    bh = y2 - y1 + 1

    out = bytearray(16)

    # Clamp width to 8, center horizontally if narrower
    h_offset = max(0, (8 - bw) // 2)

    # Center vertically in 16 rows
    v_offset = (16 - bh) // 2

    for row in range(bh):
        src_row = y1 + row
        out_row = v_offset + row
        if out_row >= 16:
            break
        byte_val = 0
        for col in range(min(bw, 8)):
            src_col = x1 + col
            if src_col < len(pixels[0]) and pixels[src_row][src_col]:
                byte_val |= (1 << (7 - (h_offset + col)))
        out[out_row] = byte_val

    return bytes(out)


def generate_font():
    buffer = bytearray(CHAR_COUNT * BYTES_PER_CHAR)
    for i, code in enumerate(range(FIRST_CHAR, LAST_CHAR + 1)):
        idx = i * BYTES_PER_CHAR
        pixels, bbox = render_char_pixels(code)
        buf = extract_8x16(pixels, bbox)
        for j, b in enumerate(buf):
            buffer[idx + j] = b

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
        f" * 大小: {len(data)} 字节",
        " * 格式: 16 bytes/char, 8x16, MSB left, vertical center",
        " * 来源: SimSun 16px + GDI GetPixel",
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
        if ch == '\\': escaped = '\\\\'
        elif ch == '"': escaped = '\\"'
        elif ch == '\'': escaped = "\\'"
        elif ch == ' ': escaped = 'SP'
        else: escaped = ch
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
    parser.add_argument("--out-dir", default=None)
    args = parser.parse_args()

    out_dir = args.out_dir or os.path.dirname(os.path.abspath(__file__))

    print("ASCII 8×16 字库生成器 (Final — SimSun 16px + 垂直居中)")
    print()

    data = generate_font()

    out_path = os.path.join(out_dir, "ascii_8x16_font.h")
    write_c_array(data, out_path)
    print(f"\n[OK] {out_path} ({len(data)} 字节)")


if __name__ == "__main__":
    main()
