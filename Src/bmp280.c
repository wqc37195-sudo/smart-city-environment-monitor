/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bmp280.c
  * @brief   BMP280 大气压强传感器驱动实现
  *
  *  ============================= 工作原理 =============================
  *
  *  【测量流程】
  *  1. 初始化阶段
  *     a) 读取芯片 ID (0xD0) → 应为 0x58，确认传感器存在
  *     b) 读取校准系数 (0x88~0x9F, 24 字节)
  *     c) 配置滤波系数
  *
  *  2. 数据采集阶段（每次读取独立触发）
  *     a) 写 CTRL_MEAS 寄存器触发 Forced 模式测量
  *     b) 等待测量完成（×1 过采样约 7ms）
  *     c) 读取 6 字节原始数据（气压 3 字节 + 温度 3 字节）
  *     d) 用 Bosch 补偿公式计算真实值
  *
  *  【校准系数】
  *  每个 BMP280 芯片出厂时都有唯一的校准系数，存储在 OTP 中。
  *  上电后必须通过 I2C 读取并保存，补偿公式依赖这些系数。
  *
  *  【补偿算法】
  *  温度补偿使用 32 位整数运算，气压补偿使用 64 位整数运算。
  *  公式来源：Bosch BMP280 数据手册 (BST-BMP280-DS001)
  *
  *  【Forced 模式 vs Normal 模式】
  *  Forced ：每次读取前触发一次测量，功耗更低，数据更新更可控
  *  Normal：持续自动测量，适合周期性轮询
  *  本驱动采用 Forced 模式以保证每次读数都是最新数据
  *
  *  ==================================================================
  *
  *  接线：
  *    BMP280      Nucleo-WL55JC
  *    VCC    →    3.3V
  *    GND    →    GND
  *    SDA    →    PB7 (I2C1_SDA) — 与 OLED + GY-30 并联
  *    SCL    →    PB8 (I2C1_SCL) — 与 OLED + GY-30 并联
  *
  *  @note 需要 CubeMX 已配置并初始化 I2C1（hi2c1）
  ******************************************************************************
 */
/* USER CODE END Header */

#include "bmp280.h"
#include <stdio.h>

/* 外部 I2C 句柄（由 CubeMX 在 i2c.c 中生成并初始化） */
extern I2C_HandleTypeDef hi2c1;

/* 校准系数（静态全局变量，所有函数共享） */
static BMP280_Calib s_calib = {0};

/* ---------------------------------------------------------------------------
 * 模块1：I2C 底层通信
 * =========================================================================
 * 使用 STM32 HAL 的 I2C 存储访问函数（Mem_Read / Mem_Write）
 * 先发送寄存器地址（8-bit），再读/写数据
 * -------------------------------------------------------------------------*/

/**
  * @brief  通过 I2C 读取 BMP280 寄存器
  * @param  reg : 寄存器地址（8-bit）
  * @param  buf : 数据接收缓冲区
  * @param  len : 读取字节数
  * @retval BMP280_OK=0 或 BMP280_ERR_I2C=1
  */
static uint8_t BMP280_ReadReg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    if (HAL_I2C_Mem_Read(&hi2c1, BMP280_ADDR_WRITE,
                         reg, I2C_MEMADD_SIZE_8BIT, buf, len,
                         HAL_MAX_DELAY) != HAL_OK)
    {
        return BMP280_ERR_I2C;
    }
    return BMP280_OK;
}

/**
  * @brief  通过 I2C 写入 BMP280 寄存器
  * @param  reg : 寄存器地址（8-bit）
  * @param  val : 要写入的值
  * @retval BMP280_OK=0 或 BMP280_ERR_I2C=1
  */
static uint8_t BMP280_WriteReg(uint8_t reg, uint8_t val)
{
    if (HAL_I2C_Mem_Write(&hi2c1, BMP280_ADDR_WRITE,
                          reg, I2C_MEMADD_SIZE_8BIT, &val, 1,
                          HAL_MAX_DELAY) != HAL_OK)
    {
        return BMP280_ERR_I2C;
    }
    return BMP280_OK;
}

/* ---------------------------------------------------------------------------
 * 模块2：校准系数读取
 * =========================================================================
 * BMP280 的校准系数存储在 0x88~0x9F（共 24 字节）：
 *
 *  地址     参数    类型    说明
 *  0x88~89  dig_T1  uint16  温度校准系数1
 *  0x8A~8B  dig_T2  int16   温度校准系数2
 *  0x8C~8D  dig_T3  int16   温度校准系数3
 *  0x8E~8F  dig_P1  uint16  气压校准系数1
 *  0x90~91  dig_P2  int16   气压校准系数2
 *  0x92~93  dig_P3  int16   气压校准系数3
 *  0x94~95  dig_P4  int16   气压校准系数4
 *  0x96~97  dig_P5  int16   气压校准系数5
 *  0x98~99  dig_P6  int16   气压校准系数6
 *  0x9A~9B  dig_P7  int16   气压校准系数7
 *  0x9C~9D  dig_P8  int16   气压校准系数8
 *  0x9E~9F  dig_P9  int16   气压校准系数9
 *
 *  数据格式：小端序（低字节在前）
 * -------------------------------------------------------------------------*/

static uint8_t BMP280_ReadCalibration(void)
{
    uint8_t buf[24] = {0};

    /* 一次突发读取 24 字节校准数据 */
    if (BMP280_ReadReg(BMP280_REG_CALIB_T1, buf, 24) != BMP280_OK)
        return BMP280_ERR_I2C;

    /* 解析温度校准系数（小端序） */
    s_calib.dig_T1 = (uint16_t)(buf[1] << 8) | buf[0];
    s_calib.dig_T2 = (int16_t)(buf[3] << 8)  | buf[2];
    s_calib.dig_T3 = (int16_t)(buf[5] << 8)  | buf[4];

    /* 解析气压校准系数（小端序） */
    s_calib.dig_P1 = (uint16_t)(buf[7] << 8) | buf[6];
    s_calib.dig_P2 = (int16_t)(buf[9] << 8)  | buf[8];
    s_calib.dig_P3 = (int16_t)(buf[11] << 8) | buf[10];
    s_calib.dig_P4 = (int16_t)(buf[13] << 8) | buf[12];
    s_calib.dig_P5 = (int16_t)(buf[15] << 8) | buf[14];
    s_calib.dig_P6 = (int16_t)(buf[17] << 8) | buf[16];
    s_calib.dig_P7 = (int16_t)(buf[19] << 8) | buf[18];
    s_calib.dig_P8 = (int16_t)(buf[21] << 8) | buf[20];
    s_calib.dig_P9 = (int16_t)(buf[23] << 8) | buf[22];

    return BMP280_OK;
}

/* ---------------------------------------------------------------------------
 * 模块3：温度补偿公式
 * =========================================================================
 *  Bosch 官方 32 位整数补偿算法
 *
 *  输入：adc_T — 20 位原始温度 ADC 值
 *  输出：温度值，单位 0.01°C（例如 2501 = 25.01°C）
 *  副作用：更新 s_calib.t_fine，供气压补偿使用
 *
 *  公式来源：BST-BMP280-DS001, Section 3.11.3
 * -------------------------------------------------------------------------*/

static int32_t BMP280_CompensateTemp(int32_t adc_T)
{
    int32_t var1, var2, T;

    /* var1 = ((ADC/8) - (T1*2)) * T2 / 2048 */
    var1 = ((((adc_T >> 3) - ((int32_t)s_calib.dig_T1 << 1))) *
            ((int32_t)s_calib.dig_T2)) >> 11;

    /* var2 = (((ADC/16 - T1)^2 / 4096) * T3) / 16384 */
    var2 = (((((adc_T >> 4) - ((int32_t)s_calib.dig_T1)) *
              ((adc_T >> 4) - ((int32_t)s_calib.dig_T1))) >> 12) *
            ((int32_t)s_calib.dig_T3)) >> 14;

    /* t_fine = var1 + var2（供气压补偿使用） */
    s_calib.t_fine = var1 + var2;

    /* T = (t_fine * 5 + 128) >> 8（结果为 0.01°C 单位） */
    T = (s_calib.t_fine * 5 + 128) >> 8;
    return T;
}

/* ---------------------------------------------------------------------------
 * 模块4：气压补偿公式
 * =========================================================================
 *  Bosch 官方 64 位整数补偿算法
 *
 *  输入：adc_P — 20 位原始气压 ADC 值
 *        依赖 s_calib.t_fine（由温度补偿函数更新）
 *  输出：气压值，单位 Pa
 *
 *  注意：Bosch 公式输出为 Q24.8 定点格式，
 *        需要除以 256 才能得到真实的 Pa 值。
 *
 *  公式来源：BST-BMP280-DS001, Section 3.11.3
 * -------------------------------------------------------------------------*/

static uint32_t BMP280_CompensatePress(int32_t adc_P)
{
    int64_t var1, var2, p;

    /* var1 = (t_fine / 2) - 64000 */
    var1 = ((int64_t)s_calib.t_fine) - 128000;

    /* var2 = var1^2 * P6 + var1*P5*2^17 + P4*2^35 */
    var2 = var1 * var1 * (int64_t)s_calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)s_calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)s_calib.dig_P4) << 35);

    /* var1 = (var1^2*P3/256 + var1*P2*4096) */
    var1 = ((var1 * var1 * (int64_t)s_calib.dig_P3) >> 8) +
           ((var1 * (int64_t)s_calib.dig_P2) << 12);

    /* var1 = (2^47 + var1) * P1 / 2^33 */
    var1 = (((((int64_t)1 << 47) + var1)) * ((int64_t)s_calib.dig_P1)) >> 33;

    /* 防除零 */
    if (var1 == 0)
        return 0;

    /* p = (2^20 - ADC) — 原始差值 */
    p = 1048576 - adc_P;

    /* p = ((p*2^31 - var2) * 3125) / var1 */
    p = (((p << 31) - var2) * 3125) / var1;

    /* 精细补偿：P9*p^2/2^25 + P8*p/2^19 */
    var1 = (((int64_t)s_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)s_calib.dig_P8) * p) >> 19;

    /* p = (p + var1 + var2) / 256 + P7 * 16 */
    p = ((p + var1 + var2) >> 8) + (((int64_t)s_calib.dig_P7) << 4);

    /* Bosch 公式输出为 Q24.8 格式，除以 256 得到 Pa */
    return (uint32_t)(p / 256);
}

/* ---------------------------------------------------------------------------
 * 模块5：公开接口
 * =========================================================================
 * 供 main.c 调用的初始化与数据读取函数
 * -------------------------------------------------------------------------*/

/**
  * @brief  初始化 BMP280 传感器
  *
  *  1. 读取芯片 ID (0xD0) → 验证为 0x58
  *  2. 读取 24 字节出厂校准系数
  *  3. 配置滤波器（IIR 系数 = 4）
  *
  *  @note 不在此处触发首次测量，由 ReadData 统一处理
  *  @retval BMP280_OK=0, BMP280_ERR_I2C=1, BMP280_ERR_ID=2
  */
uint8_t BMP280_Init(void)
{
    uint8_t id = 0;

    /* 读取芯片ID，确认传感器存在 */
    if (BMP280_ReadReg(BMP280_REG_ID, &id, 1) != BMP280_OK)
        return BMP280_ERR_I2C;

    if (id != 0x58)
        return BMP280_ERR_ID;

    /* 读取校准系数 */
    if (BMP280_ReadCalibration() != BMP280_OK)
        return BMP280_ERR_I2C;

    /* 配置：滤波系数×4，SPI关 */
    if (BMP280_WriteReg(BMP280_REG_CONFIG, 0xA0) != BMP280_OK)
        return BMP280_ERR_I2C;
    /* 0xA0 = 待机1000ms(101) | 滤波×4(010) | SPI关(00) */

    /* 初始化时不触发测量，留给 ReadData 做 Forced 模式单次触发 */

    return BMP280_OK;
}

/**
  * @brief  读取 BMP280 温度和气压数据
  *
  *  每调用一次触发一次 Forced 模式测量，流程：
  *  1. 写 CTRL_MEAS = 0x25（温度×1, 气压×1, Forced）
  *  2. 等待 10ms（测量时间约 7ms，留余量）
  *  3. 一次突发读取 6 字节（气压 + 温度原始数据）
  *  4. 分别进行温度补偿和气压补偿
  *
  *  @param  data : 输出数据缓冲区
  *  @retval BMP280_OK=0, BMP280_ERR_I2C=1
  */
uint8_t BMP280_ReadData(BMP280_Data *data)
{
    uint8_t buf[6] = {0};
    int32_t adc_T, adc_P;

    if (data == NULL) return BMP280_ERR_I2C;

    /* 触发单次测量（Forced模式）：温度×1 + 气压×1 */
    if (BMP280_WriteReg(BMP280_REG_CTRL_MEAS, 0x25) != BMP280_OK)
    {
        data->err = BMP280_ERR_I2C;
        return BMP280_ERR_I2C;
    }
    /* 0x25 = 温度OSS×1(001) | 气压OSS×1(001) | Forced(01) */

    /* 等待测量完成（×1过采样约需7ms） */
    HAL_Delay(10);

    /* 一次性读取压力 + 温度 6 字节 */
    if (BMP280_ReadReg(BMP280_REG_PRESS_MSB, buf, 6) != BMP280_OK)
    {
        data->err = BMP280_ERR_I2C;
        return BMP280_ERR_I2C;
    }

    /* 组装 20 位原始数据
     * 寄存器布局：
     *   buf[0] = PRESS_MSB  [19:12]
     *   buf[1] = PRESS_LSB  [11:4]
     *   buf[2] = PRESS_XLSB  [3:0] (高半字节)
     *   buf[3] = TEMP_MSB   [19:12]
     *   buf[4] = TEMP_LSB   [11:4]
     *   buf[5] = TEMP_XLSB   [3:0] (高半字节)
     */
    adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | ((int32_t)buf[2] >> 4);
    adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | ((int32_t)buf[5] >> 4);

    /* 温度补偿（同时更新 t_fine） */
    data->temperature = BMP280_CompensateTemp(adc_T);

    /* 气压补偿（依赖 t_fine） */
    data->pressure    = BMP280_CompensatePress(adc_P);

    data->err = BMP280_OK;
    return BMP280_OK;
}
