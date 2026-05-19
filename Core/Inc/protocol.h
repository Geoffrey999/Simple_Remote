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

extern Rx_Frame_t Rx_Frame;
extern int16_t Remote_Bat;
extern MenuSettings_t menu_settings;
extern osMutexId_t settings_mutex;
extern osMessageQueueId_t RxFrameQueue;
extern osMessageQueueId_t KeyEventQueue;
extern volatile Tx_Frame_t latest_tx_frame;
extern volatile Rx_Frame_t latest_rx_frame;

uint16_t Calculate_Checksum(const uint8_t* data, uint16_t len);

#endif
