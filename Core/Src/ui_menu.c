#include <stdio.h>
#include "ui_menu.h"
#include "ui_widgets.h"

/*
 * 菜单页布局（128x64）：
 *   0..11    反白标题条 + 右上角 "2/5"
 *   13..52   4 行列表（行高 10）
 *   56..63   底部操作提示
 *
 * sel_y 是选择条的像素纵坐标，可以从一行的位置平滑移到另一行的位置；
 * 条子用"异或反白"盖在文字上，所以滑动过程中文字不需要重画。
 */
void UI_Menu_Render(const char* title, const char* const* items, uint8_t count,
                    uint8_t cursor, uint8_t scroll, int16_t sel_y)
{
    char buf[8];
    uint8_t i;

    sprintf(buf, "%d/%d", (int)cursor + 1, (int)count);
    ui_header(title, buf);

    for (i = 0; i < UI_MENU_ROWS; i++)
    {
        uint8_t index = (uint8_t)(scroll + i);

        if (index >= count)
        {
            break;
        }

        ui_list_row(0, (int16_t)(13 + i * UI_MENU_PITCH), items[index],
                    (uint8_t)(index == cursor));
    }

    if (count > 0)
    {
        ui_xor_rect(0, sel_y, UI_W - 1, (int16_t)(sel_y + UI_LIST_ROW_H - 1));
    }

    ui_footer("UP/DN:SEL  EN:OK");
}
