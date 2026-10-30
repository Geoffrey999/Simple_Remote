#include "ui_icons.h"

/*
 * 8x8 图标点阵：每个字节是一行，bit7 = 最左边的像素。
 * 直接在源码里按行"画"出来，改起来最直观。
 */
static const uint8_t ui_icon_bitmap[UI_ICON_COUNT][UI_ICON_H] =
{
    /* LEFT：实心左三角 */
    { 0x00, 0x10, 0x30, 0x70, 0xF0, 0x70, 0x30, 0x10 },

    /* RIGHT：实心右三角 */
    { 0x00, 0x08, 0x0C, 0x0E, 0x0F, 0x0E, 0x0C, 0x08 },

    /* LOCK：锁体 + 闭合锁梁 */
    { 0x3C, 0x42, 0x42, 0xFF, 0x81, 0x99, 0xFF, 0x00 },

    /* UNLOCK：锁梁单侧打开 */
    { 0x3C, 0x42, 0x02, 0xFF, 0x81, 0x99, 0xFF, 0x00 },

    /* CHECK：对勾 */
    { 0x01, 0x02, 0x44, 0x28, 0x10, 0x00, 0x00, 0x00 },

    /* WARN：感叹号 */
    { 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00 }
};

void ui_icon(int16_t x, int16_t y, uint8_t icon, uint8_t ink)
{
    uint8_t row;
    uint8_t col;

    if (icon >= UI_ICON_COUNT)
    {
        return;
    }

    for (row = 0; row < UI_ICON_H; row++)
    {
        uint8_t bits = ui_icon_bitmap[icon][row];

        for (col = 0; col < UI_ICON_W; col++)
        {
            if (bits & (uint8_t)(0x80u >> col))
            {
                ui_px((int16_t)(x + col), (int16_t)(y + row), ink);
            }
        }
    }
}
