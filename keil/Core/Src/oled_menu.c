/*
 * oled_menu.c --- 128x64 ��ɫ OLED ���˵�ģ��
 *
 * ʹ�� OLED_MenuRow() �� OLED_FillRow() ʵ�ֲ˵���Ⱦ
 * ѡ������ > ���, ��ѡ����ǰ���ո����
 * ֧��ҳʽ��ҳ (���� 4 ��ʱ�Զ���ҳ)
 *
 * ����:
 *   - OLED.h (OLED_MenuRow, OLED_FillRow, OLED_Clear)
 */

#include "oled_menu.h"
#include "OLED.h"
#include <stdio.h>

extern volatile int16_t right_speed;

/* ============================================================
 * �ڲ�����
 * ============================================================ */

/** �������һҳ�Ķ������� */
static uint8_t last_page_top(const OLED_Menu *menu)
{
    if (menu->count == 0) return 0;
    uint8_t step = menu->visibleLines;
    return ((menu->count - 1) / step) * step;
}

/** �����ӿ� (ѡ����ı�ʱ) */
static void adjust_view(OLED_Menu *menu, uint8_t oldSel)
{
    if (menu->count <= menu->visibleLines)
    {
        menu->viewTop = 0;
        return;
    }

    uint8_t step = menu->visibleLines;

    /* ѭ��: ��ĩβ������ͷ / �ӿ�ͷ����ĩβ */
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
 * ��ʼ��
 * ============================================================ */
void OLED_Menu_Init(OLED_Menu *menu)
{
    menu->items        = NULL;
    menu->count        = 0;
    menu->selection    = 0;
    menu->startLine    = 1;
    menu->visibleLines = 4;      /* 128x64 OLED: 4 �� */
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
 * ȫ������
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
 * ѡ���ƶ�
 * ============================================================ */
static void menu_move(OLED_Menu *menu, uint8_t newSel)
{
    if (menu->count == 0) return;

    uint8_t oldSel = menu->selection;
    printf("  menu_move start R=%d sel=%d new=%d top=%d\r\n", right_speed, oldSel, newSel, menu->viewTop);
    uint8_t oldTop = menu->viewTop;
    menu->selection = newSel;
    printf("  after sel set R=%d\r\n", right_speed);
    printf("  before adjust R=%d\r\n", right_speed);
    adjust_view(menu, oldSel);
    printf("  after adjust R=%d\r\n", right_speed);

    if (menu->viewTop != oldTop)
    {
        /* ҳ������ -> ȫ���ػ� */
        OLED_Menu_Draw(menu);
    }
    else
    {
        /* ͬҳ��, ֻ���¾�/������ */
        OLED_MenuRow(menu->startLine + (oldSel - menu->viewTop),
                     menu->items[oldSel].text, 0);
        OLED_MenuRow(menu->startLine + (menu->selection - menu->viewTop),
                     menu->items[menu->selection].text, 1);
    }
    printf("  menu_move end R=%d\r\n", right_speed);
}

void OLED_Menu_Next(OLED_Menu *menu)
{
    if (menu->count == 0) return;
    printf("NEXT start R=%d\r\n", right_speed);
    uint8_t next = (menu->selection >= menu->count - 1) ? 0 : (menu->selection + 1);
    printf("NEXT mid R=%d\r\n", right_speed);
    menu_move(menu, next);
    printf("NEXT end R=%d\r\n", right_speed);
}

void OLED_Menu_Prev(OLED_Menu *menu)
{
    if (menu->count == 0) return;
    uint8_t prev = (menu->selection == 0) ? (menu->count - 1) : (menu->selection - 1);
    menu_move(menu, prev);
}

/* ============================================================
 * Enter --- ȷ��/�����Ӳ˵�
 * ============================================================ */
uint8_t OLED_Menu_Enter(OLED_Menu *menu)
{
    if (menu->count == 0) return 0;

    const OLED_MenuItem *item = &menu->items[menu->selection];

    if (item->submenu != NULL && item->submenuCount > 0)
    {
        /* ���ָ������ϼ��˵� -> ���� */
        if (menu->historyDepth > 0 &&
            item->submenu == menu->history[menu->historyDepth - 1].items)
        {
            OLED_Menu_Back(menu);
            return OLED_MENU_ENTER_SUBMENU;
        }
        /* ��ס��ǰ�˵� */
        if (menu->historyDepth < OLED_MENU_MAX_DEPTH)
        {
            menu->history[menu->historyDepth].items     = menu->items;
            menu->history[menu->historyDepth].count     = menu->count;
            menu->history[menu->historyDepth].selection = menu->selection;
            menu->historyDepth++;
        }
        /* �л����Ӳ˵� */
        menu->items     = item->submenu;
        menu->count     = item->submenuCount;
        menu->selection = 0;
        menu->viewTop   = 0;
        OLED_Menu_Draw(menu);
        return OLED_MENU_ENTER_SUBMENU;
    }

    /* Ҷ�ӽڵ�: �ص� */
    if (item->action != NULL)
        item->action();

    return menu->selection;
}

/* ============================================================
 * Back --- �����ϼ�
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
 * ����
 * ============================================================ */
uint8_t OLED_Menu_GetSelection(const OLED_Menu *menu)
{
    return menu->selection;
}

uint8_t OLED_Menu_IsRoot(const OLED_Menu *menu)
{
    return (menu->historyDepth == 0) ? 1 : 0;
}
