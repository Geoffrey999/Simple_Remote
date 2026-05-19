#include "oled.h"
#include "oledfont.h"
#include "spi.h"
#include "cmsis_os.h"
#include <string.h>

uint8_t oled_frame_buffer[8][128];
static osSemaphoreId_t oled_dma_sem;

/*
Transmit one byte data to OLED
*/
void OLED_WriteData(uint8_t data)
{
	OLED_DC_Set();
	OLED_CS_Clr();
	HAL_SPI_Transmit(&hspi1, &data, 1, 100);
	OLED_CS_Set();
	OLED_DC_Set();
}

/*
Transmit one byte command to OLED
*/	
void OLED_WriteCmd(uint8_t cmd)
{
	OLED_DC_Clr();
	OLED_CS_Clr();
	HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
	OLED_CS_Set();
	OLED_DC_Set();
}

/*
Set the position to be draw on the OLED
*/
void OLED_Set_Pos(uint8_t x, uint8_t y)
{ 
  OLED_WriteCmd(0xb0+(y));
  OLED_WriteCmd(((x&0xf0)>>4)|0x10);
  OLED_WriteCmd((x&0x0f)|0x01); 
} 

/*
Fill the OLED with the same data
*/
void OLED_Fill(uint8_t data)
{
	uint8_t y,x;
	
	for(y=0;y<8;y++)
	{
		OLED_WriteCmd(0xb0+y);
		OLED_WriteCmd(0x00);
		OLED_WriteCmd(0x10);
		for(x=0;x<X_WIDTH;x++)
			OLED_WriteData(data);
	}
}


/*
OLED Init function
*/
void OLED_Init(void)
{
	oled_dma_sem = osSemaphoreNew(1, 0, NULL);
	OLED_RST_Set(); 
	osDelay(100);
	OLED_RST_Clr();
	osDelay(200);
	OLED_RST_Set(); 

	OLED_WriteCmd(0xae);//--turn off oled panel
	OLED_WriteCmd(0x00);//---set low column address
	OLED_WriteCmd(0x10);//---set high column address
	OLED_WriteCmd(0x40);//--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
	OLED_WriteCmd(0x81);//--set contrast control register
	OLED_WriteCmd(0xcf);// Set SEG Output Current Brightness
	OLED_WriteCmd(0xa1);//--Set SEG/Column Mapping  
	OLED_WriteCmd(0xc8);//Set COM/Row Scan Direction
	OLED_WriteCmd(0xa6);//--set normal display
	OLED_WriteCmd(0xa8);//--set multiplex ratio(1 to 64)
	OLED_WriteCmd(0x3f);//--1/64 duty
	OLED_WriteCmd(0xd3);//-set display offset	Shift Mapping RAM Counter (0x00~0x3F)
	OLED_WriteCmd(0x00);//-not offset
	OLED_WriteCmd(0xd5);//--set display clock divide ratio/oscillator frequency
	OLED_WriteCmd(0x80);//--set divide ratio, Set Clock as 100 Frames/Sec
	OLED_WriteCmd(0xd9);//--set pre-charge period
	OLED_WriteCmd(0xf1);//Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
	OLED_WriteCmd(0xda);//--set com pins hardware configuration
	OLED_WriteCmd(0x12);
	OLED_WriteCmd(0xdb);//--set vcomh
	OLED_WriteCmd(0x40);//Set VCOM Deselect Level
	OLED_WriteCmd(0x20);//-Set Page Addressing Mode (0x00/0x01/0x02)
	OLED_WriteCmd(0x02);//
	OLED_WriteCmd(0x8d);//--set Charge Pump enable/disable
	OLED_WriteCmd(0x14);//--set(0x10) disable
	OLED_WriteCmd(0xa4);// Disable Entire Display On (0xa4/0xa5)
	OLED_WriteCmd(0xa6);// Disable Inverse Display On (0xa6/a7) 
	OLED_WriteCmd(0xaf);//--turn on oled panel
	OLED_Fill(0x00);  	//clear the screen
	OLED_Set_Pos(0,0);  
}

/*
Clear the OLED frame buffer
*/
void OLED_Clear_Frame_Buffer(void)
{
	memset(oled_frame_buffer, 0, sizeof(oled_frame_buffer));
}

/*
Draw a point in the frame buffer.
draw_mode =  0: draw a point
draw_mode != 0: clear a pont
*/
void OLED_DrawPoint(uint8_t x,uint8_t y,uint8_t draw_mode)
{
	uint8_t i,m,n;
	i=y/8;
	m=y%8;
	n=1<<m;
	if(!draw_mode){oled_frame_buffer[i][x]|=n;}
	else
	{
		oled_frame_buffer[i][x]=~oled_frame_buffer[i][x];
		oled_frame_buffer[i][x]|=n;
		oled_frame_buffer[i][x]=~oled_frame_buffer[i][x];
	}
}

/*
Display one character on the OLED
size: 8:Character size 6*8; 12:Character size 6*12; 16:Character size 8*16; 24:Character size 12*24; 
mode: 0: Normal display; Non-zero: Invert color
*/
void OLED_ShowChar(uint8_t x,uint8_t y,uint8_t chr,uint8_t font_size,uint8_t mode)
{
	uint8_t i,m,temp,size2,chr1;
	uint8_t x0=x,y0=y;
	if(font_size==8)size2=6;
	else size2=(font_size/8+((font_size%8)?1:0))*(font_size/2);
	chr1=chr-' ';
	for(i=0;i<size2;i++)
	{
		if(font_size==8)
			  {temp=asc2_0806[chr1][i];}
		else if(font_size==12)
        {temp=asc2_1206[chr1][i];}
		else if(font_size==16)
        {temp=asc2_1608[chr1][i];}
		else if(font_size==24)
        {temp=asc2_2412[chr1][i];}
		else return;
		for(m=0;m<8;m++)
		{
			if(temp&0x01)OLED_DrawPoint(x,y,mode);
			else OLED_DrawPoint(x,y,!mode);
			temp>>=1;
			y++;
		}
		x++;
		if((font_size!=8)&&((x-x0)==font_size/2))
		{x=x0;y0=y0+8;}
		y=y0;
  }
}

/*
Display string on the OLED
size: 8:Character size 6*8; 12:Character size 6*12; 16:Character size 8*16; 24:Character size 12*24; 
mode: 0: Normal display; Non-zero: Invert color
*/
void OLED_ShowString(uint8_t x,uint8_t y,char *chr,uint8_t size,uint8_t mode)
{
	
	while((*chr>=' ')&&(*chr<='~'))
	{
		OLED_ShowChar(x,y,*chr,size,mode);
		if(size==8)x+=6;
		else x+=size/2;
		chr++;
  }
}

/*
Transmit the frame buffer to OLED
*/
void OLED_Refresh(void)
{
	uint8_t i;

	for(i=0;i<8;i++)
	{
		OLED_WriteCmd(0xb0+i);
		OLED_WriteCmd(0x00);
		OLED_WriteCmd(0x10);
		OLED_DC_Set();
		OLED_CS_Clr();
		HAL_SPI_Transmit_DMA(&hspi1, &oled_frame_buffer[i][0], 128);
		osSemaphoreAcquire(oled_dma_sem, osWaitForever);
		OLED_CS_Set();
		OLED_DC_Set();
	}
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if(hspi == &hspi1)
	{
		osSemaphoreRelease(oled_dma_sem);
	}
}


