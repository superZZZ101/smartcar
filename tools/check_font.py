#!/usr/bin/env python3
"""Visual check of generated 8x16 ASCII font"""
import re

import os
script_dir = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(script_dir, '..', 'keil', 'Core', 'Inc', 'ascii_8x16_font.h'), 'r', encoding='utf-8') as f:
    content = f.read()

start = content.find('{')
end = content.find('};')
data_str = content[start+1:end].strip()
hex_vals = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', data_str)]
print(f'Total bytes: {len(hex_vals)}')

def show_char(ch):
    idx = (ord(ch) - 0x20) * 16
    chars = hex_vals[idx:idx+16]
    print(f"Char '{ch}' (0x{ord(ch):02X}):")
    for row, byte in enumerate(chars):
        if byte == 0:
            line = '.' * 8
        else:
            line = ''.join('#' if (byte >> (7 - c)) & 1 else '.' for c in range(8))
        print(f'  {line}')
    print()

for ch in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789>:+/-':
    show_char(ch)
