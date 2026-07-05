/*
 * oled_menu.h — 128x64 单色 OLED 多层菜单模块
 *
 * 支持: 多层嵌套子菜单, 页式滚动, 按键导航
 * 使用: UP/DOWN/ENTER 三键操作
 * 每行 16 像素高, 共 4 行可见
 *
 * 用法:
 *   static const OLED_MenuItem subItems[] = {
 *       {"Sub 1", NULL, 0, action_fn},
 *       {"< Back", mainItems, OLED_MENU_COUNT(mainItems)},
 *   };
 *   static const OLED_MenuItem mainItems[] = {
 *       {"Menu 1", subItems, OLED_MENU_COUNT(subItems)},
 *       {"Menu 2", NULL, 0},
 *   };
 *
 *   OLED_Menu menu;
 *   OLED_Menu_Init(&menu);
 *   OLED_Menu_SetRoot(&menu, mainItems, OLED_MENU_COUNT(mainItems));
 *   OLED_Menu_Draw(&menu);
 *
 *   // 主循环:
 *   switch (KEY_Scan()) {
 *       case KEY_UP:    OLED_Menu_Prev(&menu); break;
 *       case KEY_DOWN:  OLED_Menu_Next(&menu); break;
 *       case KEY_ENTER: OLED_Menu_Enter(&menu); break;
 *   }
 */

#ifndef __OLED_MENU_H
#define __OLED_MENU_H

#include "stdint.h"
#include <stddef.h>

#define OLED_MENU_COUNT(arr)       (sizeof(arr) / sizeof((arr)[0]))
#define OLED_MENU_MAX_DEPTH        4
#define OLED_MENU_ENTER_SUBMENU   0xFF   /* Enter 返回此值表示进入子菜单 */

/** 菜单项回调 — 选中叶子节点时自动调用 */
typedef void (*MenuAction)(void);

/** 菜单项 (支持嵌套子菜单) */
typedef struct OLED_MenuItem OLED_MenuItem;

struct OLED_MenuItem {
    const uint8_t *text;               /* 显示文本 (GB2312 中文/ASCII, 最多 7 个全角字符) */
    const OLED_MenuItem *submenu;      /* 子菜单 (NULL = 无) */
    uint8_t submenuCount;              /* 子菜单项数 */
    MenuAction action;                 /* 叶子节点回调 (NULL = 无) */
};

/** 菜单实例 */
typedef struct {
    const OLED_MenuItem *items;        /* 当前菜单 */
    uint8_t count;                     /* 项数 */
    uint8_t selection;                 /* 选中索引 */
    uint8_t startLine;                 /* 起始行 (1-based) */
    uint8_t visibleLines;              /* 可见行数 */
    uint8_t viewTop;                   /* 视口顶部索引 */

    /* 历史栈 (返回上级用) */
    struct {
        const OLED_MenuItem *items;
        uint8_t count;
        uint8_t selection;
    } history[OLED_MENU_MAX_DEPTH];
    uint8_t historyDepth;
} OLED_Menu;

/* ============================================================
 * API
 * ============================================================ */

/** @brief 初始化菜单 */
void OLED_Menu_Init(OLED_Menu *menu);

/** @brief 设置根菜单 */
void OLED_Menu_SetRoot(OLED_Menu *menu, const OLED_MenuItem *items, uint8_t count);

/** @brief 全量绘制菜单 */
void OLED_Menu_Draw(const OLED_Menu *menu);

/** @brief 选中下一项 */
void OLED_Menu_Next(OLED_Menu *menu);

/** @brief 选中上一项 */
void OLED_Menu_Prev(OLED_Menu *menu);

/**
 * @brief 确认当前选项
 * @return 有子菜单 → OLED_MENU_ENTER_SUBMENU
 *         叶子节点 → 选中索引 (0-based)
 */
uint8_t OLED_Menu_Enter(OLED_Menu *menu);

/**
 * @brief 返回上级菜单
 * @return 0=已在根, 1=成功返回
 */
uint8_t OLED_Menu_Back(OLED_Menu *menu);

/** @brief 获取当前选中索引 */
uint8_t OLED_Menu_GetSelection(const OLED_Menu *menu);

/** @brief 判断是否在根菜单 */
uint8_t OLED_Menu_IsRoot(const OLED_Menu *menu);

#endif /* __OLED_MENU_H */
