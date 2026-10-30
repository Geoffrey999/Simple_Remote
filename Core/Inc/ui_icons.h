#ifndef __UI_ICONS_H__
#define __UI_ICONS_H__

#include "ui_core.h"

/* 8x8 单色图标 */
#define UI_ICON_LEFT    0       /* 返回 / 上一级 */
#define UI_ICON_RIGHT   1       /* 进入 / 下一级 */
#define UI_ICON_LOCK    2       /* 已锁定 */
#define UI_ICON_UNLOCK  3       /* 已解锁 */
#define UI_ICON_CHECK   4       /* 确认 */
#define UI_ICON_WARN    5       /* 告警 */
#define UI_ICON_COUNT   6

#define UI_ICON_W       8
#define UI_ICON_H       8

/* ink: UI_ON 画白点（黑底上），UI_OFF 挖黑点（白底上，比如反白的标题条里） */
void ui_icon(int16_t x, int16_t y, uint8_t icon, uint8_t ink);

#endif
