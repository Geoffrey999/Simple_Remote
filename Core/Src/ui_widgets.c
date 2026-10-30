#include "ui_widgets.h"

/* ------------------------------------------------------------------------- */
/* 文本排版                                                                  */
/* ------------------------------------------------------------------------- */

void ui_text_center(int16_t y, const char* s, uint8_t font)
{
    uint16_t w = ui_text_width(s, font);
    int16_t x = (int16_t)((UI_W - (int16_t)w) / 2);

    if (x < 0)
    {
        x = 0;
    }

    ui_text(x, y, s, font);
}

void ui_text_right(int16_t x_right, int16_t y, const char* s, uint8_t font)
{
    uint16_t w = ui_text_width(s, font);

    ui_text((int16_t)(x_right - (int16_t)w), y, s, font);
}

void ui_text_right_inv(int16_t x_right, int16_t y, const char* s, uint8_t font)
{
    uint16_t w = ui_text_width(s, font);

    ui_text_inv((int16_t)(x_right - (int16_t)w), y, s, font);
}

/* ------------------------------------------------------------------------- */
/* 页眉 / 页脚                                                               */
/* ------------------------------------------------------------------------- */

void ui_header(const char* title, const char* right_text)
{
    ui_fill_rect(0, 0, UI_W - 1, UI_HDR_H - 1, UI_ON);

    ui_text_inv(UI_MARGIN_X, 2, title, UI_F_TITLE);

    if (right_text != 0)
    {
        uint16_t w = ui_text_width(right_text, UI_F_TITLE);

        if (w < UI_W)
        {
            ui_text_inv((int16_t)(UI_W - UI_MARGIN_X - (int16_t)w), 2, right_text, UI_F_TITLE);
        }
    }
}

void ui_footer(const char* text)
{
    ui_text(UI_MARGIN_X, UI_H - 8, text, UI_F_BODY);
}

/* ------------------------------------------------------------------------- */
/* 进度条                                                                    */
/* ------------------------------------------------------------------------- */

void ui_bar(int16_t x, int16_t y, uint8_t w, uint8_t h,
            int16_t value, int16_t vmin, int16_t vmax, uint8_t mode)
{
    int16_t inner_w;
    int16_t pos;
    int16_t mid;

    if ((w < 4) || (h < 3) || (vmax <= vmin))
    {
        return;
    }

    ui_rect(x, y, (int16_t)(x + w - 1), (int16_t)(y + h - 1), UI_ON);

    inner_w = (int16_t)(w - 2);

    pos = (int16_t)(((int32_t)(value - vmin) * (inner_w - 1)) / (vmax - vmin));

    if (pos < 0)             pos = 0;
    if (pos > inner_w - 1)   pos = (int16_t)(inner_w - 1);

    if (mode == UI_BAR_CENTER)
    {
        mid = (int16_t)((inner_w - 1) / 2);

        if (pos >= mid)
        {
            ui_fill_rect((int16_t)(x + 1 + mid), (int16_t)(y + 1),
                         (int16_t)(x + 1 + pos), (int16_t)(y + h - 2), UI_ON);
        }
        else
        {
            ui_fill_rect((int16_t)(x + 1 + pos), (int16_t)(y + 1),
                         (int16_t)(x + 1 + mid), (int16_t)(y + h - 2), UI_ON);
        }
    }
    else
    {
        ui_fill_rect((int16_t)(x + 1), (int16_t)(y + 1),
                     (int16_t)(x + 1 + pos), (int16_t)(y + h - 2), UI_ON);
    }
}

/* ------------------------------------------------------------------------- */
/* 反白标签                                                                  */
/* ------------------------------------------------------------------------- */

uint8_t ui_chip(int16_t x, int16_t y, const char* text, uint8_t font)
{
    uint8_t w   = (uint8_t)(ui_text_width(text, font) + 2 * UI_CHIP_PAD_X);
    uint8_t hgt = (uint8_t)(font + 2 * UI_CHIP_PAD_Y);

    ui_fill_rect(x, y, (int16_t)(x + w - 1), (int16_t)(y + hgt - 1), UI_ON);
    ui_text_inv((int16_t)(x + UI_CHIP_PAD_X), (int16_t)(y + UI_CHIP_PAD_Y), text, font);

    return w;
}

/* ------------------------------------------------------------------------- */
/* 状态图标                                                                  */
/* ------------------------------------------------------------------------- */

void ui_battery(int16_t x, int16_t y, uint8_t percent, uint8_t ink)
{
    uint8_t fill;

    if (percent > 100)
    {
        percent = 100;
    }

    /* 13x7 外壳 + 右侧凸点 */
    ui_rect(x, y, (int16_t)(x + 12), (int16_t)(y + 6), ink);
    ui_fill_rect((int16_t)(x + 13), (int16_t)(y + 2), (int16_t)(x + 14), (int16_t)(y + 4), ink);

    /* 内部 9 列可用宽度 */
    fill = (uint8_t)((percent * 9u) / 100u);

    if (fill > 0)
    {
        ui_fill_rect((int16_t)(x + 2), (int16_t)(y + 2),
                     (int16_t)(x + 1 + fill), (int16_t)(y + 4), ink);
    }
}

void ui_rssi(int16_t x, int16_t y, uint8_t level, uint8_t ink)
{
    uint8_t i;

    for (i = 0; i < 4; i++)
    {
        uint8_t hgt = (uint8_t)(3 + i);

        if (i < level)
        {
            ui_fill_rect((int16_t)(x + i * 3), (int16_t)(y + 7 - hgt),
                         (int16_t)(x + i * 3 + 1), (int16_t)(y + 6), ink);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 菜单行                                                                    */
/* ------------------------------------------------------------------------- */

/*
 * 菜单行：只画白色文字和三角标记。
 * 反白选择条不在这里画 —— 由调用方最后用 ui_xor_rect() 覆盖，
 * 这样条子可以平滑滑动，文字不需要重画也不会闪。
 */
void ui_list_row(int16_t x, int16_t y, const char* text, uint8_t marker)
{
    ui_text((int16_t)(x + 12), (int16_t)(y + 2), text, UI_F_BODY);

    if (marker)
    {
        uint8_t r;

        /* 4x7 实心小三角 */
        for (r = 0; r < 7; r++)
        {
            uint8_t len = (uint8_t)((r < 4) ? (r + 1) : (7 - r));

            ui_fill_rect((int16_t)(x + 4), (int16_t)(y + 2 + r),
                         (int16_t)(x + 4 + len - 1), (int16_t)(y + 2 + r), UI_ON);
        }
    }
}
