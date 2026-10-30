#ifndef __UI_WIDGETS_H__
#define __UI_WIDGETS_H__

#include "ui_theme.h"
#include "ui_icons.h"

/* 进度条填充方向 */
#define UI_BAR_LEFT     0       /* 从左往右（油门） */
#define UI_BAR_CENTER   1       /* 以中心为原点双向（摇杆） */

/* ---- 文本排版 ---- */
void ui_text_center(int16_t y, const char* s, uint8_t font);
void ui_text_right(int16_t x_right, int16_t y, const char* s, uint8_t font);
void ui_text_right_inv(int16_t x_right, int16_t y, const char* s, uint8_t font);

/* ---- 页眉 / 页脚 ---- */
void ui_header(const char* title, const char* right_text);
void ui_footer(const char* text);

/* ---- 进度条 ---- */
void ui_bar(int16_t x, int16_t y, uint8_t w, uint8_t h,
            int16_t value, int16_t vmin, int16_t vmax, uint8_t mode);

/* ---- 反白标签：返回实际占用宽度 ---- */
uint8_t ui_chip(int16_t x, int16_t y, const char* text, uint8_t font);

/* ---- 状态图标 ---- */
void ui_battery(int16_t x, int16_t y, uint8_t percent, uint8_t ink);
void ui_rssi(int16_t x, int16_t y, uint8_t level, uint8_t ink);

/* ---- 菜单行：反白条由调用方用 ui_xor_rect 单独画 ---- */
void ui_list_row(int16_t x, int16_t y, const char* text, uint8_t marker);

#endif
