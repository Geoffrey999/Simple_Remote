#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "main.h"
#include "cmsis_os.h"

#define PROTOCOL_FRAME_HEAD      0x4020
#define PROTOCOL_RX_FRAME_SIZE   24		//8bit
#define PROTOCOL_TX_FRAME_SIZE   16		//16bit


typedef struct {
    uint16_t head;
    uint16_t roll;
    uint16_t pitch;
    uint16_t throttle;
    uint16_t yaw;
    uint16_t arm;
    uint16_t mode;
    uint16_t aux3;
    uint16_t aux4;
    uint16_t aux5;
    uint16_t aux6;
    uint16_t aux7;
    uint16_t aux8;
    uint16_t roll_offset;
    uint16_t pitch_offset;
    uint16_t checksum;
} Tx_Frame_t;

typedef struct {
    uint16_t head;
    uint16_t bat;
    float roll;
    float pitch;
    float yaw;
	float altitude;
    uint16_t rssi;
    uint16_t checksum;
} Rx_Frame_t;

typedef struct {
    uint16_t aux3;
    uint16_t aux4;
    uint16_t aux5;
    uint16_t aux6;
    uint16_t aux7;
    uint16_t aux8;
    uint16_t roll_offset;
    uint16_t pitch_offset;
} MenuSettings_t;

/* 菜单可编辑的通道值：都直接发给飞控，统一由 protocol.c 加锁 + 限幅维护 */
typedef enum {
    SETTING_AUX3 = 0,
    SETTING_AUX4,
    SETTING_AUX5,
    SETTING_AUX6,
    SETTING_AUX7,
    SETTING_AUX8,
    SETTING_ROLL_OFFSET,
    SETTING_PITCH_OFFSET,
    SETTING_COUNT
} SettingId_t;

extern volatile Rx_Frame_t Rx_Frame;	/* DMA 直接写入，必须 volatile */
extern int16_t Remote_Bat;
extern MenuSettings_t menu_settings;
extern osMutexId_t settings_mutex;
extern osMutexId_t frame_mutex;
extern osMessageQueueId_t RxFrameQueue;
extern osMessageQueueId_t KeyEventQueue;

uint16_t Calculate_Checksum(const uint8_t* data, uint16_t len);

/* USART1 接收：DMA + IDLE 事件（在 protocol.c 中由 HAL 回调驱动） */
void Protocol_Start_Receive(void);
void Protocol_Restart_Receive(void);

/* 最新帧快照：内部加锁，避免 32/24 字节结构体拷贝读到"半新半旧"的数据 */
void TxFrame_Latest_Set(const Tx_Frame_t* frame);
void TxFrame_Latest_Get(Tx_Frame_t* frame);
void RxFrame_Latest_Set(const Rx_Frame_t* frame);
void RxFrame_Latest_Get(Rx_Frame_t* frame);

/* 遥测链路超时：超过这个时间没收到帧就认为链路断开 */
#define RX_LINK_TIMEOUT_MS  1000

/* 链路是否在线：最近 RX_LINK_TIMEOUT_MS 内收到过遥测帧（从没收到过也算断开） */
uint8_t RxLink_IsAlive(void);

/* 菜单设置访问：内部加锁 + 限幅到 1000~2000 */
void     Settings_Snapshot(MenuSettings_t* out);
uint16_t Settings_Get(SettingId_t id);
void     Settings_Adjust(SettingId_t id, int32_t delta);

#endif
