#!/usr/bin/env python3
"""Debug font rendering differences"""
import ctypes
import ctypes.wintypes

gdi32 = ctypes.windll.gdi32
user32 = ctypes.windll.user32

hwnd = user32.GetDesktopWindow()
sd = user32.GetDC(hwnd)

def render_A(width):
    md = gdi32.CreateCompatibleDC(sd)
    bmp = gdi32.CreateCompatibleBitmap(sd, 16, 32)
    gdi32.SelectObject(md, bmp)
    rect = ctypes.wintypes.RECT(0, 0, 16, 32)
    brush = gdi32.CreateSolidBrush(0x00FFFFFF)
    user32.FillRect(md, ctypes.byref(rect), brush)
    gdi32.DeleteObject(brush)
    hf = gdi32.CreateFontA(16, width, 0, 0, 400, 0, 0, 0, 0, 4, 0, 2, 0, b'SimSun')
    gdi32.SelectObject(md, hf)
    gdi32.SetTextAlign(md, 0x00000001 | 0x00000000)
    gdi32.SetTextColor(md, 0x00000000)
    gdi32.SetBkColor(md, 0x00FFFFFF)
    gdi32.TextOutA(md, 0, 0, b'A', 1)

    rows = []
    for r in range(32):
        byte = 0
        for c in range(8):
            if gdi32.GetPixel(md, c, r) == 0:
                byte |= (1 << (7 - c))
        rows.append(byte)

    gdi32.DeleteObject(hf)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(md)
    return rows

print("width=8:")
rows = render_A(8)
for r in range(32):
    if rows[r]:
        pat = ''.join('#' if (rows[r]>>(7-c))&1 else '.' for c in range(8))
        print(f"  r{r:2d}: 0x{rows[r]:02X} {pat}")

print("\nwidth=0:")
rows = render_A(0)
for r in range(32):
    if rows[r]:
        pat = ''.join('#' if (rows[r]>>(7-c))&1 else '.' for c in range(8))
        print(f"  r{r:2d}: 0x{rows[r]:02X} {pat}")

user32.ReleaseDC(hwnd, sd)
