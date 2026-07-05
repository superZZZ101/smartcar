# -*- coding: utf-8 -*-
import os

base = r'C:\ti\mspm0_sdk_2_10_00_04\m0g3507\menu-setParameters'
old_path = base + r'\empty.c'

with open(old_path, 'rb') as f:
    raw = f.read()

# Try to decode
for enc in ['gbk', 'utf-8']:
    try:
        text = raw.decode(enc)
        print('Decoded as', enc)
        break
    except:
        text = None

if text is None:
    # Force decode with replacement
    text = raw.decode('utf-8', errors='replace')
    print('Force decoded')

# Find and replace variable definitions + padding
old_vars = 'volatile int16_t left_speed=0, right_speed=0;'
new_vars = '#include "motor_vars.h"'

text = text.replace(old_vars, new_vars)
text = text.replace('bool pwm_flag=1;', '')
# Remove any padding lines
text = text.replace('char _pad2[32];', '')
text = text.replace('static char _padding[64];', '')

# Write back as gbk
with open(old_path, 'wb') as f:
    f.write(text.encode('gbk', errors='replace'))

print('empty.c updated, size:', len(text))
