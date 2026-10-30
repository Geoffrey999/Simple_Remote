#include "protocol.h"
#include "usart.h"

int16_t Remote_Bat;
volatile Rx_Frame_t Rx_Frame;		/* USART1 的 DMA 目标缓冲区 */
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
osMutexId_t frame_mutex;
osMessageQueueId_t RxFrameQueue;
osMessageQueueId_t KeyEventQueue;

/* 显示用的"最新一帧"，只能通过下面的 *_Latest_* 接口访问 */
static Tx_Frame_t latest_tx_frame;
static Rx_Frame_t latest_rx_frame;

/* 遥测链路心跳：最后一次收到帧的时刻 */
static uint32_t s_last_rx_tick;
static uint8_t  s_rx_seen;

/* 通道值的合法范围（µs） */
#define SETTING_MIN_VALUE   1000
#define SETTING_MAX_VALUE   2000

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

/*
 * 重新挂上 USART1 的 DMA + IDLE 接收。
 * 只能在 RxState == READY 时调用（HAL 回调里调用是安全的：HAL 进入回调前
 * 已经把 RxState 置回 READY）。
 */
void Protocol_Start_Receive(void)
{
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart1, (uint8_t*)&Rx_Frame, PROTOCOL_RX_FRAME_SIZE);
}

/*
 * 强制重启接收：先只中止接收通道（HAL_UART_AbortReceive 不会碰 TX DMA），
 * 再重新挂载。用于长时间收不到帧时的看门狗式恢复。
 */
void Protocol_Restart_Receive(void)
{
    (void)HAL_UART_AbortReceive(&huart1);
    Protocol_Start_Receive();
}

/*
 * HAL 接收事件回调（中断上下文）：
 *   HAL_UART_RXEVENT_TC   —— 收满 PROTOCOL_RX_FRAME_SIZE 字节
 *   HAL_UART_RXEVENT_IDLE —— 线路空闲但没收满（半帧，丢弃）
 * 回调返回前必须重新挂接收，否则后面再也收不到数据。
 *
 * 注意：进到这里时 DMA 已经停止（TC 或 HAL 在 IDLE 分支里 abort 了接收通道），
 * 所以 Rx_Frame 的内容在这一刻是稳定的。
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART1)
    {
        return;
    }

    if (Size == PROTOCOL_RX_FRAME_SIZE)
    {
        uint16_t checksum_calc = Calculate_Checksum((uint8_t*)&Rx_Frame,
                                                    PROTOCOL_RX_FRAME_SIZE - 2);
        if ((Rx_Frame.head == PROTOCOL_FRAME_HEAD) &&
            (Rx_Frame.checksum == checksum_calc))
        {
            /* Rx_Frame 是 volatile，这里显式去掉限定符；DMA 已停止，拷贝安全 */
            (void)osMessageQueuePut(RxFrameQueue, (const void*)&Rx_Frame, 0, 0);
        }
    }

    Protocol_Start_Receive();
}

/* ------------------------------------------------------------------------- */
/* 最新帧快照                                                                */
/* ------------------------------------------------------------------------- */

void TxFrame_Latest_Set(const Tx_Frame_t* frame)
{
    if (osMutexAcquire(frame_mutex, osWaitForever) == osOK)
    {
        latest_tx_frame = *frame;
        osMutexRelease(frame_mutex);
    }
}

void TxFrame_Latest_Get(Tx_Frame_t* frame)
{
    if (osMutexAcquire(frame_mutex, osWaitForever) == osOK)
    {
        *frame = latest_tx_frame;
        osMutexRelease(frame_mutex);
    }
}

void RxFrame_Latest_Set(const Rx_Frame_t* frame)
{
    if (osMutexAcquire(frame_mutex, osWaitForever) == osOK)
    {
        latest_rx_frame = *frame;
        s_last_rx_tick = HAL_GetTick();
        s_rx_seen = 1;
        osMutexRelease(frame_mutex);
    }
}

void RxFrame_Latest_Get(Rx_Frame_t* frame)
{
    if (osMutexAcquire(frame_mutex, osWaitForever) == osOK)
    {
        *frame = latest_rx_frame;
        osMutexRelease(frame_mutex);
    }
}

uint8_t RxLink_IsAlive(void)
{
    if (s_rx_seen == 0)
    {
        return 0;
    }

    /* 32 位无符号相减，tick 回绕也是对的 */
    return (uint8_t)(((HAL_GetTick() - s_last_rx_tick) <= RX_LINK_TIMEOUT_MS) ? 1 : 0);
}

/* ------------------------------------------------------------------------- */
/* 菜单设置：所有读写都走这里，保证加锁 + 限幅                               */
/* ------------------------------------------------------------------------- */

/* 用显式 switch 取字段地址，避免 &aux3 + index 这种跨成员的指针运算 */
static uint16_t* Settings_Field(SettingId_t id)
{
    switch (id)
    {
        case SETTING_AUX3:         return &menu_settings.aux3;
        case SETTING_AUX4:         return &menu_settings.aux4;
        case SETTING_AUX5:         return &menu_settings.aux5;
        case SETTING_AUX6:         return &menu_settings.aux6;
        case SETTING_AUX7:         return &menu_settings.aux7;
        case SETTING_AUX8:         return &menu_settings.aux8;
        case SETTING_ROLL_OFFSET:  return &menu_settings.roll_offset;
        case SETTING_PITCH_OFFSET: return &menu_settings.pitch_offset;
        default:                   return 0;
    }
}

void Settings_Snapshot(MenuSettings_t* out)
{
    if (out == 0)
    {
        return;
    }

    if (osMutexAcquire(settings_mutex, osWaitForever) == osOK)
    {
        *out = menu_settings;
        osMutexRelease(settings_mutex);
    }
}

uint16_t Settings_Get(SettingId_t id)
{
    uint16_t value = 1500;
    uint16_t* field;

    if (osMutexAcquire(settings_mutex, osWaitForever) != osOK)
    {
        return value;
    }

    field = Settings_Field(id);
    if (field != 0)
    {
        value = *field;
    }

    osMutexRelease(settings_mutex);
    return value;
}

void Settings_Adjust(SettingId_t id, int32_t delta)
{
    uint16_t* field;
    int32_t value;

    if (osMutexAcquire(settings_mutex, osWaitForever) != osOK)
    {
        return;
    }

    field = Settings_Field(id);
    if (field != 0)
    {
        value = (int32_t)(*field) + delta;

        if (value < SETTING_MIN_VALUE) value = SETTING_MIN_VALUE;
        if (value > SETTING_MAX_VALUE) value = SETTING_MAX_VALUE;

        *field = (uint16_t)value;
    }

    osMutexRelease(settings_mutex);
}
