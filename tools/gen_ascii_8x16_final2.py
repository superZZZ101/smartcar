"""
gen_ascii_8x16_final.py — 生成 8×16 半角 ASCII 点阵字库 (C 头文件)
方案: GDI + SimSun 16px + 全新 DC/char + 垂直居中
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

def render_char(ch):
    """Render one ASCII char, return 16-byte 8x16 bitmap (fresh DC each call)"""
    hwnd = user32.GetDesktopWindow()
    screen_dc = user32.GetDC(hwnd)
    mem_dc = gdi32.CreateCompatibleDC(screen_dc)

    w, h = 16, 32
    hbmp = gdi32.CreateCompatibleBitmap(screen_dc, w, h)
    old_bmp = gdi32.SelectObject(mem_dc, hbmp)

    # White background
    rect = ctypes.wintypes.RECT(0, 0, w, h)
    brush = gdi32.CreateSolidBrush(0x00FFFFFF)
    user32.FillRect(mem_dc, ctypes.byref(rect), brush)
    gdi32.DeleteObject(brush)

    # Font: SimSun 16px, ANSI charset
    hfont = gdi32.CreateFontA(16, 8, 0, 0, 400, 0, 0, 0,
        0, 4, 0, 2, 0, b'SimSun')
    old_font = gdi32.SelectObject(mem_dc, hfont)
    gdi32.SetTextAlign(mem_dc, 0x00000001 | 0x00000000)
    gdi32.SetTextColor(mem_dc, 0x00000000)
    gdi32.SetBkColor(mem_dc, 0x00FFFFFF)

    gdi32.TextOutA(mem_dc, 0, 0, bytes([ch]), 1)

    # Read pixels: scan all 32 rows x 8 columns
    rows = [0] * h
    y_min, y_max = h, 0
    for row in range(h):
        byte_val = 0
        for col in range(8):
            if gdi32.GetPixel(mem_dc, col, row) == 0:
                byte_val |= (1 << (7 - col))
                if row < y_min: y_min = row
                if row > y_max: y_max = row
        rows[row] = byte_val

    # Cleanup: ALWAYS delete in reverse order of creation
    gdi32.SelectObject(mem_dc, old_font)
    gdi32.DeleteObject(hfont)
    gdi32.SelectObject(mem_dc, old_bmp)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(mem_dc)
    user32.ReleaseDC(hwnd, screen_dc)

    # If no content, return zeros
    if y_max < y_min:
        return bytes(16)

    content_h = y_max - y_min + 1
    v_offset = max(0, (16 - content_h) // 2)

    out = bytearray(16)
    for i in range(content_h):
        out_row = v_offset + i
        if out_row >= 16:
            break
        out[out_row] = rows[y_min + i]

    return bytes(out)


def generate_font():
    buffer = bytearray(CHAR_COUNT * BYTES_PER_CHAR)
    for i, code in enumerate(range(FIRST_CHAR, LAST_CHAR + 1)):
        idx = i * BYTES_PER_CHAR
        buf = render_char(code)
        for j, b in enumerate(buf):
            buffer[idx + j] = b

        pct = int((i + 1) / CHAR_COUNT * 100)
        bar_len = 30
        filled = bar_len * (i + 1) // CHAR_COUNT
        bar = "#" * filled + "." * (bar_len - filled)
        print(f"\r  [{bar}] {pct:>3}%  ({i+1}/{CHAR_COUNT})", end="", flush=True)
    print()
    return buffer


def write_c_array(data, path):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)

    lines = [
        "/*",
        " * ascii_8x16_font.h — 自动生成, 8x16 半角 ASCII 点阵字库",
        f" * 字符: 0x{FIRST_CHAR:02X} ~ 0x{LAST_CHAR:02X} ({CHAR_COUNT} 个)",
        f" * 大小: {len(data)} 字节",
        " * 格式: 16 bytes/char, 8x16, MSB left, vertically centered",
        " */",
        "#ifndef ASCII_8X16_FONT_H",
        "#define ASCII_8X16_FONT_H",
        "#include <stdint.h>",
        f"#define ASCII_8X16_FIRST_CHAR   0x{FIRST_CHAR:02X}",
        f"#define ASCII_8X16_LAST_CHAR    0x{LAST_CHAR:02X}",
        f"#define ASCII_8X16_CHAR_COUNT   {CHAR_COUNT}",
        f"#define ASCII_8X16_BYTES_PER_CHAR {BYTES_PER_CHAR}",
        f"static const uint8_t ascii_8x16_font[{len(data)}] = {{",
    ]

    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        hex_str = ", ".join(f"0x{b:02X}" for b in chunk)
        char_idx = i // 16
        ch = chr(FIRST_CHAR + char_idx)
        if ch == '\\': escaped = '\\\\\\\\'
        elif ch == '"': escaped = '\\\\"'
        elif ch == '\'': escaped = "\\\\'"
        elif ch == ' ': escaped = 'SP'
        else: escaped = ch
        comment = f"  /* 0x{FIRST_CHAR + char_idx:02X} '{escaped}' */"
        comma = "," if i + 16 < len(data) else ""
        lines.append(f"    {hex_str}{comma}{comment}")

    lines.append("};")
    lines.append("#endif /* ASCII_8X16_FONT_H */")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", default=None)
    args = parser.parse_args()
    out_dir = args.out_dir or os.path.dirname(os.path.abspath(__file__))

    print("ASCII 8×16 字库生成器 (Final — SimSun + Fresh DC/char + 垂直居中)")
    print()
    data = generate_font()
    out_path = os.path.join(out_dir, "ascii_8x16_font.h")
    write_c_array(data, out_path)
    print(f"\n[OK] {out_path} ({len(data)} 字节)")


if __name__ == "__main__":
    main()
