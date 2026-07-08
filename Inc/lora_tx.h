/**
  ******************************************************************************
  * @file    lora_tx.h
  * @brief   STM32WL55 板载 LoRa 发送驱动
  *
  *  使用 ST SubGHz_Phy Radio 中间件发送 LoRa 数据包
  *  频率 470MHz，SF7，BW125kHz，CR4/5（见 main.h 中的硬件配置宏）
  *
  *  需在 CubeMX 中配置 SUBGHZ → LoRa + Low Band
  *  需调用 MX_SubGHz_Phy_Init() 后再调用 LoRa_TX_Init()
  ******************************************************************************
 */
#ifndef __LORA_TX_H__
#define __LORA_TX_H__

#include "main.h"
#include "radio.h"

/* ====== 射频发送状态标志（定义在 subghz_phy_app.c） ====== */
extern volatile uint8_t s_radio_tx_busy;  /* 1=正在发送 */
extern volatile uint8_t s_radio_tx_ok;    /* 1=发送成功, 0=超时/错误 */

void LoRa_TX_Init(void);                    /* 初始化 LoRa 发送（配置 Radio 参数） */
uint8_t LoRa_TX_Send(const uint8_t *data, uint16_t len); /* 发送数据包（1=成功） */
uint8_t LoRa_TX_IsBusy(void);               /* 查询是否正在发送（1=忙，0=空闲） */

#endif
