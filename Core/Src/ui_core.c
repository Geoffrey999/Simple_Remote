#include <string.h>
#include "ui_core.h"
#include "oled.h"
#include "oledfont.h"

/* 帧缓冲：按 8 行一页组织，一个字节就是 8 个纵向像素，正好对应 SSD1306 */
static uint8_t ui_fb[UI_PAGES][UI_W];

/* 全局绘制偏移（滑动动画用） */
static int16_t s_dx;
static int16_t s_dy;

/* 每页内容的指纹：没变化的页就不用再花 SPI 时间发一遍 */
static uint32_t s_page_hash[UI_PAGES];
static uint8_t  s_page_valid;

/* 连续多少帧发送失败就重新初始化面板（约 8 帧 ≈ 0.16s） */
#define UI_FLUSH_FAIL_LIMIT     8
static uint8_t s_flush_fail;

static uint32_t ui_page_hash(uint8_t page);

/* ------------------------------------------------------------------------- */
/* 字体描述                                                                  */
/* ------------------------------------------------------------------------- */

/* 返回 1 表示字体合法。count 直接从数组元素个数算出来，
   顺带修掉 asc2_0806 实际只有 92 个字形（少了 '|' '}' '~'）的越界问题。 */
static uint8_t ui_font_info(uint8_t font, const uint8_t** glyphs,
                            uint8_t* cols, uint8_t* pages,
                            uint8_t* advance, uint8_t* count)
{
    switch (font)
    {
        case UI_FONT_6X8:
            *glyphs  = &asc2_0806[0][0];
            *cols    = 6;
            *pages   = 1;
            *advance = 6;
            *count   = (uint8_t)(sizeof(asc2_0806) / sizeof(asc2_0806[0]));
            return 1;

        case UI_FONT_6X12:
            *glyphs  = &asc2_1206[0][0];
            *cols    = 6;
            *pages   = 2;
            *advance = 6;
            *count   = (uint8_t)(sizeof(asc2_1206) / sizeof(asc2_1206[0]));
            return 1;

        case UI_FONT_8X16:
            *glyphs  = &asc2_1608[0][0];
            *cols    = 8;
            *pages   = 2;
            *advance = 8;
            *count   = (uint8_t)(sizeof(asc2_1608) / sizeof(asc2_1608[0]));
            return 1;

        case UI_FONT_12X24:
            *glyphs  = &asc2_2412[0][0];
            *cols    = 12;
            *pages   = 3;
            *advance = 12;
            *count   = (uint8_t)(sizeof(asc2_2412) / sizeof(asc2_2412[0]));
            return 1;

        default:
            return 0;
    }
}

uint8_t ui_font_advance(uint8_t font)
{
    const uint8_t* glyphs;
    uint8_t cols, pages, advance, count;

    if (!ui_font_info(font, &glyphs, &cols, &pages, &advance, &count))
    {
        return 0;
    }
    return advance;
}

/* ------------------------------------------------------------------------- */
/* 帧管理                                                                    */
/* ------------------------------------------------------------------------- */

void ui_begin(void)
{
    memset(ui_fb, 0, sizeof(ui_fb));
    s_dx = 0;
    s_dy = 0;
}

void ui_invalidate(void)
{
    s_page_valid = 0;
}

/*
 * 面板自救：重发 SSD1306 配置并把当前帧整屏重写一遍。
 * 全程不关显示、不清屏，所以屏幕上不会闪。
 */
void ui_resync_display(void)
{
    uint8_t page;

    if (!OLED_Resync(&ui_fb[0][0]))
    {
        s_page_valid = 0;       /* 没成功：下一帧整屏重发 */
        return;
    }

    /* 内容已经直接写到屏上了，更新指纹避免下一帧又发一遍 */
    for (page = 0; page < UI_PAGES; page++)
    {
        s_page_hash[page] = ui_page_hash(page);
    }
    s_page_valid = 0xFF;
    s_flush_fail = 0;
}

void ui_set_offset(int16_t dx, int16_t dy)
{
    s_dx = dx;
    s_dy = dy;
}

/* ------------------------------------------------------------------------- */
/* 图元                                                                      */
/* ------------------------------------------------------------------------- */

void ui_px(int16_t x, int16_t y, uint8_t on)
{
    int16_t px = (int16_t)(x + s_dx);
    int16_t py = (int16_t)(y + s_dy);

    if (((uint16_t)px >= UI_W) || ((uint16_t)py >= UI_H))
    {
        return;
    }

    if (on)
    {
        ui_fb[py >> 3][px] |= (uint8_t)(1u << (py & 7));
    }
    else
    {
        ui_fb[py >> 3][px] &= (uint8_t)~(1u << (py & 7));
    }
}

void ui_hline(int16_t x0, int16_t x1, int16_t y, uint8_t on)
{
    int16_t x;

    if (x0 > x1)
    {
        int16_t t = x0; x0 = x1; x1 = t;
    }

    for (x = x0; x <= x1; x++)
    {
        ui_px(x, y, on);
    }
}

void ui_vline(int16_t x, int16_t y0, int16_t y1, uint8_t on)
{
    int16_t y;

    if (y0 > y1)
    {
        int16_t t = y0; y0 = y1; y1 = t;
    }

    for (y = y0; y <= y1; y++)
    {
        ui_px(x, y, on);
    }
}

void ui_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t on)
{
    ui_hline(x0, x1, y0, on);
    ui_hline(x0, x1, y1, on);
    ui_vline(x0, y0, y1, on);
    ui_vline(x1, y0, y1, on);
}

/* op: 0 = 置位, 1 = 清零, 2 = 异或 */
static void ui_rect_op(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t op)
{
    int16_t t;
    uint8_t page, first_page, last_page, mask, mask_top, mask_bot;

    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }

    x0 = (int16_t)(x0 + s_dx);
    x1 = (int16_t)(x1 + s_dx);
    y0 = (int16_t)(y0 + s_dy);
    y1 = (int16_t)(y1 + s_dy);

    if (x0 < 0)          x0 = 0;
    if (y0 < 0)          y0 = 0;
    if (x1 > UI_W - 1)   x1 = UI_W - 1;
    if (y1 > UI_H - 1)   y1 = UI_H - 1;
    if ((x0 > x1) || (y0 > y1)) return;

    first_page = (uint8_t)(y0 >> 3);
    last_page  = (uint8_t)(y1 >> 3);

    /* 按页整字节填充，比逐点快一个数量级（整屏填充也只有 1024 次写） */
    for (page = first_page; page <= last_page; page++)
    {
        int16_t x;

        mask_top = (page == first_page) ? (uint8_t)(0xFFu << (y0 & 7)) : 0xFFu;
        mask_bot = (page == last_page)  ? (uint8_t)(0xFFu >> (7 - (y1 & 7))) : 0xFFu;
        mask     = (uint8_t)(mask_top & mask_bot);

        for (x = x0; x <= x1; x++)
        {
            if (op == 0)      ui_fb[page][x] |= mask;
            else if (op == 1) ui_fb[page][x] &= (uint8_t)~mask;
            else              ui_fb[page][x] ^= mask;
        }
    }
}

void ui_fill_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t on)
{
    ui_rect_op(x0, y0, x1, y1, on ? 0 : 1);
}

/*
 * 异或填充：选择条滑过的地方把底下的像素反相。
 * 这样"白色文字 + 反白条"可以是两张独立的图层，条子随便移动，
 * 不需要判断文字落在条内还是条外。
 */
void ui_xor_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    ui_rect_op(x0, y0, x1, y1, 2);
}

/* ------------------------------------------------------------------------- */
/* 文字                                                                      */
/* ------------------------------------------------------------------------- */

int16_t ui_char(int16_t x, int16_t y, uint8_t ch, uint8_t font, uint8_t mode)
{
    const uint8_t* glyphs;
    const uint8_t* glyph;
    uint8_t cols, pages, advance, count;
    int16_t px, py;
    uint8_t c, p;

    if (!ui_font_info(font, &glyphs, &cols, &pages, &advance, &count))
    {
        return 0;
    }

    /* 字库里没有的字符直接跳过（asc2_0806 只有 92 个字形） */
    if ((ch < ' ') || ((uint8_t)(ch - ' ') >= count))
    {
        return (int16_t)advance;
    }

    glyph = glyphs + ((uint32_t)(ch - ' ') * cols * pages);

    px = (int16_t)(x + s_dx);
    py = (int16_t)(y + s_dy);

    /* 快路径：y 对齐到页、整字在屏内 → 一个字节写一列，比逐点快 8 倍 */
    if ((px >= 0) && (py >= 0) && ((py & 7) == 0) &&
        ((px + cols) <= UI_W) && ((py + pages * 8) <= UI_H))
    {
        uint8_t page = (uint8_t)(py >> 3);

        for (c = 0; c < cols; c++)
        {
            for (p = 0; p < pages; p++)
            {
                uint8_t bits = glyph[p * cols + c];

                /* 不透明绘制：mode=0 前景置位，mode=1 反白（背景置位、字形清零） */
                ui_fb[page + p][px + c] = mode ? (uint8_t)~bits : bits;
            }
        }
        return (int16_t)advance;
    }

    /* 慢路径：带偏移/裁剪，逐像素 */
    for (c = 0; c < cols; c++)
    {
        for (p = 0; p < pages; p++)
        {
            uint8_t bits = glyph[p * cols + c];
            uint8_t i;

            for (i = 0; i < 8; i++)
            {
                uint8_t on = (uint8_t)((bits >> i) & 1u);

                ui_px((int16_t)(x + c), (int16_t)(y + p * 8 + i),
                      mode ? (uint8_t)!on : on);
            }
        }
    }

    return (int16_t)advance;
}

static int16_t ui_text_mode(int16_t x, int16_t y, const char* s, uint8_t font, uint8_t mode)
{
    uint8_t advance = ui_font_advance(font);

    if (advance == 0)
    {
        return x;
    }

    while (*s != '\0')
    {
        uint8_t ch = (uint8_t)(*s);

        if ((ch < ' ') || (ch > '~'))
        {
            break;
        }

        ui_char(x, y, ch, font, mode);
        x = (int16_t)(x + advance);

        if (x >= UI_W)
        {
            break;
        }
        s++;
    }

    return x;
}

int16_t ui_text(int16_t x, int16_t y, const char* s, uint8_t font)
{
    return ui_text_mode(x, y, s, font, 0);
}

int16_t ui_text_inv(int16_t x, int16_t y, const char* s, uint8_t font)
{
    return ui_text_mode(x, y, s, font, 1);
}

uint16_t ui_text_width(const char* s, uint8_t font)
{
    uint8_t advance = ui_font_advance(font);
    uint16_t n = 0;

    while ((*s != '\0') && (n < 255))
    {
        n++;
        s++;
    }

    return (uint16_t)(n * advance);
}

/* ------------------------------------------------------------------------- */
/* 抖动淡入淡出                                                              */
/* ------------------------------------------------------------------------- */

/* 4x4 Bayer 阈值矩阵：1bpp 屏没有 alpha，靠空间抖动模拟透明度 */
static const uint8_t ui_bayer4[4][4] =
{
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

/*
 * visibility: 0 = 全黑, 255 = 完全显示。
 * 掩码只取决于 x&3 和 y&3；而 y = page*8 + bit，(page*8)&3 == 0，
 * 所以同一列在 8 个页面里的掩码字节是一样的 —— 按列算 4 个字节就能整屏应用。
 */
void ui_dither_fade(uint8_t visibility)
{
    uint8_t masks[4];
    uint8_t k, bit, x, page;

    for (k = 0; k < 4; k++)
    {
        uint8_t m = 0;

        for (bit = 0; bit < 8; bit++)
        {
            if ((uint8_t)(ui_bayer4[k][bit & 3] * 16u) < visibility)
            {
                m |= (uint8_t)(1u << bit);
            }
        }
        masks[k] = m;
    }

    for (page = 0; page < UI_PAGES; page++)
    {
        for (x = 0; x < UI_W; x++)
        {
            ui_fb[page][x] &= masks[x & 3];
        }
    }
}

/* ------------------------------------------------------------------------- */
/* 刷新                                                                      */
/* ------------------------------------------------------------------------- */

/* 每页内容的指纹：没变化的页就不用再花 SPI 时间发一遍。
   注意这里必须用"非线性"的哈希：之前用 rotate^xor 是线性运算，
   不同内容会撞出同一个值，撞上的那一页就再也不会刷新。
   FNV-1a 32 位，碰撞概率 1/2^32，代价约 85us/帧（50fps 下 0.4% CPU）。 */
static uint32_t ui_page_hash(uint8_t page)
{
    uint32_t h = 2166136261u;
    uint8_t i;

    for (i = 0; i < UI_W; i++)
    {
        h ^= ui_fb[page][i];
        h *= 16777619u;
    }

    return h;
}

void ui_flush(void)
{
    uint32_t hashes[UI_PAGES];
    uint8_t page;
    uint8_t run_start = 0xFF;
    uint8_t run_end   = 0xFF;
    uint8_t sent_ok   = 1;

    for (page = 0; page < UI_PAGES; page++)
    {
        hashes[page] = ui_page_hash(page);
    }

    for (page = 0; page < UI_PAGES; page++)
    {
        uint8_t changed = ((s_page_valid & (1u << page)) == 0) ||
                          (hashes[page] != s_page_hash[page]);

        if (changed)
        {
            if (run_start == 0xFF)
            {
                run_start = page;
            }
            run_end = page;
        }
        else if (run_start != 0xFF)
        {
            /* 连续变化的页合并成一次 DMA */
            sent_ok &= OLED_FlushWindow(run_start, run_end, &ui_fb[run_start][0]);
            run_start = 0xFF;
        }
    }

    if (run_start != 0xFF)
    {
        sent_ok &= OLED_FlushWindow(run_start, run_end, &ui_fb[run_start][0]);
    }

    /* 只有整帧都发成功了才提交指纹，否则下一帧重发 */
    if (sent_ok)
    {
        for (page = 0; page < UI_PAGES; page++)
        {
            s_page_hash[page] = hashes[page];
        }
        s_page_valid = 0xFF;
        s_flush_fail  = 0;
    }
    else if (++s_flush_fail >= UI_FLUSH_FAIL_LIMIT)
    {
        /* 连续失败：大概率 SPI 状态机卡住了，重新初始化面板 + 整屏重发。
           这一步在显示任务里执行，几毫秒，用户最多看到一次闪。 */
        s_flush_fail = 0;
        ui_resync_display();
    }
}
