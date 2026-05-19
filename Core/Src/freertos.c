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

#define ROLL_ADC_CENTER		1986.0f
#define PITCH_ADC_CENTER	1993.0f
#define YAW_ADC_CENTER		2005.0f

#define ROLL_ADC_MAX		4025.0f
#define PITCH_ADC_MAX		4025.0f
#define YAW_ADC_MAX			4024.0f
#define THROTTLE_ADC_MAX	4022.0f

#define ROLL_MAX_RANGE		300.0f		//Set the output limit to 300, MAX: 1000
#define PITCH_MAX_RANGE		300.0f
#define YAW_MAX_RANGE		300.0f
#define THROTTLE_MAX_RANGE	1000.0f
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static osMessageQueueId_t TxFrameQueue;
/* USER CODE END Variables */
/* Definitions for DisplayTask */
osThreadId_t DisplayTaskHandle;
const osThreadAttr_t DisplayTask_attributes = {
  .name = "DisplayTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for InputTask */
osThreadId_t InputTaskHandle;
const osThreadAttr_t InputTask_attributes = {
  .name = "InputTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for UartTxTask */
osThreadId_t UartTxTaskHandle;
const osThreadAttr_t UartTxTask_attributes = {
  .name = "UartTxTask",
  .stack_size = 256 * 4,
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

/* USER CODE BEGIN 2 */
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
  RxFrameQueue = osMessageQueueNew(8, sizeof(Rx_Frame_t), NULL);
  KeyEventQueue = osMessageQueueNew(8, sizeof(uint8_t), NULL);
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
	OLED_Init();
	Menu_Init();
	
  /* Infinite loop */
  for(;;)
  {
		Menu_Task_Handler();
		osDelay(10);
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
	uint16_t adc_buf[5];
	TickType_t last_tick;
	static Tx_Frame_t tx_frame = {0};

	tx_frame.head = PROTOCOL_FRAME_HEAD;
	for(int i=1;i<15;i++)
	{
		*(((uint16_t*)&tx_frame)+i) = 1500;
	}
	tx_frame.throttle = 1000;
	HAL_ADC_Start_DMA(&hadc1,(uint32_t*)adc_buf,5);
	osDelay(1);
	/* Infinite loop */
	for(;;)
	{
		last_tick = xTaskGetTickCount();
		//Correct joystick ADC values
		tx_frame.roll 		= 1500 + (float)(adc_buf[0] - ROLL_ADC_CENTER)  / (ROLL_ADC_MAX/ROLL_MAX_RANGE);		//Roll
		if(abs(tx_frame.roll - 1500) < 2)tx_frame.roll = 1500;
		tx_frame.pitch 		= 1500 + (float)(adc_buf[1] - PITCH_ADC_CENTER) / (PITCH_ADC_MAX/PITCH_MAX_RANGE);	//Pitch
		if(abs(tx_frame.pitch - 1500) < 2)tx_frame.pitch = 1500;
		tx_frame.throttle 	= 1000 + (float) adc_buf[2] / (THROTTLE_ADC_MAX/THROTTLE_MAX_RANGE);					//Throttle value don't need to adjust
		Remote_Bat 			= (adc_buf[4]-BATTERY_MIN_VOLTAGE)/SCALING_FACTOR;
		if(Remote_Bat >= 100)Remote_Bat = 100;
		if(Remote_Bat <= 0){ Remote_Bat = 0;RED_LIGHT_ON;}else {RED_LIGHT_OFF;}

		//Yaw, control with buttons (persisted via static tx_frame)
		if((HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_13)==RESET))
		{
			if(tx_frame.yaw >= 1002) tx_frame.yaw -= 2;
		}
		else if((HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_14)==RESET))
		{
			if(tx_frame.yaw <= 1998) tx_frame.yaw += 2;
		}
		if((HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_13)==RESET) && (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_14)==RESET))
		{
			tx_frame.yaw = 1500;
		}

		//ARM
		if((HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_3)==RESET)&&(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_4)==SET))
		{
			tx_frame.arm=1000;
		}
		else if((HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_3)==SET)&&(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_4)==SET))
		{
			tx_frame.arm=1500;
		}
		else if((HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_3)==SET)&&(HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_4)==RESET))
		{
			tx_frame.arm=2000;
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

		//Read menu-editable settings with mutex
		osMutexAcquire(settings_mutex, osWaitForever);
		tx_frame.aux3 = menu_settings.aux3;
		tx_frame.aux4 = menu_settings.aux4;
		tx_frame.aux5 = menu_settings.aux5;
		tx_frame.aux6 = menu_settings.aux6;
		tx_frame.aux7 = menu_settings.aux7;
		tx_frame.aux8 = menu_settings.aux8;
		tx_frame.roll_offset = menu_settings.roll_offset;
		tx_frame.pitch_offset = menu_settings.pitch_offset;
		osMutexRelease(settings_mutex);

		tx_frame.checksum = Calculate_Checksum((uint8_t*)&tx_frame,30);

		//Send complete frame to UartTxTask via queue
		osMessageQueuePut(TxFrameQueue, &tx_frame, 0, 0);

		//Scan keys and send events to DisplayTask
		uint8_t key = KEY_Scan();
		if(key != KEY_NONE)
		{
			osMessageQueuePut(KeyEventQueue, &key, 0, 0);
		}
		
		vTaskDelayUntil(&last_tick,16);
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
	uint32_t no_rx_count = 0;
	uint32_t loop_count = 0;

	__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
	HAL_UART_Receive_DMA(&huart1, (uint8_t*)&Rx_Frame, PROTOCOL_RX_FRAME_SIZE);
	/* Infinite loop */
	for(;;)
	{
		Tx_Frame_t tx_to_send;

		//Wait for new TX frame from InputTask (blocks here)
		osMessageQueueGet(TxFrameQueue, &tx_to_send, 0, osWaitForever);

		//Save snapshot for DisplayTask
		latest_tx_frame = tx_to_send;

		//Send via DMA
		HAL_UART_Transmit_DMA(&huart1, (uint8_t*)&tx_to_send, sizeof(Tx_Frame_t));

		//Check for received RX frames every 4th TX cycle
		loop_count++;
		if(loop_count % 4 == 0)
		{
			Rx_Frame_t rx_frame;
			if(osMessageQueueGet(RxFrameQueue, &rx_frame, 0, 0) == osOK)
			{
				latest_rx_frame = rx_frame;
				no_rx_count = 0;
				//Drain remaining RX frames (keep the latest)
				while(osMessageQueueGet(RxFrameQueue, &rx_frame, 0, 0) == osOK)
				{
					latest_rx_frame = rx_frame;
				}
			}
			else
			{
				no_rx_count++;
			}

			if(no_rx_count > 20)
			{
				HAL_UART_DMAStop(&huart1);
				HAL_UART_Receive_DMA(&huart1, (uint8_t*)&Rx_Frame, PROTOCOL_RX_FRAME_SIZE);
				no_rx_count = 0;
				BLUE_LIGHT_BLINK;
			}
		}
	}
  /* USER CODE END StartUartTxTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

