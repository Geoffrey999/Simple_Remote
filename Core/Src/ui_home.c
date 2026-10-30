#include <stdio.h>
#include "ui_home.h"
#include "ui_widgets.h"
#include "ui_icons.h"

/*
 * 首页布局（128x64）：
 *
 *   0..11    反白标题条：左侧机型，右侧电池图标（不再显示百分比数字）
 *   13..51   4 路通道：标签 + 进度条 + 右对齐数值
 *              ROL/PIT/YAW 用中心双向条，THR 用左起条
 *   54..63   模式 chip + 锁定 chip + 链路信号格 + RSSI 数值
 */

/* 通道条的显示值做平滑跟随，避免 ADC 抖动让进度条一直跳 */
static int16_t Home_Anim[4] = { 1500, 1500, 1500, 1000 };

/* 每帧走剩余差的 1/3：50fps 下约 4~5 帧（80~100ms）到位 */
static int16_t Home_Slew(int16_t current, int16_t target)
{
    int16_t delta = (int16_t)(target - current);

    if ((delta >= -2) && (delta <= 2))
    {
        return target;
    }

    return (int16_t)(current + delta / 3);
}

static const char* Home_Mode_Text(uint16_t mode)
{
    switch (mode)
    {
        case 1000: return "ANGLE";
        case 1500: return "LAND";
        case 2000: return "HORIZ";
        default:   return "----";
    }
}

void UI_Home_Render(const UiHomeData_t* d)
{
    static const char* const labels[4] = { "ROL", "PIT", "YAW", "THR" };

    char buf[8];
    const char* arm_text;
    uint8_t mode_w;
    uint8_t rssi_level;
    int16_t target[4];
    int16_t y;
    uint8_t i;

    target[0] = (int16_t)d->roll;
    target[1] = (int16_t)d->pitch;
    target[2] = (int16_t)d->yaw;
    target[3] = (int16_t)d->throttle;

    /* ---- 顶部标题条：只留电池图标，不再显示百分比数字 ---- */
    ui_header("REMOTE", 0);
    ui_battery(111, 2, d->bat, UI_OFF);

    /* ---- 4 路通道 ---- */
    for (i = 0; i < 4; i++)
    {
        Home_Anim[i] = Home_Slew(Home_Anim[i], target[i]);

        y = (int16_t)(13 + i * 10);

        ui_text(0, (int16_t)(y + 1), labels[i], UI_F_BODY);

        sprintf(buf, "%d", target[i]);
        ui_text_right(126, (int16_t)(y + 1), buf, UI_F_BODY);

        ui_bar(20, (int16_t)(y + 2), 82, 7, Home_Anim[i], 1000, 2000,
               (i == 3) ? UI_BAR_LEFT : UI_BAR_CENTER);
    }

    /* ---- 底部状态 ---- */
    arm_text = (d->arm == 2000) ? "ARMED" : "SAFE";

    mode_w = ui_chip(0, 54, Home_Mode_Text(d->mode), UI_F_BODY);
    ui_chip((int16_t)(mode_w + 3), 54, arm_text, UI_F_BODY);

    /* 信号格 + 具体 RSSI 数值；断链时两者都不画。
       向上取整：只要收到过信号（rssi > 0）至少亮一格，和右边的数字对得上。 */
    if (d->rx_valid != 0)
    {
        rssi_level = (uint8_t)((d->rssi + 24u) / 25u);

        if (rssi_level > 4)
        {
            rssi_level = 4;
        }

        ui_rssi(92, 56, rssi_level, UI_ON);

        sprintf(buf, "%u", (unsigned)d->rssi);
        ui_text_right(126, 55, buf, UI_F_BODY);
    }
    /* 断链：这里什么都不画。帧缓冲每帧全量重绘，留空就是干净的黑色，
       既不会留最后一次的旧数值，也不会显示一个误导人的 0。 */
}
