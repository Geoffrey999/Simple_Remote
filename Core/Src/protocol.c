#include "protocol.h"

int16_t Remote_Bat;
Rx_Frame_t Rx_Frame;
MenuSettings_t menu_settings = {
    .aux3 = 1500,
    .aux4 = 1500,
    .aux5 = 1500,
    .aux6 = 1500,
    .aux7 = 1500,
    .aux8 = 1500,
    .roll_offset = 1500,
    .pitch_offset = 1500,
};

osMutexId_t settings_mutex;
osMessageQueueId_t RxFrameQueue;
osMessageQueueId_t KeyEventQueue;
volatile Tx_Frame_t latest_tx_frame;
volatile Rx_Frame_t latest_rx_frame;

uint16_t Calculate_Checksum(const uint8_t* data, uint16_t len)
{
    uint16_t sum = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        sum += data[i];
    }
    sum = 0xffff - sum;
    return sum;
}
