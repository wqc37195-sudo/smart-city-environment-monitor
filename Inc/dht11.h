/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dht11.h
  * @brief   DHT11 温湿度传感器驱动头文件
  *
  *  【协议】单总线（Single Bus），数据引脚 PB3
  *  【数据格式】40bit = 16bit湿度 + 16bit温度 + 8bit校验和
  *  【精度】温度 ±2°C，湿度 ±5%RH，采样间隔 ≥1秒
  *  【注意】DHT11 的小数部分始终为 0（只有整数精度）
  *         DHT22 才有小数精度，此驱动同样适用于 DHT22
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __DHT11_H__
#define __DHT11_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* DHT11 数据结构体 */
typedef struct {
    uint8_t humidity_int;       /* [IN]  湿度整数部分       (0~100%)         */
    uint8_t humidity_dec;       /* [IN]  湿度小数部分       (DHT11恒为0)     */
    uint8_t temperature_int;    /* [IN]  温度整数部分       (单位°C)         */
    uint8_t temperature_dec;    /* [IN]  温度小数部分       (DHT11恒为0)     */
    uint8_t checksum;           /* [IN]  校验和 = byte0+1+2+3 的低8位        */
    uint8_t err;                /* [OUT] 错误码: 0=成功, 1=超时, 2=校验失败  */
} DHT11_Data;

/* 函数声明 */
void    DHT11_Init(void);           /* 初始化 DHT11（使能 DWT 延时） */
uint8_t DHT11_Read(DHT11_Data *data);/* 读取温湿度：返回 0=成功 */

#ifdef __cplusplus
}
#endif

#endif /* __DHT11_H__ */
