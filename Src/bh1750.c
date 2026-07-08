/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bh1750.c
  * @brief   GY-30 (BH1750) 数字光照度传感器驱动实现
  *
  *  ============================= 工作原理 =============================
  *
  *  【测量原理】
  *  BH1750 内部集成了光电二极管 + 积分式 ADC。
  *  光电二极管将光强转换为电流，ADC 将电流积分后转换为数字值。
  *  内置的 50/60Hz 纹波抑制功能可过滤人工光源的频闪。
  *
  *  【通信流程】
  *  1. 初始化：复位 → 上电 → 设置连续高分辨率模式
  *  2. 读取：直接 I2C 读取 2 字节数据寄存器
  *  3. 转换：原始值 / 1.2 = lux
  *
  *  【数据格式】
  *  16 位数据寄存器，高字节在前，低字节在后。
  *  光照度 = (高字节<<8 | 低字节) / 1.2
  *
  *  例如：
  *    buf[0]=0x6A, buf[1]=0x12
  *    raw = 0x6A12 = 27154
  *    lux = 27154 / 1.2 = 22628 lux（户外太阳直射）
  *
  *  【测量模式对比】
  *  模式             精度    测量时间     适用场景
  *  高分辨率 (H)     1 lx    120ms       标准室内外
  *  高分辨率2 (H2)   0.5 lx  120ms       暗光环境
  *  低分辨率 (L)     4 lx     16ms       快速扫描
  *
  *  ==================================================================
  *
  *  接线：
  *    GY-30       Nucleo-WL55JC
  *    VCC     →   3.3V
  *    GND     →   GND
  *    SDA     →   PB7 (I2C1_SDA) — 与 BMP280 + OLED 并联
  *    SCL     →   PB8 (I2C1_SCL) — 与 BMP280 + OLED 并联
  *    ADDR    →   悬空或接 GND（地址 0x23）
  ******************************************************************************
 */
/* USER CODE END Header */

#include "bh1750.h"

/* 外部 I2C 句柄（由 CubeMX 在 i2c.c 中初始化） */
extern I2C_HandleTypeDef hi2c1;

/**
  * @brief  初始化 BH1750 光照传感器
  *
  *  执行顺序：
  *  1. 复位（RESET）— 清除内部数据寄存器
  *  2. 上电（POWER_ON）— 退出断电状态
  *  3. 设置连续高分辨率模式（CONT_H_RES）
  *     → 传感器开始持续测量，每 120ms 更新一次内部数据寄存器
  *
  *  @note 连续模式下，只需在读取时发送 I2C 接收请求，
  *        不需要每次触发测量。
  *
  *  @retval 0 = 成功, 1 = I2C 通信失败
  */
uint8_t BH1750_Init(void)
{
    uint8_t cmd;

    /* 第1步：复位传感器 */
    cmd = BH1750_CMD_RESET;
    if (HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR_WRITE, &cmd, 1,
                                HAL_MAX_DELAY) != HAL_OK)
        return 1;
    HAL_Delay(30);

    /* 第2步：上电 */
    cmd = BH1750_CMD_POWER_ON;
    if (HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR_WRITE, &cmd, 1,
                                HAL_MAX_DELAY) != HAL_OK)
        return 1;
    HAL_Delay(30);

    /* 第3步：设置连续高分辨率模式 */
    cmd = BH1750_CMD_CONT_H_RES;
    if (HAL_I2C_Master_Transmit(&hi2c1, BH1750_ADDR_WRITE, &cmd, 1,
                                HAL_MAX_DELAY) != HAL_OK)
        return 1;

    /* 等待首次测量完成（高分辨率模式约需 120ms） */
    HAL_Delay(180);

    return 0;
}

/**
  * @brief  读取环境光照度
  *
  *  在连续模式下，BH1750 内部持续更新数据寄存器。
  *  读取操作只需发送 I2C 接收请求，传感器会自动返回当前值。
  *
  *  数据转换：
  *    raw = (buf[0] << 8) | buf[1]
  *    lux = raw / 1.2
  *
  *  使用整数运算实现除以 1.2：
  *    lux = raw * 100 / 120
  *
  *  @param  data : 输出数据结构体（接收 lux 值）
  *  @retval 0 = 成功, 1 = I2C 通信失败
  */
uint8_t BH1750_ReadLight(BH1750_Data *data)
{
    uint8_t buf[2] = {0};

    if (data == NULL) return 1;

    /* BH1750 在连续模式下自动更新数据，直接读取 2 字节 */
    if (HAL_I2C_Master_Receive(&hi2c1, BH1750_ADDR_READ, buf, 2,
                               HAL_MAX_DELAY) != HAL_OK)
    {
        data->err = 1;
        return 1;
    }

    /* 原始值 = (高字节<<8) | 低字节，除以 1.2 得到 lx */
    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    data->lux = (uint16_t)((uint32_t)raw * 100 / 120);  /* raw / 1.2 */
    data->err = 0;

    return 0;
}
