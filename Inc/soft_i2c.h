/**
  ******************************************************************************
  * @file    soft_i2c.h
  * @brief   软件 I2C（GPIO 模拟）驱动头文件
  *
  *  使用两根空闲 GPIO 引脚模拟 I2C 协议，适用于低速 I2C 设备。
  *  本实现用于 SGP30 传感器，与硬件 I2C1（PB7/PB8）完全隔离。
  *
  *  【引脚分配】
  *    SCL — PA12  (GPIOA, PIN_12)
  *    SDA — PA15  (GPIOA, PIN_15)
  *
  *  【接线】
  *    Nucleo-WL55JC    SGP30
  *    PA12         →   SCL
  *    PA15         →   SDA
  *    3.3V         →   VCC
  *    GND          →   GND
  *
  *  @note 需要 4.7kΩ 上拉电阻（PA12/PA15 内部上拉约 40kΩ，短线可用）
  ******************************************************************************
 */

#ifndef __SOFT_I2C_H__
#define __SOFT_I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ==========================================================================
 *  引脚定义
 * ==========================================================================*/
#define SOFT_I2C_SCL_PORT       GPIOA
#define SOFT_I2C_SCL_PIN        GPIO_PIN_12
#define SOFT_I2C_SDA_PORT       GPIOA
#define SOFT_I2C_SDA_PIN        GPIO_PIN_15

/* ==========================================================================
 *  延时宏（非精确微秒延时，48MHz → 约 1~2μs）
 * ==========================================================================*/
#define SOFT_I2C_DELAY()        do { volatile uint32_t _i; for (_i = 0; _i < 100; _i++) { __NOP(); } } while (0)

/* ==========================================================================
 *  函数声明
 * ==========================================================================*/
void    SOFT_I2C_Init(void);                    /* 初始化引脚为开漏输出 */
void    SOFT_I2C_Start(void);                   /* 产生 START 条件 */
void    SOFT_I2C_Stop(void);                    /* 产生 STOP 条件 */
uint8_t SOFT_I2C_SendByte(uint8_t data);        /* 发送 1 字节，返回 ACK(0) / NACK(1) */
uint8_t SOFT_I2C_RecvByte(uint8_t ack);         /* 接收 1 字节，ack=0 发 ACK, ack=1 发 NACK */

/* 便捷封装：发送命令 + 读取数据，含设备地址 */
uint8_t SOFT_I2C_WriteCmd(uint8_t dev_addr, uint16_t cmd, uint8_t cmd_len);
uint8_t SOFT_I2C_ReadData(uint8_t dev_addr, uint8_t *buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_I2C_H__ */