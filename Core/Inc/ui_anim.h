#ifndef __UI_ANIM_H__
#define __UI_ANIM_H__

#include "main.h"

/*
 * 极简动画引擎：只做"从 A 到 B 的进度"，具体怎么用由调用方决定。
 * 进度是 0..255 的整数，全程无浮点。
 */
typedef struct
{
    uint32_t start_ms;
    uint16_t duration_ms;
} UiAnim_t;

void    UiAnim_Start(UiAnim_t* a, uint32_t now_ms, uint16_t duration_ms);
uint8_t UiAnim_Progress(const UiAnim_t* a, uint32_t now_ms);

/* ease-out-cubic：开头快、结尾缓，0..255 进 0..255 出 */
uint8_t ui_ease_out(uint8_t t);

#endif
