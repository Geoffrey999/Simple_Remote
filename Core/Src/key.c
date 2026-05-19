#include "key.h"
#include "cmsis_os.h"
#include "stdio.h"

uint8_t KEY_Scan(void) {
    static uint8_t key_last_state = 0;
    static uint32_t key_hold_start = 0;
    static uint8_t key_hold_sent = 0;
    
    uint8_t key_return = KEY_NONE;
    
    uint8_t key_current_state = 0;
    if(HAL_GPIO_ReadPin(KEY_UP_PORT, KEY_UP_PIN) == GPIO_PIN_RESET) key_current_state |= KEY_UP;
    if(HAL_GPIO_ReadPin(KEY_DOWN_PORT, KEY_DOWN_PIN) == GPIO_PIN_RESET) key_current_state |= KEY_DOWN;
    if(HAL_GPIO_ReadPin(KEY_ENTER_PORT, KEY_ENTER_PIN) == GPIO_PIN_RESET) key_current_state |= KEY_ENTER;
    if(HAL_GPIO_ReadPin(KEY_BACK_PORT, KEY_BACK_PIN) == GPIO_PIN_RESET) key_current_state |= KEY_BACK;

    uint8_t key_new_press = (key_current_state & ~key_last_state);
    
    if(key_new_press) {
        osDelay(20);
        
        uint8_t key_confirm = 0;
        if(HAL_GPIO_ReadPin(KEY_UP_PORT, KEY_UP_PIN) == GPIO_PIN_RESET) key_confirm |= KEY_UP;
        if(HAL_GPIO_ReadPin(KEY_DOWN_PORT, KEY_DOWN_PIN) == GPIO_PIN_RESET) key_confirm |= KEY_DOWN;
        if(HAL_GPIO_ReadPin(KEY_ENTER_PORT, KEY_ENTER_PIN) == GPIO_PIN_RESET) key_confirm |= KEY_ENTER;
        if(HAL_GPIO_ReadPin(KEY_BACK_PORT, KEY_BACK_PIN) == GPIO_PIN_RESET) key_confirm |= KEY_BACK;
        
        key_return = key_confirm & ~key_last_state;
        
        if(key_return) {
            key_hold_start = HAL_GetTick();
            key_hold_sent = 0;
        }
    }
    
    if(key_current_state & (KEY_UP | KEY_DOWN)) {
        uint32_t hold_time = HAL_GetTick() - key_hold_start;
        
        if(hold_time >= KEY_HOLD_TIME_MS) {
            if(key_current_state & KEY_UP) {
                if((hold_time - KEY_HOLD_TIME_MS) >= (key_hold_sent * KEY_REPEAT_TIME_MS)) {
                    key_return = KEY_UP_HOLD;
                    key_hold_sent++;
                }
            }
            else if(key_current_state & KEY_DOWN) {
                if((hold_time - KEY_HOLD_TIME_MS) >= (key_hold_sent * KEY_REPEAT_TIME_MS)) {
                    key_return = KEY_DOWN_HOLD;
                    key_hold_sent++;
                }
            }
        }
    }
    else {
        key_hold_sent = 0;
    }
    
    key_last_state = key_current_state;
    
    if(key_return & KEY_UP_HOLD) return KEY_UP_HOLD;
    if(key_return & KEY_DOWN_HOLD) return KEY_DOWN_HOLD;
    if(key_return & KEY_UP) return KEY_UP;
    if(key_return & KEY_DOWN) return KEY_DOWN;
    if(key_return & KEY_ENTER) return KEY_ENTER;
    if(key_return & KEY_BACK) return KEY_BACK;
    
    return KEY_NONE;
}
