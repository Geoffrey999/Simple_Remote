#ifndef __UI_INFO_H__
#define __UI_INFO_H__

#include "main.h"

/* 信息页：固件版本 + 硬件信息 + 运行时间 */
void UI_Info_Render(const char* version, uint32_t uptime_s);

#endif
