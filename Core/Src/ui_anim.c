#include "ui_anim.h"

void UiAnim_Start(UiAnim_t* a, uint32_t now_ms, uint16_t duration_ms)
{
    a->start_ms    = now_ms;
    a->duration_ms = (duration_ms == 0) ? 1 : duration_ms;
}

uint8_t UiAnim_Progress(const UiAnim_t* a, uint32_t now_ms)
{
    uint32_t elapsed = now_ms - a->start_ms;

    if (elapsed >= a->duration_ms)
    {
        return 255;
    }

    return (uint8_t)((elapsed * 255u) / a->duration_ms);
}

uint8_t ui_ease_out(uint8_t t)
{
    /* 1 - (1-t)^3，用整数算：u 最大 255，u^3 约 1.66e7，32 位足够 */
    uint32_t u  = (uint32_t)(255u - t);
    uint32_t u3 = u * u * u;

    return (uint8_t)(255u - (u3 / 65025u));     /* 65025 = 255^2 */
}
