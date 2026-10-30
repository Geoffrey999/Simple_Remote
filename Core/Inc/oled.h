#ifndef __OLED_H__
#define __OLED_H__

#include "main.h"

/* 屏幕尺寸 */
#define OLED_W          128
#define OLED_PAGES      8

/* 控制脚 */
#define OLED_RST_Clr()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET)   //RES
#define OLED_RST_Set()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET)

#define OLED_DC_Clr()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET)   //DC
#define OLED_DC_Set()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET)

#define OLED_CS_Clr()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET)  //CS
#define OLED_CS_Set()   HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET)

/*
 * 驱动层只负责"把一块显存送到屏上"：不再自己持有帧缓冲，也不做绘制。
 * 上层 ui_core.c 持有 8x128 帧缓冲，按页刷新。
 */
void OLED_Init(void);

/* 清屏（整屏写 0），返回 1 成功 */
uint8_t OLED_Clear(void);

/* 运行时重新同步：只重发寄存器配置（不关显示、不清屏），并把 full_fb
   指向的整屏数据重写一遍。返回 1 表示成功。 */
uint8_t OLED_Resync(const uint8_t* full_fb);

/* 把 data 中 (page1-page0+1)*128 字节送到指定页区间，阻塞到 DMA 完成。
   返回 1 表示发送成功（用于刷新层判断要不要提交"已发送"标记）。 */
uint8_t OLED_FlushWindow(uint8_t page0, uint8_t page1, const uint8_t* data);

#endif
