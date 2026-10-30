#include <stdio.h>
#include "ui_attitude.h"
#include "ui_widgets.h"
#include "ui_icons.h"

/*
 * 姿态页布局：
 *   0..11    标题条：ATTITUDE + 右侧信号格 + LINK/LOST
 *   14/25    两列 "标签 值"：ROL/PIT、YAW/BAT
 *   36..51   ALT 用 8x16 大字号右对齐
 *   56..63   底部提示
 */

/* 遥测数据可能因为校验碰撞带进离谱值，这里做一次兜底 */
static int16_t Att_Int(float v)
{
    if (!(v > -1000.0f) || !(v < 1000.0f))
    {
        return 0;
    }
    return (int16_t)v;
}

static float Att_Float(float v)
{
    if (!(v > -10000.0f) || !(v < 100000.0f))
    {
        return 0.0f;
    }
    return v;
}

void UI_Attitude_Render(const UiAttitudeData_t* d)
{
    char buf[16];
    uint8_t level;

    level = d->link_ok ? (uint8_t)(d->rssi / 25u) : 0;
    if (level > 4)
    {
        level = 4;
    }

    ui_header("ATTITUDE", 0);
    ui_rssi(84, 2, level, UI_OFF);
    ui_text_right_inv(126, 2, d->link_ok ? "LINK" : "LOST", UI_F_BODY);

    sprintf(buf, "%+d", (int)Att_Int(d->roll));
    ui_text(2, 14, "ROL", UI_F_BODY);
    ui_text_right(62, 14, buf, UI_F_BODY);

    sprintf(buf, "%+d", (int)Att_Int(d->pitch));
    ui_text(68, 14, "PIT", UI_F_BODY);
    ui_text_right(126, 14, buf, UI_F_BODY);

    sprintf(buf, "%d", (int)Att_Int(d->yaw));
    ui_text(2, 25, "YAW", UI_F_BODY);
    ui_text_right(62, 25, buf, UI_F_BODY);

    sprintf(buf, "%d%%", (int)((d->bat > 100u) ? 100u : d->bat));
    ui_text(68, 25, "BAT", UI_F_BODY);
    ui_text_right(126, 25, buf, UI_F_BODY);

    sprintf(buf, "%.1f", (double)Att_Float(d->altitude));
    ui_text(2, 42, "ALT", UI_F_BODY);
    ui_text_right(126, 36, buf, UI_F_BIG);

    ui_footer("BACK: EXIT");
}
