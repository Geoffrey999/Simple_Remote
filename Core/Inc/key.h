#ifndef __KEY_H__
#define __KEY_H__

#include "main.h"

#define KEY_NONE       0x00
#define KEY_UP         0x01
#define KEY_DOWN       0x02
#define KEY_ENTER      0x04
#define KEY_BACK       0x08
#define KEY_UP_HOLD    0x10
#define KEY_DOWN_HOLD  0x20

#define KEY_HOLD_TIME_MS  500
#define KEY_REPEAT_TIME_MS 100

#define KEY_UP_PIN      GPIO_PIN_8
#define KEY_UP_PORT     GPIOB
#define KEY_DOWN_PIN    GPIO_PIN_10
#define KEY_DOWN_PORT   GPIOA
#define KEY_ENTER_PIN   GPIO_PIN_9
#define KEY_ENTER_PORT  GPIOB
#define KEY_BACK_PIN    GPIO_PIN_9
#define KEY_BACK_PORT   GPIOA

uint8_t KEY_Scan(void);

#endif
