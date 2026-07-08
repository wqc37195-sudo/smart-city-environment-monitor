/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gp2y1010.h
  * @brief   GP2Y1010AU0F 粉尘传感器驱动头文件
  *
  *  【芯片】Sharp GP2Y1010AU0F 光学粉尘传感器
  *  【原理】红外LED照射空气中的颗粒物，光敏管接收散射光
  *         输出电压与粉尘浓度成正比
  *  【接口】模拟输出（Vo）+ 数字控制（V-LED）
  *
  *  【引脚】
  *    Vo    → PB13 (ADC1_IN0) — 模拟电压输出
  *    V-LED → PB1  (GPIO)      — LED 控制（低电平点亮）
  *
  *  【测量时序】
  *    1. V-LED = HIGH（熄灭LED，准备开始）
  *    2. V-LED = LOW（点亮LED）
  *    3. 等待 280μs（LED稳定，粉尘散射光达到峰值）
  *    4. 读取 ADC 值
  *    5. V-LED = HIGH（熄灭LED）
  *    6. 等待 40μs
  *    7. 重复 10 次取平均
  *
  *  【浓度换算】
  *    Vout = ADC × 3.3V / 4096
  *    浓度 (μg/m³) = 200 × Vout - 120  (Vout > 0.6V 时)
  *
  *  @note 测量间隔建议 ≥10ms，本驱动内部做了多次采样平均
  ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __GP2Y1010_H__
#define __GP2Y1010_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** GP2Y1010 数据结构体 */
typedef struct {
    float    voltage;      /* 输出电压 (V) */
    uint16_t density;      /* 粉尘浓度 (μg/m³) */
    uint8_t  err;          /* 错误码：0=成功 */
} GP2Y1010_Data;

/* 函数声明 */
void    GP2Y1010_Init(void);                 /* 初始化：确保 LED 初始熄灭 */
uint8_t GP2Y1010_Read(GP2Y1010_Data *data);  /* 读取粉尘浓度（多次采样取平均） */

#ifdef __cplusplus
}
#endif

#endif /* __GP2Y1010_H__ */
