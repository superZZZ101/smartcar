"""
gen_ascii_8x16_v3.py — 生成 8×16 半角 ASCII 点阵字库 (C 头文件)
方案: GDI + SimSun + 扫描实际边界框
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


def get_text_bmp(ch_str, charset=0, font_name='SimSun'):
    """渲染文本到内存位图并返回所有像素数据"""
    hwnd = user32.GetDesktopWindow()
    hdc = user32.GetDC(hwnd)
    mem_dc = gdi32.CreateCompatibleDC(hdc)

    w, h = 64, 48
    hbmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(mem_dc, hbmp)

    # White bg
    rect = ctypes.wintypes.RECT(0, 0, w, h)
    brush = gdi32.CreateSolidBrush(0x00FFFFFF)
    user32.FillRect(mem_dc, ctypes.byref(rect), brush)
    gdi32.DeleteObject(brush)

    # Font
    hfont = gdi32.CreateFontA(20, 10, 0, 0, 400, 0, 0, 0,
        charset, 4, 0, 2, 0, font_name.encode('ascii'))
    gdi32.SelectObject(mem_dc, hfont)
    gdi32.SetTextAlign(mem_dc, 0x00000001 | 0x00000000)
    gdi32.SetTextColor(mem_dc, 0x00000000)
    gdi32.SetBkColor(mem_dc, 0x00FFFFFF)
    gdi32.TextOutA(mem_dc, 0, 0, ch_str, len(ch_str))

    # Read pixels
    pixels = []
    for row in range(h):
        row_data = []
        for col in range(w):
            row_data.append(1 if gdi32.GetPixel(mem_dc, col, row) == 0 else 0)
        pixels.append(row_data)

    gdi32.SelectObject(mem_dc, hfont)
    gdi32.DeleteObject(hfont)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(mem_dc)
    user32.ReleaseDC(hwnd, hdc)

    return pixels


def find_bbox(pixels):
    """Find bounding box of black pixels, returns (x1, y1, x2, y2) or None"""
    h = len(pixels)
    w = len(pixels[0]) if h > 0 else 0
    x1, y1, x2, y2 = w, h, 0, 0
    for y in range(h):
        for x in range(w):
            if pixels[y][x]:
                x1 = min(x1, x)
                y1 = min(y1, y)
                x2 = max(x2, x)
                y2 = max(y2, y)
    if x2 < x1 or y2 < y1:
        return None
    return (x1, y1, x2, y2)


def downsample_bbox_to_8x16(pixels, bbox):
    """
    Down-sample the content in the bounding box to an 8x16 grid.
    Use simple averaging (nearest neighbor).
    """
    x1, y1, x2, y2 = bbox
    bw = x2 - x1 + 1  # bounding box width
    bh = y2 - y1 + 1  # bounding box height

    result = bytearray(16)  # 16 rows, 1 byte per row

    for row in range(16):
        byte_val = 0
        for col in range(8):
            # Map 8x16 position to source coordinates
            src_x = x1 + int(col * bw / 8 + 0.5)
            src_y = y1 + int(row * bh / 16 + 0.5)
            src_x = min(src_x, len(pixels[0]) - 1)
            src_y = min(src_y, len(pixels) - 1)

            if pixels[src_y][src_x]:
                byte_val |= (1 << (7 - col))
        result[row] = byte_val

    return bytes(result)


def render_char(ch):
    """Render single ASCII char, return 16 bytes (8x16 font)"""
    ch_str = bytes([ch])

    # Use DEFAULT_CHARSET=0 for ASCII, SimSun font
    pixels = get_text_bmp(ch_str, charset=0, font_name='SimSun')
    bbox = find_bbox(pixels)

    if bbox is None:
        # Empty char (space or no content)
        return bytes(16)

    return downsample_bbox_to_8x16(pixels, bbox)


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


def write_c_array(data, path, array_name="ascii_8x16_font"):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)

    lines = [
        "/*",
        " * ascii_8x16_font.h — 自动生成, 8x16 半角 ASCII 点阵字库",
        f" * 字符: 0x{FIRST_CHAR:02X} ~ 0x{LAST_CHAR:02X} ({CHAR_COUNT} 个)",
        f" * 大小: {len(data)} 字节 ({len(data)//1024:.1f} KB)",
        " * 格式: 每个字符 16 字节 (8×16 逐行, MSB 左)",
        " * 来源: SimSun (宋体) + GDI 降采样",
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
        if ch == '\\': escaped = '\\\\\\\\'
        elif ch == '"': escaped = '\\\\\"'
        elif ch == '\'': escaped = "\\\\'"
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

    print(f"ASCII 8×16 字库生成器 (v3 — SimSun + 边界框)")
    print()

    data = generate_font()

    out_path = os.path.join(out_dir, "ascii_8x16_font.h")
    write_c_array(data, out_path)
    print(f"\n[OK] {out_path} ({len(data)} 字节, {len(data)/1024:.1f} KB)")


if __name__ == "__main__":
    main()
