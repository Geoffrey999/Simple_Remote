#include "oled.h"
#include "spi.h"
#include "cmsis_os.h"

/*
 * SSD1306 驱动层（只做 I/O，不做绘制、不持有显存）
 *
 * 关键设计：
 *   1. 水平寻址模式 + 一次 DMA 写整个页区间；
 *   2. 初始化顺序：配置 -> 清 GDDRAM -> 再开显示。
 *      显示关着的时候清屏，用户看不到花屏闪一下；万一清屏失败也只是黑屏，
 *      不会出现"上电花屏";
 *   3. 所有 SPI 调用都检查返回值，失败就把 SPI 状态机 abort 掉，
 *      避免卡在 BUSY 里导致之后每一帧都失败（黑屏/定屏）。
 */

#define OLED_SPI_TIMEOUT_MS     20      /* 单次阻塞传输超时 */
#define OLED_DMA_TIMEOUT_MS     20      /* 等 DMA 完成超时 */

/* 上电/复位时序（ms） */
#define OLED_POWERUP_MS         100     /* VDD 上升后的建立时间 */
#define OLED_RESET_LOW_MS       80      /* RST 低电平 */
#define OLED_RESET_HIGH_MS      100     /* 释放复位后等内部 LDO/振荡器 */
#define OLED_PUMP_MS            100     /* 电荷泵建立 VCOM */

static osSemaphoreId_t oled_dma_sem;

/* 清屏用的 128 字节全 0（放 Flash，不占 RAM） */
static const uint8_t oled_zero_page[OLED_W] = {0};

/* 配置序列：故意不含 0xAF（开显示），开显示要等 GDDRAM 清干净 */
static const uint8_t oled_cfg_seq[] =
{
    0xAE,               /* 关显示 */
    0xD5, 0x80,         /* 时钟分频 / 振荡频率 */
    0xA8, 0x3F,         /* 复用率 1/64 */
    0xD3, 0x00,         /* 显示偏移 0 */
    0x40,               /* 起始行 0 */
    0x8D, 0x14,         /* 电荷泵使能 */
    0x20, 0x00,         /* 水平寻址模式 */
    0xA1,               /* 段重映射：左右方向 */
    0xC8,               /* COM 扫描方向：上下方向 */
    0xDA, 0x12,         /* COM 引脚配置 */
    0x81, 0xCF,         /* 对比度 */
    0xD9, 0xF1,         /* 预充电周期 */
    0xDB, 0x40,         /* VCOMH */
    0xA4,               /* 显示跟随 RAM 内容 */
    0xA6                /* 正常显示（非反白） */
};

static const uint8_t oled_on_seq[] = { 0xAF };   /* 开显示 */

/* 一条命令序列（DC=0）一次发完；失败则复位 SPI 状态机 */
static uint8_t OLED_CmdSeq(const uint8_t* cmd, uint8_t len)
{
    HAL_StatusTypeDef st;

    OLED_DC_Clr();
    OLED_CS_Clr();
    st = HAL_SPI_Transmit(&hspi1, (uint8_t*)cmd, len, OLED_SPI_TIMEOUT_MS);
    OLED_CS_Set();
    OLED_DC_Set();

    if (st != HAL_OK)
    {
        (void)HAL_SPI_Abort(&hspi1);    /* 清掉 BUSY/错误状态，保证下次能发 */
        return 0;
    }

    return 1;
}

/* 水平寻址模式下设置写入窗口 */
static uint8_t OLED_SetWindow(uint8_t col0, uint8_t col1, uint8_t page0, uint8_t page1)
{
    uint8_t cmd[6];

    cmd[0] = 0x21;          /* Set Column Address */
    cmd[1] = col0;
    cmd[2] = col1;
    cmd[3] = 0x22;          /* Set Page Address */
    cmd[4] = page0;
    cmd[5] = page1;

    return OLED_CmdSeq(cmd, sizeof(cmd));
}

uint8_t OLED_Clear(void)
{
    uint8_t i;
    uint8_t ok = 1;

    ok &= OLED_SetWindow(0, OLED_W - 1, 0, OLED_PAGES - 1);

    OLED_DC_Set();
    OLED_CS_Clr();
    for (i = 0; i < OLED_PAGES; i++)
    {
        if (HAL_SPI_Transmit(&hspi1, (uint8_t*)oled_zero_page, OLED_W,
                             OLED_SPI_TIMEOUT_MS) != HAL_OK)
        {
            ok = 0;
            break;
        }
    }
    OLED_CS_Set();
    OLED_DC_Set();

    if (!ok)
    {
        (void)HAL_SPI_Abort(&hspi1);
        return 0;
    }

    return 1;
}

void OLED_Init(void)
{
    uint8_t i;

    oled_dma_sem = osSemaphoreNew(1, 0, NULL);

    /* 1) 上电建立时间：VDD 还在爬升时发的命令会被丢掉 */
    osDelay(OLED_POWERUP_MS);

    /* 2) 硬复位（datasheet 只要求 3us，这里给足余量） */
    OLED_RST_Set();
    osDelay(10);
    OLED_RST_Clr();
    osDelay(OLED_RESET_LOW_MS);
    OLED_RST_Set();
    osDelay(OLED_RESET_HIGH_MS);

    /* 3) 配置 + 电荷泵（显示仍关着），失败重试一次 */
    for (i = 0; i < 2; i++)
    {
        if (OLED_CmdSeq(oled_cfg_seq, (uint8_t)sizeof(oled_cfg_seq)))
        {
            break;
        }
        osDelay(20);
    }

    /* 4) 等电荷泵把 VCOM 建起来 */
    osDelay(OLED_PUMP_MS);

    /* 5) 清 GDDRAM：显示关着，这一步不会闪花屏 */
    (void)OLED_Clear();

    /* 6) 最后才开显示 */
    (void)OLED_CmdSeq(oled_on_seq, (uint8_t)sizeof(oled_on_seq));
    osDelay(20);
}

/*
 * 运行时重新同步：只重发寄存器配置，不动 0xAE/0xAF，也不清 GDDRAM。
 * 整帧数据由调用方随后整屏重写一遍覆盖掉，所以屏幕上不会出现黑屏闪过。
 */
uint8_t OLED_Resync(const uint8_t* full_fb)
{
    uint8_t ok = 1;

    (void)HAL_SPI_Abort(&hspi1);

    /* oled_cfg_seq 的第 0 个字节是 0xAE，这里跳过它 */
    ok &= OLED_CmdSeq(&oled_cfg_seq[1], (uint8_t)(sizeof(oled_cfg_seq) - 1));

    ok &= OLED_FlushWindow(0, OLED_PAGES - 1, full_fb);

    /* 补一条 0xAF：面板本来就亮着时这条是空操作（不会闪），
       只有上电初始化丢掉、显示还关着的情况下才会真正起作用 */
    ok &= OLED_CmdSeq(oled_on_seq, (uint8_t)sizeof(oled_on_seq));

    return ok;
}

uint8_t OLED_FlushWindow(uint8_t page0, uint8_t page1, const uint8_t* data)
{
    uint16_t len;

    if ((page0 > page1) || (page1 >= OLED_PAGES))
    {
        return 0;
    }

    len = (uint16_t)(page1 - page0 + 1) * OLED_W;

    if (!OLED_SetWindow(0, OLED_W - 1, page0, page1))
    {
        return 0;
    }

    /* 丢掉可能残留的计数，保证下面一定等到"本次"传输完成 */
    while (osSemaphoreAcquire(oled_dma_sem, 0) == osOK)
    {
    }

    OLED_DC_Set();
    OLED_CS_Clr();

    if (HAL_SPI_Transmit_DMA(&hspi1, (uint8_t*)data, len) != HAL_OK)
    {
        OLED_CS_Set();
        OLED_DC_Set();
        (void)HAL_SPI_Abort(&hspi1);
        return 0;
    }

    if (osSemaphoreAcquire(oled_dma_sem, OLED_DMA_TIMEOUT_MS) != osOK)
    {
        /* 超时：中断可能丢了，中止 DMA 复位 SPI，下一帧重来（不能死等） */
        (void)HAL_SPI_Abort(&hspi1);
        OLED_CS_Set();
        OLED_DC_Set();
        return 0;
    }

    OLED_CS_Set();
    OLED_DC_Set();
    return 1;
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi)
{
    if (hspi == &hspi1)
    {
        osSemaphoreRelease(oled_dma_sem);
    }
}
