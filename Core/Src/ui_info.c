#include <stdio.h>
#include "ui_info.h"
#include "ui_widgets.h"

/*
 * 信息页布局：
 *   0..11    标题条：INFO + 右侧版本号
 *   16/27/38 三行 "标签 ... 值"（值右对齐）
 *   56..63   底部提示
 */
void UI_Info_Render(const char* version, uint32_t uptime_s)
{
    char buf[16];
    uint32_t hours = uptime_s / 3600u;
    uint32_t minutes = (uptime_s / 60u) % 60u;
    uint32_t seconds = uptime_s % 60u;

    ui_header("INFO", version);

    ui_text(2, 16, "MCU", UI_F_BODY);
    ui_text_right(126, 16, "STM32F103C8", UI_F_BODY);

    ui_text(2, 27, "LCD", UI_F_BODY);
    ui_text_right(126, 27, "SSD1306 128x64", UI_F_BODY);

    sprintf(buf, "%02u:%02u:%02u", (unsigned)hours, (unsigned)minutes, (unsigned)seconds);
    ui_text(2, 38, "Uptime", UI_F_BODY);
    ui_text_right(126, 38, buf, UI_F_BODY);

    ui_footer("BACK: EXIT");
}
