#ifndef __OLED_H__
#define __OLED_H__

#include "main.h"

//#define OLED_SCLK_Clr() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET)//CLK
//#define OLED_SCLK_Set() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET)

//#define OLED_SDIN_Clr() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET)//DIN
//#define OLED_SDIN_Set() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET)

#define OLED_RST_Clr() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET)//RES
#define OLED_RST_Set() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET)

#define OLED_DC_Clr() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET)//DC
#define OLED_DC_Set() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET)
 		     
#define OLED_CS_Clr()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET)//CS
#define OLED_CS_Set()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET)

#define X_WIDTH 128
#define Y_WIDTH 64

extern uint8_t oled_frame_buffer[8][128];

void OLED_Init(void);
void OLED_Refresh(void);
void OLED_Fill(uint8_t oled_frame_buffer);
void OLED_Clear_Frame_Buffer(void);
void OLED_DrawPoint(uint8_t x,uint8_t y,uint8_t t);
void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t font_size,uint8_t mode);
void OLED_ShowString(uint8_t x,uint8_t y,char *chr,uint8_t size,uint8_t mode);
#endif
