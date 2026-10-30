#include <stdio.h>
#include "ui_edit.h"
#include "ui_widgets.h"

/*
 * 编辑页布局（128x64）：
 *   0..11    标题条（编辑中右上角显示 EDIT）
 *   13..22   字段标签条：选中的反白，其余普通文字
 *   25..48   当前值（12x24 大字号，居中）
 *   50..55   进度条（以 1500 为中心）
 *   56..63   操作提示
 */
void UI_Edit_Render(const char* title, const UiField_t* fields, uint8_t count,
                    uint8_t cursor, uint8_t editing)
{
    char buf[8];
    uint16_t tab_w = 0;
    int16_t total_w;
    int16_t x;
    uint8_t i;

    if ((fields == 0) || (count == 0) || (cursor >= count))
    {
        return;
    }

    /* 所有标签用同一宽度，排列才整齐 */
    for (i = 0; i < count; i++)
    {
        uint16_t w = (uint16_t)(ui_text_width(fields[i].label, UI_F_BODY) + 2 * UI_CHIP_PAD_X);

        if (w > tab_w)
        {
            tab_w = w;
        }
    }
    if (tab_w > (UI_W / count))
    {
        tab_w = (uint16_t)(UI_W / count);
    }

    total_w = (int16_t)(count * tab_w + (count - 1) * 4);
    x = (int16_t)((UI_W - total_w) / 2);

    ui_header(title, editing ? "EDIT" : 0);

    /* 字段标签条 */
    for (i = 0; i < count; i++)
    {
        if (i == cursor)
        {
            ui_chip(x, 13, fields[i].label, UI_F_BODY);
        }
        else
        {
            uint16_t w = ui_text_width(fields[i].label, UI_F_BODY);

            ui_text((int16_t)(x + ((int16_t)tab_w - (int16_t)w) / 2), 14,
                    fields[i].label, UI_F_BODY);
        }

        x = (int16_t)(x + tab_w + 4);
    }

    /* 大号数值 */
    sprintf(buf, "%d", (int)fields[cursor].value);
    {
        uint16_t w = ui_text_width(buf, UI_F_HUGE);

        ui_text((int16_t)((UI_W - (int16_t)w) / 2), 25, buf, UI_F_HUGE);
    }

    /* 进度条 */
    ui_bar(2, 48, 124, 6, (int16_t)fields[cursor].value, 1000, 2000, UI_BAR_CENTER);

    ui_footer(editing ? "UP/DN:ADJ  EN:OK" : "UP/DN:SEL  EN:EDIT");
}
