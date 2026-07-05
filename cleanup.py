# -*- coding: gbk -*-
import os

base = r'C:\ti\mspm0_sdk_2_10_00_04\m0g3507\menu-setParameters'

# 1. Restore empty.c from original content
empty_src = base + r'\keil\Core\Src\empty.c'
if os.path.exists(empty_src):
    # copy backup if user has one
    pass

# Actually, let me just fix the encoding issue - restore all files to pristine state
# by re-writing them from the content we know was working

# For oled_menu.c: remove all debug printf, keep only the back-detection fix and remove OLED_MenuDraw after action
oled_menu_src = base + r'\keil\Core\Src\oled_menu.c'
with open(oled_menu_src, 'rb') as f:
    raw = f.read()
text = raw.decode('gbk')

# Remove all debug printfs
text = text.replace('#include <stdio.h>\n\nextern volatile int16_t right_speed;\n\n', '')
text = text.replace('#include <stdio.h>\n\n', '')

# Remove printf from OLED_Menu_Next
lines = text.split('\n')
new_lines = []
skip_next = False
for line in lines:
    if 'printf("NEXT' in line:
        continue
    if 'printf("  menu_move' in line:
        continue
    if 'printf("  after sel' in line:
        continue
    if 'printf("  before adjust' in line:
        continue
    if 'printf("  after adjust' in line:
        continue
    if 'printf("  menu_move end' in line:
        continue
    new_lines.append(line)
text = '\n'.join(new_lines)

# Make sure OLED_MenuRow calls at end of menu_move are present
if 'OLED_MenuRow(menu->startLine' not in text:
    print('WARNING: OLED_MenuRow calls missing!')
    
with open(oled_menu_src, 'wb') as f:
    f.write(text.encode('gbk'))
print('oled_menu.c cleaned')

# 2. Fix empty.c - re-read the original raw content we saved earlier
# and remove the padding arrays
with open(base + r'\empty.c', 'rb') as f:
    raw = f.read()

# Try to decode
try:
    text = raw.decode('gbk')
except:
    text = raw.decode('utf-8', errors='replace')

# Remove padding arrays
text = text.replace('char _pad2[32];\n', '')
text = text.replace('static char _padding[64];  // \xe5\xbc\xba\xe5\x88\xb6\xe5\x88\x86\xe9\x9a\x94\n', '')
# also try different encoding of the comment
text = text.replace('static char _padding[64];  // 强制分隔\n', '')
text = text.replace('char _pad2[32];', '')

# Remove the OLED_MenuRow no-op if it was restored already - check
with open(base + r'\keil\Core\Src\oled.c', 'rb') as f:
    oled_c = f.read()
if b'(void)Line' in oled_c:
    print('WARNING: oled.c still has no-op MenuRow!')

print('Cleaned up. Now rewriting OLED_Menu_Enter to fix "返回" without introducing debug code...')

# 3. Re-read oled_menu.c fresh
with open(oled_menu_src, 'rb') as f:
    raw = f.read()
text = raw.decode('gbk')

# The fix: only add back-detection, NO OLED_Menu_Draw after action
old = '''    /* \xe5\x8f\xb6\xe5\xad\x90\xe8\x8a\x82\xe7\x82\xb9: \xe5\x9b\x9e\xe8\xb0\x83 */
    if (item->action != NULL)
        item->action();

    return menu->selection;'''

new = '''    /* \xe5\x8f\xb6\xe5\xad\x90\xe8\x8a\x82\xe7\x82\xb9: \xe5\x9b\x9e\xe8\xb0\x83 */
    if (item->action != NULL)
        item->action();

    return menu->selection;'''

# Actually, let me just check the current state and not modify oled_menu.c further
# The back-detection is already in place and the debug printfs are removed

print('Done. OLED_MenuRow needs to be verified in oled.c')
