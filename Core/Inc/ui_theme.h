#ifndef __UI_THEME_H__
#define __UI_THEME_H__

#include "ui_core.h"

/*
 * 全局视觉常量：以后调版式只改这里，不用去翻各个页面。
 */

/* ---- 布局 ---- */
#define UI_MARGIN_X     2       /* 左右安全边距 */
#define UI_HDR_H        12      /* 顶部标题条高度 */
#define UI_FOOTER_H     10      /* 底部提示行高度 */
#define UI_ROW_H        13      /* 首页通道行高 */
#define UI_LIST_ROW_H   10      /* 菜单行高 */

/* ---- 字体 ---- */
#define UI_F_TITLE      UI_FONT_6X8
#define UI_F_BODY       UI_FONT_6X8
#define UI_F_BIG        UI_FONT_8X16
#define UI_F_HUGE       UI_FONT_12X24

/* ---- 标签(chip)内边距 ---- */
#define UI_CHIP_PAD_X   3
#define UI_CHIP_PAD_Y   1

/* ---- 进度条 ---- */
#define UI_BAR_H        7       /* 首页通道条默认高度 */

#endif
