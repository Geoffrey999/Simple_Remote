#ifndef __UI_ATTITUDE_H__
#define __UI_ATTITUDE_H__

#include "main.h"

/* 姿态页的视图数据（来自遥测帧） */
typedef struct
{
    float    roll;          /* 度 */
    float    pitch;
    float    yaw;
    float    altitude;      /* 米 */
    uint16_t bat;           /* 飞控电量 0..100 */
    uint16_t rssi;          /* 链路 0..100 */
    uint8_t  link_ok;       /* 遥测是否在线 */
} UiAttitudeData_t;

void UI_Attitude_Render(const UiAttitudeData_t* d);

#endif
