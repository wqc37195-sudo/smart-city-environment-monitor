/**
  ******************************************************************************
  * @file    mic.c
  * @brief   MAX9814/MAX4466 模拟麦克风驱动实现
  *
  *  ============================= 测量原理 =============================
  *
  *  麦克风模块输出模拟电压，振幅随声音强度变化。
  *  单次 ADC 读数只能反映瞬时值，无法衡量"响度"。
  *
  *  本驱动采用"快速突发采样"：
  *    1. 连续采集 MIC_SAMPLE_COUNT 个 ADC 样本
  *    2. 每个样本间隔 MIC_SAMPLE_INTERVAL_US 微秒
  *    3. 记录最大值和最小值
  *    4. 峰峰值 (max-min) 作为声音强度代理指标
  *    5. 计算平均电压用于参考
  *
  *  总采样耗时 = MIC_SAMPLE_COUNT × MIC_SAMPLE_INTERVAL_US
  *             = 32 × 200us = 6.4ms
  *
  *  ============================= 硬件 =============================
  *
  *  接线：
  *    MAX9814/4466    Nucleo-WL55JC
  *    VCC         →   3.3V
  *    GND         →   GND
  *    OUT         →   PB12 (ADC1_IN1)
  *
  *  @note ADC 为 GP2Y1010 和麦克风共享，每次读取前需切换通道
  ******************************************************************************
 */

#include "mic.h"
#include <stdio.h>

/* 外部 ADC 句柄（由 CubeMX 在 adc.c 中初始化） */
extern ADC_HandleTypeDef hadc;

/* ---------------------------------------------------------------------------
 * 模块1：ADC 通道切换 + 单次转换
 * =========================================================================
 * 由于 ADC 被 GP2Y1010 和麦克风共享，每次读取前需将通道切换到 IN1。
 * -------------------------------------------------------------------------*/

/**
  * @brief  执行一次 ADC 单次转换（切换到麦克风通道）
  * @param  none
  * @retval 12-bit ADC 值 (0~4095)，失败返回 0
  */
static uint16_t ADC_ReadOnce(void)
{
    uint16_t val = 0;

    /* 配置 ADC 通道为 MIC 对应的通道（IN1） */
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = MIC_ADC_CHANNEL;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;  /* 使用 Common 采样时间 */
    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
    {
        return 0;
    }

    /* 启动 ADC 转换 */
    HAL_ADC_Start(&hadc);

    /* 等待转换完成 */
    if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK)
    {
        val = (uint16_t)HAL_ADC_GetValue(&hadc);
    }

    /* 停止 ADC */
    HAL_ADC_Stop(&hadc);

    return val;
}

/* ---------------------------------------------------------------------------
 * 模块2：微秒延时（使用 DWT，与 dht11.c 共享）
 * =========================================================================
 * 如果 DHT11 已经使能了 DWT，直接使用。
 * 否则独立使能。
 * -------------------------------------------------------------------------*/

/* DWT 初始化标志（只初始化一次） */
static uint8_t s_dwt_inited = 0;

/**
  * @brief  确保 DWT 周期计数器已使能
  */
static void DWT_EnsureInit(void)
{
    if (!s_dwt_inited)
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        s_dwt_inited = 1;
    }
}

/**
  * @brief  微秒级延时（忙等待）
  * @param  us : 延时微秒数（系统 48MHz → 1us = 48 ticks）
  */
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * 48;
    while ((DWT->CYCCNT - start) < ticks);
}

/* ---------------------------------------------------------------------------
 * 模块3：公开接口
 * -------------------------------------------------------------------------*/

/**
  * @brief  初始化麦克风
  *
  *  本函数负责：
  *    1. 确保 DWT 周期计数器已使能（用于微秒延时）
  *    2. PB12 的 GPIO 模拟模式在 HAL_ADC_MspInit 中配置
  *
  *  @note 须在 MX_ADC_Init() 之后调用
  */
void MIC_Init(void)
{
    /* 确保 DWT 可用（如果 DHT11 已经初始化，此操作是幂等的） */
    DWT_EnsureInit();

    /* GPIO 配置在 adc.c 的 HAL_ADC_MspInit 中完成 */
    /* 触发一次空读，确保 ADC 通道稳定 */
    ADC_ReadOnce();
}

/**
  * @brief  读取环境声音强度
  *
  *  通过快速突发采样计算声音强度：
  *    - 连续采样 MIC_SAMPLE_COUNT（32）次
  *    - 每次间隔 MIC_SAMPLE_INTERVAL_US（200us）
  *    - 总耗时约 6.4ms
  *
  *  @param  data : 输出数据结构体
  *  @retval 0=成功, 1=错误
  */
uint8_t MIC_Read(MIC_Data *data)
{
    uint32_t sum = 0;
    uint16_t min_val = 4095;
    uint16_t max_val = 0;

    if (data == NULL) return 1;

    /* 突发采样 */
    for (int i = 0; i < MIC_SAMPLE_COUNT; i++)
    {
        uint16_t sample = ADC_ReadOnce();

        if (i == 0 && sample == 0)
        {
            /* 第一次采样就失败，可能是 ADC 通道配置问题 */
            data->err = 1;
            return 1;
        }

        /* 更新统计 */
        if (sample < min_val) min_val = sample;
        if (sample > max_val) max_val = sample;
        sum += sample;

        /* 采样间隔 */
        if (i < MIC_SAMPLE_COUNT - 1)
        {
            delay_us(MIC_SAMPLE_INTERVAL_US);
        }
    }

    /* 计算结果 */
    data->raw_max = max_val;
    data->raw_min = min_val;
    data->raw_avg = (uint16_t)(sum / MIC_SAMPLE_COUNT);
    data->level   = max_val - min_val;   /* 峰峰值 = 声音强度 */

    /* 噪声门限：峰峰值低于阈值视为静音 */
    if (data->level < MIC_NOISE_THRESHOLD)
    {
        data->level     = 0;
        data->level_pct = 0;
    }
    else
    {
        /* 声音强度百分比 */
        uint32_t pct = (uint32_t)data->level * 100 / 2048;
        data->level_pct = (pct > 100) ? 100 : (uint16_t)pct;
    }

    /* 平均电压 (mV) */
    data->voltage_mv = (uint32_t)data->raw_avg * MIC_ADC_REFERENCE_MV / MIC_ADC_RESOLUTION;

    data->err = 0;
    return 0;
}
