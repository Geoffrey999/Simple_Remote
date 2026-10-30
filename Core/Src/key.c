#include "key.h"
#include "cmsis_os.h"

/*
 * 非阻塞扫描：KEY_Scan() 由 InputTask 每 16ms 调用一次。
 *
 * 原来用的是 osDelay(20) 二次采样消抖，会把 16ms 的控制周期直接拖到 36ms，
 * 按键时必然丢掉一到两帧遥控数据。这里改成"连续 N 次采样一致才认"的状态机，
 * 全程不阻塞。
 *
 * KEY_DEBOUNCE_SAMPLES = 2 → 约 32ms 的消抖窗口。
 */
#define KEY_DEBOUNCE_SAMPLES   2

static uint8_t KEY_ReadRaw(void)
{
    uint8_t state = 0;

    if(HAL_GPIO_ReadPin(KEY_UP_PORT,    KEY_UP_PIN)    == GPIO_PIN_RESET) state |= KEY_UP;
    if(HAL_GPIO_ReadPin(KEY_DOWN_PORT,  KEY_DOWN_PIN)  == GPIO_PIN_RESET) state |= KEY_DOWN;
    if(HAL_GPIO_ReadPin(KEY_ENTER_PORT, KEY_ENTER_PIN) == GPIO_PIN_RESET) state |= KEY_ENTER;
    if(HAL_GPIO_ReadPin(KEY_BACK_PORT,  KEY_BACK_PIN)  == GPIO_PIN_RESET) state |= KEY_BACK;

    return state;
}

uint8_t KEY_Scan(void)
{
    static uint8_t  key_raw_last   = 0;     /* 上一次的原始采样 */
    static uint8_t  key_debounce   = 0;     /* 连续一致次数 */
    static uint8_t  key_stable     = 0;     /* 消抖后的稳定状态 */
    static uint8_t  key_stable_last= 0;     /* 上一轮的稳定状态（找边沿） */
    static uint32_t key_hold_start = 0;
    static uint32_t key_hold_sent  = 0;

    uint8_t key_raw = KEY_ReadRaw();

    /* ---- 消抖：连续 KEY_DEBOUNCE_SAMPLES 次采样一致才更新稳定状态 ---- */
    if(key_raw != key_raw_last)
    {
        key_raw_last = key_raw;
        key_debounce = 0;
    }
    else if(key_debounce < KEY_DEBOUNCE_SAMPLES)
    {
        key_debounce++;
    }

    if(key_debounce >= KEY_DEBOUNCE_SAMPLES)
    {
        key_stable = key_raw;
    }

    uint8_t key_pressed = (uint8_t)(key_stable & ~key_stable_last);
    uint8_t key_event   = KEY_NONE;

    /* ---- 长按重复（只对 UP/DOWN 有效）---- */
    if(key_stable & (KEY_UP | KEY_DOWN))
    {
        if(key_pressed & (KEY_UP | KEY_DOWN))
        {
            key_hold_start = HAL_GetTick();
            key_hold_sent  = 0;
        }
        else
        {
            uint32_t hold_time = HAL_GetTick() - key_hold_start;

            if(hold_time >= KEY_HOLD_TIME_MS)
            {
                /* 第 1 次重复在 KEY_HOLD_TIME_MS，之后每 KEY_REPEAT_TIME_MS 一次 */
                uint32_t repeat_index = ((hold_time - KEY_HOLD_TIME_MS) / KEY_REPEAT_TIME_MS) + 1;

                if(repeat_index > key_hold_sent)
                {
                    key_hold_sent = repeat_index;
                    key_event = (key_stable & KEY_UP) ? KEY_UP_HOLD : KEY_DOWN_HOLD;
                }
            }
        }
    }
    else
    {
        key_hold_sent = 0;
    }

    key_stable_last = key_stable;

    if(key_event != KEY_NONE) return key_event;

    /* ---- 普通按下事件（每一次只上报一个，保持原来的优先级）---- */
    if(key_pressed & KEY_UP)    return KEY_UP;
    if(key_pressed & KEY_DOWN)  return KEY_DOWN;
    if(key_pressed & KEY_ENTER) return KEY_ENTER;
    if(key_pressed & KEY_BACK)  return KEY_BACK;

    return KEY_NONE;
}
