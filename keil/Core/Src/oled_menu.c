/*
 * oled_menu.c — 128x64 单色 OLED 多层菜单模块
 *
 * 使用 OLED_MenuRow() 和 OLED_FillRow() 实现菜单渲染
 * 选中项以 ▶ 标记, 非选中项前留空格对齐
 * 支持页式翻页 (多于 4 项时自动分页)
 *
 * 依赖:
 *   - OLED.h (OLED_MenuRow, OLED_FillRow, OLED_Clear)
 */

#include "oled_menu.h"
#include "OLED.h"

/* ============================================================
 * 内部辅助
 * ============================================================ */

/** 计算最后一页的顶部索引 */
static uint8_t last_page_top(const OLED_Menu *menu)
{
    if (menu->count == 0) return 0;
    uint8_t step = menu->visibleLines;
    return ((menu->count - 1) / step) * step;
}

/** 调整视口 (选中项改变时) */
static void adjust_view(OLED_Menu *menu, uint8_t oldSel)
{
    if (menu->count <= menu->visibleLines)
    {
        menu->viewTop = 0;
        return;
    }

    uint8_t step = menu->visibleLines;

    /* 循环: 从末尾跳到开头 / 从开头跳到末尾 */
    if (oldSel == menu->count - 1 && menu->selection == 0)
        menu->viewTop = 0;
    else if (oldSel == 0 && menu->selection == menu->count - 1)
        menu->viewTop = last_page_top(menu);
    else if (menu->selection >= menu->viewTop + step)
        menu->viewTop += step;
    else if (menu->selection < menu->viewTop)
        menu->viewTop -= step;
}

/* ============================================================
 * 初始化
 * ============================================================ */
void OLED_Menu_Init(OLED_Menu *menu)
{
    menu->items        = NULL;
    menu->count        = 0;
    menu->selection    = 0;
    menu->startLine    = 1;
    menu->visibleLines = 4;      /* 128x64 OLED: 4 行 */
    menu->viewTop      = 0;
    menu->historyDepth = 0;
}

void OLED_Menu_SetRoot(OLED_Menu *menu, const OLED_MenuItem *items, uint8_t count)
{
    menu->items        = items;
    menu->count        = count;
    menu->selection    = 0;
    menu->viewTop      = 0;
    menu->historyDepth = 0;
}

/* ============================================================
 * 全量绘制
 * ============================================================ */
void OLED_Menu_Draw(const OLED_Menu *menu)
{
    if (menu->items == NULL) return;

    OLED_Clear();

    uint8_t end = menu->viewTop + menu->visibleLines;
    if (end > menu->count) end = menu->count;

    for (uint8_t i = menu->viewTop; i < end; i++)
    {
        uint8_t line = menu->startLine + (i - menu->viewTop);
        OLED_MenuRow(line, menu->items[i].text, i == menu->selection);
    }
}

/* ============================================================
 * 选中移动
 * ============================================================ */
static void menu_move(OLED_Menu *menu, uint8_t newSel)
{
    if (menu->count == 0) return;

    uint8_t oldSel = menu->selection;
    uint8_t oldTop = menu->viewTop;
    menu->selection = newSel;
    adjust_view(menu, oldSel);

    if (menu->viewTop != oldTop)
    {
        /* 页滚动了 → 全量重绘 */
        OLED_Menu_Draw(menu);
    }
    else
    {
        /* 同页内, 只更新旧/新两行 */
        OLED_MenuRow(menu->startLine + (oldSel - menu->viewTop),
                     menu->items[oldSel].text, 0);
        OLED_MenuRow(menu->startLine + (menu->selection - menu->viewTop),
                     menu->items[menu->selection].text, 1);
    }
}

void OLED_Menu_Next(OLED_Menu *menu)
{
    if (menu->count == 0) return;
    uint8_t next = (menu->selection >= menu->count - 1) ? 0 : (menu->selection + 1);
    menu_move(menu, next);
}

void OLED_Menu_Prev(OLED_Menu *menu)
{
    if (menu->count == 0) return;
    uint8_t prev = (menu->selection == 0) ? (menu->count - 1) : (menu->selection - 1);
    menu_move(menu, prev);
}

/* ============================================================
 * Enter — 确认/进入子菜单
 * ============================================================ */
uint8_t OLED_Menu_Enter(OLED_Menu *menu)
{
    if (menu->count == 0) return 0;

    const OLED_MenuItem *item = &menu->items[menu->selection];

    if (item->submenu != NULL && item->submenuCount > 0)
    {
        /* 记住当前菜单 */
        if (menu->historyDepth < OLED_MENU_MAX_DEPTH)
        {
            menu->history[menu->historyDepth].items     = menu->items;
            menu->history[menu->historyDepth].count     = menu->count;
            menu->history[menu->historyDepth].selection = menu->selection;
            menu->historyDepth++;
        }
        /* 切换到子菜单 */
        menu->items     = item->submenu;
        menu->count     = item->submenuCount;
        menu->selection = 0;
        menu->viewTop   = 0;
        OLED_Menu_Draw(menu);
        return OLED_MENU_ENTER_SUBMENU;
    }

    /* 叶子节点: 回调 */
    if (item->action != NULL)
        item->action();
		   OLED_Menu_Draw(menu);
    return menu->selection;
}

/* ============================================================
 * Back — 返回上级
 * ============================================================ */
uint8_t OLED_Menu_Back(OLED_Menu *menu)
{
    if (menu->historyDepth == 0) return 0;

    menu->historyDepth--;
    menu->items     = menu->history[menu->historyDepth].items;
    menu->count     = menu->history[menu->historyDepth].count;
    menu->selection = menu->history[menu->historyDepth].selection;
    menu->viewTop   = 0;
    OLED_Menu_Draw(menu);
    return 1;
}

/* ============================================================
 * 辅助
 * ============================================================ */
uint8_t OLED_Menu_GetSelection(const OLED_Menu *menu)
{
    return menu->selection;
}

uint8_t OLED_Menu_IsRoot(const OLED_Menu *menu)
{
    return (menu->historyDepth == 0) ? 1 : 0;
}
