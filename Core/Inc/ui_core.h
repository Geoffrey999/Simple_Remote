#ifndef __UI_CORE_H__
#define __UI_CORE_H__

#include "main.h"

/* 屏幕几何（1bpp，按 8 行一页组织，与 SSD1306 的页结构一致） */
#define UI_W        128
#define UI_H        64
#define UI_PAGES    (UI_H / 8)

/* 像素值 */
#define UI_OFF      0
#define UI_ON       1

/* 字体 id：数字就是字高，和 oledfont.h 里的命名对应 */
#define UI_FONT_6X8     8
#define UI_FONT_6X12    12
#define UI_FONT_8X16    16
#define UI_FONT_12X24   24

/*
 * 每一帧的流程：
 *     ui_begin();                      // 清缓冲、复位偏移
 *     ... 画东西 ...
 *     ui_flush();                      // 只把"内容真的变了"的页发给屏
 */
void ui_begin(void);

/* 让下一帧强制整屏发送（上电/花屏恢复时用） */
void ui_invalidate(void);

/* 面板自救：重新初始化 SSD1306 + 清屏 + 强制整屏重发 */
void ui_resync_display(void);

/* 全局绘制偏移：滑动动画就是不断改这个偏移再画两页 */
void ui_set_offset(int16_t dx, int16_t dy);

/* 图元：坐标允许为负，越界自动裁剪 */
void ui_px(int16_t x, int16_t y, uint8_t on);
void ui_hline(int16_t x0, int16_t x1, int16_t y, uint8_t on);
void ui_vline(int16_t x, int16_t y0, int16_t y1, uint8_t on);
void ui_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t on);
void ui_fill_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t on);
void ui_xor_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

/*
 * 字符/字符串：不透明绘制（字模 1 的位置置位，0 的位置清零），
 * 这正是"反白标签"需要的效果：
 *     ui_fill_rect(...);            // 先铺白底
 *     ui_text_inv(x, y, "ARMED");   // 再用 mode=1 画黑字
 */
int16_t ui_char(int16_t x, int16_t y, uint8_t ch, uint8_t font, uint8_t mode);
int16_t ui_text(int16_t x, int16_t y, const char* s, uint8_t font);
int16_t ui_text_inv(int16_t x, int16_t y, const char* s, uint8_t font);
uint16_t ui_text_width(const char* s, uint8_t font);
uint8_t ui_font_advance(uint8_t font);

/* 把帧缓冲里变化的页刷新到屏（DMA，阻塞到传输完成） */
void ui_flush(void);

/* 4x4 Bayer 抖动淡入淡出：0 = 全黑，255 = 完全显示（在 ui_flush 之前调用） */
void ui_dither_fade(uint8_t visibility);

#endif
