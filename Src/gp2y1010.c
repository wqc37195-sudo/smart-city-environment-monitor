/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gp2y1010.c
  * @brief   GP2Y1010AU0F 粉尘传感器驱动实现
  *
  *  ============================= 工作原理 =============================
  *
  *  GP2Y1010 内部有一个红外发光二极管和一个光电二极管。
  *  红外光照射到空气中的悬浮颗粒物会产生散射，
  *  光电二极管接收散射光并转换为电压输出。
  *
  *  V-LED 引脚需要脉冲驱动才能正常工作（非连续点亮），
  *  典型脉冲宽度 280μs，脉冲间隔 ≥10ms。
  *
  *  ============================= 时序 =============================
  *
  *  单个测量脉冲时序：
  *
  *   V-LED ──┐                    ┌────
  *           │                    │
  *           └────────────────────┘
  *           0    280us   320us   t(us)
  *                    ↓
  *               ADC 采样点
  *
  *  此时 Vo 引脚输出的电压与粉尘浓度成正比。
  *
  *  ============================= 公式 =============================
  *
  *  输出电压  Vout = ADC × 3.3V / 4096   (12-bit ADC, 3.3V参考)
  *
  *  粉尘浓度  density (μg/m³):
  *     当 Vout < 0.6V 时 → 0（低于检测阈值）
  *     当 Vout ≥ 0.6V 时 → 200 × Vout - 120
  *
  *  本驱动采样 10 次取平均，减少随机噪声。
  *
  *  ==================================================================
  *
  *  接线：
  *    GP2Y1010    Nucleo-WL55JC
  *    Pin6 VCC →  5V（推荐）
  *    Pin5 Vo  →  PB13 (ADC1_IN0)
  *    Pin1 V-LED → PB1  + 150Ω → V-LED
  *    GND     →  GND
  *    注意：V-LED 和 GND 之间需接 220μF 电解电容
  ******************************************************************************
 */
/* USER CODE END Header */

#include "gp2y1010.h"

/* 外部 ADC 句柄（由 CubeMX 在 adc.c 中初始化） */
extern ADC_HandleTypeDef hadc;

/* 引脚宏定义（与 CubeMX 生成的 main.h 一致） */
#define LED_PIN     GP2Y1010_LED_Pin       /* PB1 */
#define LED_PORT    GP2Y1010_LED_GPIO_Port /* GPIOB */

/**
  * @brief  微秒级延时（使用 DWT 周期计数器）
  * @note   与 dht11.c 共用 DWT，需确保 DWT 已被 DHT11_Init 使能
  */
static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * 48;  /* 48MHz = 48 ticks/us */
    while ((DWT->CYCCNT - start) < ticks);
}

/**
  * @brief  执行一次单次 ADC 转换并读取结果
  * @retval 12-bit ADC 值 (0~4095)
  */
static uint16_t ADC_Read(void)
{
    uint32_t val = 0;

    /* 启动 ADC 转换 */
    HAL_ADC_Start(&hadc);

    /* 等待转换完成 */
    if (HAL_ADC_PollForConversion(&hadc, 10) == HAL_OK)
    {
        val = HAL_ADC_GetValue(&hadc);
    }

    /* 停止 ADC */
    HAL_ADC_Stop(&hadc);

    return (uint16_t)val;
}

/**
  * @brief  执行一次完整的 GP2Y1010 脉冲采样
  *
  *  时序：
  *    1. 熄灭 LED（V-LED = HIGH）
  *    2. 点亮 LED（V-LED = LOW）
  *    3. 等待 280μs（Vo 稳定到散射光水平）
  *    4. 读取 ADC
  *    5. 熄灭 LED（V-LED = HIGH）
  *    6. 等待 40μs
  *
  *  @retval 本次采样的 12-bit ADC 值
  */
static uint16_t GP2Y1010_SampleOnce(void)
{
    /* 熄灭 LED（高电平熄灭，低电平点亮） */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    delay_us(10);  /* 确保 LED 稳定熄灭 */

    /* 点亮 LED */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
    delay_us(280); /* 等待 Vo 稳定到峰值 */

    /* 读取 ADC */
    uint16_t adc_val = ADC_Read();

    /* 熄灭 LED */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
    delay_us(40);  /* 等待脉冲结束 */

    return adc_val;
}

/**
  * @brief  将 ADC 值转换为粉尘浓度
  *
  *  公式推导：
  *    1. Vout = ADC × 3.3 / 4096
  *    2. density = 200 × Vout - 120  (当 Vout ≥ 0.6V)
  *
  *  @param  adc_val : 12-bit ADC 原始值
  *  @return 粉尘浓度 (μg/m³)，低于阈值返回 0
  */
static uint16_t ADC_ToDensity(uint16_t adc_val)
{
    /* ADC 值转电压 (mV) */
    uint32_t v_mv = (uint32_t)adc_val * 3300 / 4096;

    /* 低于检测阈值 (600mV)，返回 0 */
    if (v_mv < 600)
        return 0;

    /* density = 200 × (v/1000) - 120 = v/5 - 120 */
    uint32_t density = v_mv / 5;
    if (density > 120)
        density -= 120;
    else
        density = 0;

    if (density > 65535) density = 65535;
    return (uint16_t)density;
}

/* ---------------------------------------------------------------------------
 * 公开接口
 * -------------------------------------------------------------------------*/

/**
  * @brief  初始化 GP2Y1010
  *         确保 LED 初始为熄灭状态（V-LED = HIGH）
  *         并在初始化时做一次启动测量（让传感器退出待机）
  */
void GP2Y1010_Init(void)
{
    /* 初始熄灭 LED */
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);

    /* 执行一次空采样，让传感器进入工作状态 */
    GP2Y1010_SampleOnce();
    HAL_Delay(20);
}

/**
  * @brief  读取粉尘浓度
  *
  *  连续采样 10 次，每次间隔约 10ms，
  *  去掉最大值和最小值后取平均。
  *
  *  @param  data : 输出数据缓冲区
  *  @retval 0=成功
  */
uint8_t GP2Y1010_Read(GP2Y1010_Data *data)
{
    uint16_t samples[10];
    uint32_t sum = 0;
    uint16_t min_val = 4095;
    uint16_t max_val = 0;

    if (data == NULL) return 1;

    /* 采样 10 次 */
    for (int i = 0; i < 10; i++)
    {
        samples[i] = GP2Y1010_SampleOnce();
        if (samples[i] < min_val) min_val = samples[i];
        if (samples[i] > max_val) max_val = samples[i];
        HAL_Delay(10);  /* 脉冲间隔 ≥10ms */
    }

    /* 去掉最大最小值后求和（统计实际剔除数量） */
    uint8_t valid_cnt = 0;
    for (int i = 0; i < 10; i++)
    {
        if (samples[i] != min_val && samples[i] != max_val)
        {
            sum += samples[i];
            valid_cnt++;
        }
    }
    uint16_t avg_adc = (valid_cnt > 0) ? (uint16_t)(sum / valid_cnt) : (uint16_t)(sum / 10);

    /* 计算输出电压 */
    data->voltage = (float)avg_adc * 3.3f / 4096.0f;

    /* 转换为粉尘浓度 */
    data->density = ADC_ToDensity(avg_adc);

    data->err = 0;
    return 0;
}
