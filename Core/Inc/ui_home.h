#ifndef __UI_HOME_H__
#define __UI_HOME_H__

#include "main.h"

/*
 * 首页的"视图数据"：页面只认这个结构，不直接碰协议帧 / RTOS，
 * 这样既能单独在 PC 上渲染验证，也方便 P3 抽成统一的页面接口。
 */
typedef struct
{
    uint16_t roll;          /* 1000..2000 */
    uint16_t pitch;
    uint16_t yaw;
    uint16_t throttle;
    uint16_t arm;           /* 1000=上锁  1500=中位  2000=解锁 */
    uint16_t mode;          /* 1000=Angle 1500=Horizon 2000=Acro */
    uint8_t  bat;           /* 发射机电量 0..100 */
    uint8_t  rssi;          /* 链路信号 0..100 */
    uint8_t  rx_valid;      /* 是否收到过遥测帧 */
} UiHomeData_t;

void UI_Home_Render(const UiHomeData_t* d);

#endif
