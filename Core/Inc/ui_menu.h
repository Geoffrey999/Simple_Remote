#ifndef __UI_MENU_H__
#define __UI_MENU_H__

#include "main.h"

#define UI_MENU_ROWS    4       /* 一屏能放几项 */
#define UI_MENU_PITCH   10      /* 行间距 */

/*
 * 菜单列表页：只负责画，不管导航状态。
 *   title  —— 标题
 *   items  —— 名称数组
 *   count  —— 总数
 *   cursor —— 当前选中项（0 基）
 *   scroll —— 第一个可见项（0 基）
 *   sel_y  —— 选择条的像素纵坐标（可动画）
 */
void UI_Menu_Render(const char* title, const char* const* items, uint8_t count,
                    uint8_t cursor, uint8_t scroll, int16_t sel_y);

#endif
