/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gpio.h"
#include "usart.h"
#include "adc.h"
#include "oled.h"
#include "ui_core.h"
#include "menu.h"
#include "protocol.h"
#include "key.h"
#include "stm32f1xx_it.h"
#include <string.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BATTERY_MIN_VOLTAGE	3300.0f		//3.7V
#define BATTERY_MAX_VOLTAGE	3752.0f		//4.2V
#define SCALING_FACTOR		4.52f		//(BATTERY_MAX_VOLTAGE-BATTERY_MIN_VOLTAGE)/100

/*
 * 摇杆 ADC 标定：中点 + 两端各自独立映射。
 *
 * 旧写法：
 *     out = 1500 + (adc - CENTER) / (ADC_MAX / 1000)
 * 分母用的是"满量程"，但偏移是从中点算的。中点是 1986、满量程 4025，
 * 两者不是对半关系（一半应该是 2012.5），所以低半边增益大、高半边增益小，
 * 实测低端 1006、高端 2006 —— 正好对上。
 *
 * 现在左右两侧各自标定：中点 -> 1500、低端 -> 1000、高端 -> 2000，三点都精确命中。
 */
#define ROLL_ADC_CENTER		1986		/* 摇杆回中时的 ADC 读数 */
#define ROLL_ADC_MIN		0			/* 摇杆推到下极限时的 ADC 读数 */
#define ROLL_ADC_MAX		4025		/* 摇杆推到上极限时的 ADC 读数 */

#define PITCH_ADC_CENTER	1993
#define PITCH_ADC_MIN		0
#define PITCH_ADC_MAX		4025

#define THROTTLE_ADC_MIN	0			/* 油门是单向的，只有两端 */
#define THROTTLE_ADC_MAX	4022

/* 所有发给飞控的 PWM/通道值都必须落在这个范围内 */
#define SERVO_MIN			1000
#define SERVO_MAX			2000
#define SERVO_CENTER		1500
#define YAW_TRIM_STEP		2

/*
 * 解锁联锁：油门必须"归零"才允许 arm 从 1000/1500 切到 2000。
 * 注意通道值是 1000..2000，没有真正的 0，所以"归零"= 油门在最低位。
 * 阈值给 10 个单位的余量，防止 ADC 噪声刚好卡在边界上。
 */
#define ARM_THROTTLE_MAX	(SERVO_MIN + 10)

/* 超过这个时间没收到任何遥测帧，就重启一次接收通道 */
#define RX_TIMEOUT_MS		1000

/* 飞控回传电量低于这个百分比（0~100）时，绿灯常亮 */
#define TELEM_BAT_LOW		10

/* UI 帧周期：静态画面 50 fps，过渡/选择条动画期间提到 62 fps */
#define DISPLAY_FRAME_MS	20
#define ANIM_FRAME_MS		14
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static osMessageQueueId_t TxFrameQueue;
/* USER CODE END Variables */
/* Definitions for DisplayTask */
/* 栈深 1024 字节：链接器静态调用图给出的最大栈深仅 256 字节，
   sprintf 浮点格式化链路约 300 字节，留 2 倍以上余量即可。
   原来 8KB 白占了 20KB RAM 的 40%。 */
osThreadId_t DisplayTaskHandle;
const osThreadAttr_t DisplayTask_attributes = {
  .name = "DisplayTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for InputTask */
osThreadId_t InputTaskHandle;
const osThreadAttr_t InputTask_attributes = {
  .name = "InputTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for UartTxTask */
osThreadId_t UartTxTaskHandle;
const osThreadAttr_t UartTxTask_attributes = {
  .name = "UartTxTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
/* USER CODE END FunctionPrototypes */

void StartDisplayTask(void *argument);
void StartInputTask(void *argument);
void StartUartTxTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationIdleHook(void);
void vApplicationTickHook(void);
void vApplicationMallocFailedHook(void);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

/* USER CODE BEGIN 2 */

/* 致命错误：亮红灯并停机。
   堆/栈耗尽属于程序错误，继续跑只会在别的地方随机失效，停机比乱跑安全。 */
static void App_Fatal(void)
{
	RED_LIGHT_ON;
	__disable_irq();
	for (;;)
	{
	}
}

/* 把值限制在 [min, max] 内 */
static uint16_t Clamp_u16(int32_t value, int32_t min, int32_t max)
{
	if (value < min) return (uint16_t)min;
	if (value > max) return (uint16_t)max;
	return (uint16_t)value;
}

/*
 * 摇杆映射：中点两侧各用一条直线。
 *   中点        -> 1500
 *   adc_max     -> 2000
 *   adc_min     -> 1000
 * 两侧斜率不同也没关系，因为中点、两端都是按实测值标定的。
 *
 * 两端各有 AXIS_EDGE_SNAP 个 ADC 计数的"吸附区"：
 * 摇杆压到机械极限时 ADC 会在 ±2~3 个计数上抖，不吸附的话输出会在
 * 1999/2000 之间来回跳。吸附区约 1.5 个输出单位，肉眼看不出。
 */
#define AXIS_EDGE_SNAP		6

/* 四舍五入的整数除法（分子分母均非负） */
static int32_t Div_Round(int32_t num, int32_t den)
{
	return (num + den / 2) / den;
}

static uint16_t Axis_Map(uint16_t adc, uint16_t center, uint16_t adc_min, uint16_t adc_max)
{
	int32_t out;

	/* 端点吸附：先判饱和，避免端点抖动 */
	if ((adc_max > center) && ((int32_t)adc >= ((int32_t)adc_max - AXIS_EDGE_SNAP)))
	{
		return SERVO_MAX;
	}
	if ((center > adc_min) && ((int32_t)adc <= ((int32_t)adc_min + AXIS_EDGE_SNAP)))
	{
		return SERVO_MIN;
	}

	if (adc >= center)
	{
		if (adc_max <= center)
		{
			return SERVO_CENTER;
		}
		out = SERVO_CENTER + Div_Round((int32_t)(adc - center) * (SERVO_MAX - SERVO_CENTER),
		                               (int32_t)(adc_max - center));
	}
	else
	{
		if (center <= adc_min)
		{
			return SERVO_CENTER;
		}
		out = SERVO_CENTER - Div_Round((int32_t)(center - adc) * (SERVO_CENTER - SERVO_MIN),
		                               (int32_t)(center - adc_min));
	}

	return Clamp_u16(out, SERVO_MIN, SERVO_MAX);
}

/* 油门单向：adc_min -> 1000，adc_max -> 2000，两端同样带吸附区 */
static uint16_t Throttle_Map(uint16_t adc)
{
	int32_t out;

	if (THROTTLE_ADC_MAX <= THROTTLE_ADC_MIN)
	{
		return SERVO_MIN;
	}

	/* 怠速端和满油端吸附，避免油门显示/输出在端点抖 */
	if ((int32_t)adc <= ((int32_t)THROTTLE_ADC_MIN + AXIS_EDGE_SNAP))
	{
		return SERVO_MIN;
	}
	if ((int32_t)adc >= ((int32_t)THROTTLE_ADC_MAX - AXIS_EDGE_SNAP))
	{
		return SERVO_MAX;
	}

	out = SERVO_MIN + Div_Round((int32_t)(adc - THROTTLE_ADC_MIN) * (SERVO_MAX - SERVO_MIN),
	                            (int32_t)(THROTTLE_ADC_MAX - THROTTLE_ADC_MIN));

	return Clamp_u16(out, SERVO_MIN, SERVO_MAX);
}

/* FreeRTOS：动态内存分配失败（configUSE_MALLOC_FAILED_HOOK = 1） */
void vApplicationMallocFailedHook(void)
{
	App_Fatal();
}

/* FreeRTOS：任务栈溢出（configCHECK_FOR_STACK_OVERFLOW = 2） */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
	(void)xTask;
	(void)pcTaskName;
	App_Fatal();
}

void vApplicationIdleHook( void )
{
   /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
   to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
   task. It is essential that code added to this hook function never attempts
   to block in any way (for example, call xQueueReceive() with a block time
   specified, or call vTaskDelay()). If the application makes use of the
   vTaskDelete() API function (as this demo application does) then it is also
   important that vApplicationIdleHook() is permitted to return to its calling
   function, because it is the responsibility of the idle task to clean up
   memory allocated by the kernel to any task that has since been deleted. */
}
/* USER CODE END 2 */

/* USER CODE BEGIN 3 */
void vApplicationTickHook( void )
{
   /* This function will be called by each tick interrupt if
   configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h. User code can be
   added here, but the tick hook is called from an interrupt context, so
   code must not attempt to block, and only the interrupt safe FreeRTOS API
   functions can be used (those that end in FromISR()). */
}
/* USER CODE END 3 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  settings_mutex = osMutexNew(NULL);
  if (settings_mutex == NULL) App_Fatal();
  frame_mutex = osMutexNew(NULL);		/* 保护"最新一帧"快照 */
  if (frame_mutex == NULL) App_Fatal();
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  TxFrameQueue = osMessageQueueNew(3, sizeof(Tx_Frame_t), NULL);
  /* RX 队列深度给到 4：UartTxTask 每 16ms 取一次，深度 4 足够吸收突发帧 */
  RxFrameQueue = osMessageQueueNew(4, sizeof(Rx_Frame_t), NULL);
  KeyEventQueue = osMessageQueueNew(8, sizeof(uint8_t), NULL);
  if ((TxFrameQueue == NULL) || (RxFrameQueue == NULL) || (KeyEventQueue == NULL))
  {
    App_Fatal();
  }
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of DisplayTask */
  DisplayTaskHandle = osThreadNew(StartDisplayTask, NULL, &DisplayTask_attributes);

  /* creation of InputTask */
  InputTaskHandle = osThreadNew(StartInputTask, NULL, &InputTask_attributes);

  /* creation of UartTxTask */
  UartTxTaskHandle = osThreadNew(StartUartTxTask, NULL, &UartTxTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  if ((DisplayTaskHandle == NULL) || (InputTaskHandle == NULL) || (UartTxTaskHandle == NULL))
  {
    App_Fatal();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDisplayTask */
/**
  * @brief  Function implementing the DisplayTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDisplayTask */
void StartDisplayTask(void *argument)
{
  /* USER CODE BEGIN StartDisplayTask */
	TickType_t last_tick;

	OLED_Init();
	ui_begin();			/* 帧缓冲清零，保证第一帧渲染前是黑的 */
	Menu_Init();

	last_tick = xTaskGetTickCount();

  /* Infinite loop */
  for(;;)
  {
		Menu_Task_Handler();
		vTaskDelayUntil(&last_tick,
			pdMS_TO_TICKS(Menu_Is_Animating() ? ANIM_FRAME_MS : DISPLAY_FRAME_MS));
  }
  /* USER CODE END StartDisplayTask */
}

/* USER CODE BEGIN Header_StartInputTask */
/**
* @brief Function implementing the InputTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartInputTask */
void StartInputTask(void *argument)
{
  /* USER CODE BEGIN StartInputTask */
	/* ADC 由 DMA 直接写入，必须是 volatile（否则提高优化等级后读到缓存值）。
	   放成 static 也顺带省下 10 字节任务栈。 */
	static volatile uint16_t adc_buf[5];
	TickType_t last_tick;
	static Tx_Frame_t tx_frame = {0};
	MenuSettings_t settings;
	static uint8_t arm_pos_prev;		/* 解锁开关上一次是否在"解锁"位（边沿检测） */

	tx_frame.head = PROTOCOL_FRAME_HEAD;
	for(int i=1;i<15;i++)
	{
		*(((uint16_t*)&tx_frame)+i) = 1500;
	}
	tx_frame.throttle = 1000;
	HAL_ADC_Start_DMA(&hadc1,(uint32_t*)adc_buf,5);
	osDelay(1);

	last_tick = xTaskGetTickCount();	/* 只在循环外取一次，vTaskDelayUntil 会自己更新它 */

	/* Infinite loop */
	for(;;)
	{
		//Correct joystick ADC values (中点/两端分别标定，保证 1000 / 1500 / 2000 精确)
		tx_frame.roll 		= Axis_Map(adc_buf[0], ROLL_ADC_CENTER,  ROLL_ADC_MIN,  ROLL_ADC_MAX);
		tx_frame.pitch 		= Axis_Map(adc_buf[1], PITCH_ADC_CENTER, PITCH_ADC_MIN, PITCH_ADC_MAX);
		tx_frame.throttle 	= Throttle_Map(adc_buf[2]);

		/* 中点附近 2 个单位的死区：ADC 噪声不会让静止值来回跳（也避免飞控把它当动作） */
		if (abs((int)tx_frame.roll - SERVO_CENTER) < 2)  tx_frame.roll  = SERVO_CENTER;
		if (abs((int)tx_frame.pitch - SERVO_CENTER) < 2) tx_frame.pitch = SERVO_CENTER;

		Remote_Bat 			= (adc_buf[4]-BATTERY_MIN_VOLTAGE)/SCALING_FACTOR;
		if(Remote_Bat >= 100)Remote_Bat = 100;
		if(Remote_Bat <= 0){ Remote_Bat = 0;RED_LIGHT_ON;}else {RED_LIGHT_OFF;}

		//Yaw, control with buttons (persisted via static tx_frame), clamped to SERVO_MIN..SERVO_MAX
		if((HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_13)==RESET) && (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_14)==RESET))
		{
			tx_frame.yaw = 1500;		//两个键同时按：回中
		}
		else if((HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_13)==RESET))
		{
			tx_frame.yaw = Clamp_u16((int32_t)tx_frame.yaw - YAW_TRIM_STEP, SERVO_MIN, SERVO_MAX);
		}
		else if((HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_14)==RESET))
		{
			tx_frame.yaw = Clamp_u16((int32_t)tx_frame.yaw + YAW_TRIM_STEP, SERVO_MIN, SERVO_MAX);
		}

		//ARM：解锁（切到 2000）前必须油门归零，否则保持原状态并在屏幕上提示
		{
			uint8_t pa3_set  = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == SET) ? 1u : 0u;
			uint8_t pa4_set  = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == SET) ? 1u : 0u;
			uint8_t arm_pos  = (uint8_t)(pa3_set && !pa4_set);   /* 开关拨在"解锁"位 */

			if(!pa3_set && pa4_set)
			{
				tx_frame.arm=1000;
			}
			else if(pa3_set && pa4_set)
			{
				tx_frame.arm=1500;
			}
			else if(arm_pos)
			{
				if(tx_frame.throttle <= ARM_THROTTLE_MAX)
				{
					tx_frame.arm=2000;
				}
				else if(!arm_pos_prev)
				{
					/* 刚拨到解锁位、但油门没收到底：拒绝解锁（arm 保持原值），
					   只在"拨动那一下"弹提示，避免飞行中每帧都糊一个提示框 */
					Menu_Notify_ArmDenied();
				}
			}

			arm_pos_prev = arm_pos;
		}

		//MODE
		if((HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13)==RESET)&&(HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_14)==SET))
		{
			tx_frame.mode=1000;
		}
		else if((HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13)==SET)&&(HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_14)==SET))
		{
			tx_frame.mode=1500;
		}
		else if((HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_13)==SET)&&(HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_14)==RESET))
		{
			tx_frame.mode=2000;
		}

		//Read menu-editable settings (a consistent snapshot, locked inside protocol.c)
		Settings_Snapshot(&settings);
		tx_frame.aux3 = settings.aux3;
		tx_frame.aux4 = settings.aux4;
		tx_frame.aux5 = settings.aux5;
		tx_frame.aux6 = settings.aux6;
		tx_frame.aux7 = settings.aux7;
		tx_frame.aux8 = settings.aux8;
		tx_frame.roll_offset = settings.roll_offset;
		tx_frame.pitch_offset = settings.pitch_offset;

		tx_frame.checksum = Calculate_Checksum((uint8_t*)&tx_frame,30);

		//Send complete frame to UartTxTask via queue
		osMessageQueuePut(TxFrameQueue, &tx_frame, 0, 0);

		//Scan keys and send events to DisplayTask
		uint8_t key = KEY_Scan();
		if(key != KEY_NONE)
		{
			osMessageQueuePut(KeyEventQueue, &key, 0, 0);
		}

		vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(16));
	}
  /* USER CODE END StartInputTask */
}

/* USER CODE BEGIN Header_StartUartTxTask */
/**
* @brief Function implementing the UartTxTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUartTxTask */
void StartUartTxTask(void *argument)
{
  /* USER CODE BEGIN StartUartTxTask */
	/* 乒乓缓冲：DMA 发送期间源地址必须一直有效，不能再用循环里的栈变量 */
	static Tx_Frame_t tx_buf[2];
	uint8_t  tx_idx = 0;
	uint32_t last_rx_tick;

	/* 启动 USART1 的 DMA + IDLE 接收（协议层负责校验、入队和重新挂载） */
	Protocol_Start_Receive();
	last_rx_tick = HAL_GetTick();

	/* Infinite loop */
	for(;;)
	{
		Tx_Frame_t *tx_to_send = &tx_buf[tx_idx];

		tx_idx ^= 1;

		//Wait for new TX frame from InputTask (blocks here)
		osMessageQueueGet(TxFrameQueue, tx_to_send, 0, osWaitForever);

		//Save snapshot for DisplayTask
		TxFrame_Latest_Set(tx_to_send);

		//Send via DMA
		if(HAL_UART_Transmit_DMA(&huart1, (uint8_t*)tx_to_send, sizeof(Tx_Frame_t)) != HAL_OK)
		{
			/* 上一帧还没发完或 TX 通道卡住：只中止发送通道，再补发一次 */
			(void)HAL_UART_AbortTransmit(&huart1);
			(void)HAL_UART_Transmit_DMA(&huart1, (uint8_t*)tx_to_send, sizeof(Tx_Frame_t));
		}

		//每轮都取一次 RX，只保留最新的（原来每 4 轮取一次 = 64ms，队列里大量帧被丢）
		{
			Rx_Frame_t rx_frame;

			if(osMessageQueueGet(RxFrameQueue, &rx_frame, 0, 0) == osOK)
			{
				//Drain remaining RX frames (keep the latest)
				while(osMessageQueueGet(RxFrameQueue, &rx_frame, 0, 0) == osOK)
				{
				}

				RxFrame_Latest_Set(&rx_frame);
				last_rx_tick = HAL_GetTick();

				/* 链路恢复：强制熄灭蓝灯。
				   BLUE_LIGHT_BLINK 是"翻转"，如果恢复时正好停在下一次翻转前
				   （也就是亮着的那一拍），不显式熄灭就会一直亮着。 */
				BLUE_LIGHT_OFF;

				/* 飞控回传的电量低于 10%：绿灯常亮提醒（高于 10% 就熄灭） */
				if(rx_frame.bat < TELEM_BAT_LOW)
				{
					GREEN_LIGHT_ON;
				}
				else
				{
					GREEN_LIGHT_OFF;
				}
			}
			else if((HAL_GetTick() - last_rx_tick) > RX_TIMEOUT_MS)
			{
				/* 超时没收到任何帧：只重启接收通道（不会碰 TX DMA） */
				Protocol_Restart_Receive();
				last_rx_tick = HAL_GetTick();

				/* 链路断了，电量提示失效，交给蓝灯慢闪表达 */
				GREEN_LIGHT_OFF;
				BLUE_LIGHT_BLINK;
			}
		}
	}
  /* USER CODE END StartUartTxTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

