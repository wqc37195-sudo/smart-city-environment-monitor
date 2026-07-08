/**
  ******************************************************************************
  * @file    mic.h
  * @brief   MAX9814/MAX4466 模拟麦克风驱动头文件
  *
  *  ============================= 工作原理 =============================
  *
  *  MAX9814 / MAX4466 是带 AGC/前置放大器的驻极体麦克风模块。
  *  输出为模拟电压，随环境声音强度波动，中心电压约 VCC/2。
  *
  *  【测量方法】
  *  本驱动采用"快速突发采样"方式：
  *    1. 连续采样 MIC_SAMPLE_COUNT 次，每次间隔 MIC_SAMPLE_INTERVAL_US
  *    2. 记录采样中的最大值和最小值
  *    3. 峰峰值 (max-min) 作为当前声音强度的代理指标
  *    4. 同时计算平均电压（mV）
  *
  *  【接线】
  *    MAX9814/4466    Nucleo-WL55JC
  *    VCC         →   3.3V
  *    GND         →   GND
  *    OUT         →   PB12 (ADC1_IN1)
  *
  *  @note 依赖 ADC（hadc），须在 MX_ADC_Init() 之后调用 MIC_Init()
  *        依赖 DWT 微秒计数器（由 dht11.c 初始化，或在 MIC_Init 中自初始化）
  ******************************************************************************
  */

#ifndef __MIC_H__
#define __MIC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ==========================================================================
 *  采样参数
 * ==========================================================================*/
#define MIC_SAMPLE_COUNT        32      /* 每次读取采样的次数 */
#define MIC_SAMPLE_INTERVAL_US  200     /* 采样间隔 (微秒) — 总耗时约 6.4ms */

/**
 * @brief 噪声门限（ADC 峰峰值）
 *   MAX9814 60dB 增益下，静音时 ADC 峰峰值通常在 5~30 之间。
 *   低于此值视为静音，输出 level=0，避免显示随机噪声。
 */
#define MIC_NOISE_THRESHOLD     15
#define MIC_ADC_CHANNEL         ADC_CHANNEL_1   /* PB12 = ADC1_IN1 */
#define MIC_ADC_RESOLUTION      4096    /* 12-bit ADC */
#define MIC_ADC_REFERENCE_MV    3300    /* ADC 参考电压 (mV) */

/* ==========================================================================
 *  麦克风数据结构
 * ==========================================================================*/
typedef struct {
    uint16_t raw_max;           /* 本次采样的最大 ADC 值 (0~4095) */
    uint16_t raw_min;           /* 本次采样的最小 ADC 值 (0~4095) */
    uint16_t raw_avg;           /* 本次采样的平均 ADC 值 */
    uint16_t level;             /* 声音强度 = raw_max - raw_min（峰峰值）*/
    uint32_t voltage_mv;        /* 平均电压 (mV) */
    uint16_t level_pct;         /* 声音强度百分比 (0~100%)，归一化到 VCC */
    uint8_t  err;               /* 错误标志 */
} MIC_Data;

/* ==========================================================================
 *  函数声明
 * ==========================================================================*/
void    MIC_Init(void);                     /* 初始化麦克风（使能 ADC 通道 GPIO） */
uint8_t MIC_Read(MIC_Data *data);           /* 读取声音强度（突发采样） */

#ifdef __cplusplus
}
#endif

#endif /* __MIC_H__ */
