/**
  ******************************************************************************
  * @file    lora_tx.c
  * @brief   STM32WL55 板载 LoRa 发送驱动实现
  *
  *  使用 ST SubGHz_Phy Radio 中间件发送 LoRa 数据包。
  *  参数由 main.h 中的宏控制：
  *    - RF_FREQUENCY         = 433000000 (433MHz)
  *    - LORA_SPREADING_FACTOR = LORA_SF7
  *    - LORA_BANDWIDTH       = 0 (= LORA_BW_125/125kHz, Bandwidths[] index)
  *    - LORA_CODING_RATE     = LORA_CR_4_5
  *    - LORA_CRC_ENABLE      = 1
  *
  *  依赖：须先调用 MX_SubGHz_Phy_Init() 完成 Radio 中间件初始化。
  ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "lora_tx.h"
#include "radio.h"
#include "radio_driver.h"

/* Extern variables ----------------------------------------------------------*/
/* s_radio_tx_busy / s_radio_tx_ok 定义在 subghz_phy_app.c */

/* Private define ------------------------------------------------------------*/
#define LORA_TX_POWER_DBM      14          /* 输出功率 (dBm)，匹配 CubeMX 配置 */
#define LORA_TX_PREAMBLE_LEN   8           /* 前导码长度 (symbols) */
#define LORA_TX_TIMEOUT_MS     5000        /* 发送超时 (ms) */

/* Private variables ---------------------------------------------------------*/
static uint8_t s_tx_buf[256];              /* Radio.Send 需要非常量指针 */

/* Exported functions ---------------------------------------------------------*/

/**
  * @brief  初始化 LoRa 发送参数（仅配置 Radio 参数，不发送）。
  *         需在 MX_SubGHz_Phy_Init() 之后调用。
  */
void LoRa_TX_Init(void)
{
    /*
     * 配置发送参数：
     *   modem       = MODEM_LORA
     *   power       = 14 dBm
     *   fdev        = 0 (LoRa 不使用)
     *   bandwidth   = Bandwidths[0] = LORA_BW_125 (125kHz)
     *   datarate    = LORA_SF7 → 0x07 (128 chips/symbol)
     *   coderate    = LORA_CR_4_5 → 0x01
     *   preambleLen = 8 symbols
     *   fixLen      = false (变长包头)
     *   crcOn       = true (开启 CRC)
     *   freqHopOn   = false
     *   hopPeriod   = 0
     *   iqInverted  = false
     *   timeout     = 5000 ms
     */
    Radio.SetTxConfig(MODEM_LORA,
                      LORA_TX_POWER_DBM,
                      0,                              /* fdev */
                      LORA_BANDWIDTH,                 /* 125kHz */
                      LORA_SPREADING_FACTOR,          /* SF7 */
                      LORA_CODING_RATE,               /* CR 4/5 */
                      LORA_TX_PREAMBLE_LEN,
                      false,                          /* fixLen */
                      (LORA_CRC_ENABLE != 0) ? true : false,  /* crcOn */
                      false,                          /* freqHopOn */
                      0,                              /* hopPeriod */
                      false,                          /* iqInverted */
                      LORA_TX_TIMEOUT_MS);
}

/**
  * @brief  发送一帧 LoRa 数据。
  *         阻塞等待发送完成或超时。
  * @param  data  待发送数据缓冲区
  * @param  len   数据长度（字节）
  * @retval 1 发送成功；0 发送失败或超时
  */
uint8_t LoRa_TX_Send(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0)
    {
        return 0;
    }

    if (len > sizeof(s_tx_buf))
    {
        len = sizeof(s_tx_buf);
    }

    /* 如果上次发送尚未完成，轮询等待 */
    uint32_t wait_start = HAL_GetTick();
    while (s_radio_tx_busy)
    {
        if (HAL_GetTick() - wait_start > LORA_TX_TIMEOUT_MS)
        {
            return 0;   /* 前次发送超时 */
        }
    }

    uint8_t send_len = (len > 255) ? 255 : (uint8_t)len;
    for (uint16_t i = 0; i < send_len; i++)
    {
        s_tx_buf[i] = data[i];
    }

    /* 设置射频频率 */
    Radio.SetChannel(RF_FREQUENCY);

    /* 重置并设置发送标志 */
    s_radio_tx_ok   = 0;
    s_radio_tx_busy = 1;

    /* 启动发送 */
    Radio.Send(s_tx_buf, send_len);

    /* 等待发送完成（回调清除 s_radio_tx_busy） */
    wait_start = HAL_GetTick();
    while (s_radio_tx_busy)
    {
        if (HAL_GetTick() - wait_start > LORA_TX_TIMEOUT_MS)
        {
            /* 超时 — 强行停止并返回失败 */
            Radio.Standby();
            s_radio_tx_busy = 0;
            s_radio_tx_ok   = 0;
            return 0;
        }
    }

    return s_radio_tx_ok;
}

/**
  * @brief  查询 LoRa 是否正在发送。
  * @retval 1 正在发送；0 空闲
  */
uint8_t LoRa_TX_IsBusy(void)
{
    return s_radio_tx_busy;
}
