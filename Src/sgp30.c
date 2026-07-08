/**
  ******************************************************************************
  * @file    sgp30.c
  * @brief   GY-SGP30 TVOC/eCO2 传感器驱动实现
  *
  *  ============================= 通信协议 =============================
  *
  *  SGP30 使用 Sensirion 标准 I2C 协议：
  *
  *  1. 主机发送 2 字节命令（big-endian）
  *  2. 传感器执行测量
  *  3. 主机读取 N×(2 字节数据 + 1 字节 CRC8)
  *
  *  CRC-8 校验覆盖每 2 字节数据：
  *    多项式: 0x31 (x^8 + x^5 + x^4 + 1)
  *    初始值: 0xFF
  *    不反转，不异或
  *
  *  ============================= 测量流程 =============================
  *
  *  Init:
  *    发 0x3682 (Get Serial ID) → 读 9 字节 → 验证 CRC → 确认传感器存在
  *
  *  Read (每次调用):
  *    1. 发 0x2008 (measure IAQ)
  *    2. 等 12ms
  *    3. 读 6 字节 (eCO2_H+eCO2_L+CRC + TVOC_H+TVOC_L+CRC)
  *    4. 解析并校验 CRC
  *
  *  ==================================================================
  *
  *  @note 使用软件 I2C（PA12=SCL, PA15=SDA），与 I2C1 完全隔离
  ******************************************************************************
 */

#include "sgp30.h"
#include "soft_i2c.h"
#include <stdio.h>

/* 软件 I2C 设备地址（7-bit，不左移）*/
#define SGP30_SOFT_ADDR     0x58

/* ---------------------------------------------------------------------------
 * 模块1：CRC-8 计算（Sensirion 自定义）
 * =========================================================================
 * 多项式: x^8 + x^5 + x^4 + 1 = 0x31
 * 算法: 逐字节查表
 * -------------------------------------------------------------------------*/

/**
  * @brief  CRC-8 查表法（Sensirion 自定义算法）
  * @param  data : 输入数据字节
  * @param  crc  : 前一步的 CRC 值（初始 0xFF）
  * @retval 更新后的 CRC 值
  */
static uint8_t CRC8_Step(uint8_t data, uint8_t crc)
{
    crc ^= data;
    for (int i = 0; i < 8; i++)
    {
        if (crc & 0x80)
            crc = (crc << 1) ^ 0x31;
        else
            crc = (crc << 1);
    }
    return crc;
}

/**
  * @brief  计算一段数据的 Sensirion CRC-8
  * @param  data : 数据指针
  * @param  len  : 数据长度（字节）
  * @retval 8-bit CRC 值
  */
static uint8_t CRC8_Calc(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    for (uint8_t i = 0; i < len; i++)
    {
        crc = CRC8_Step(data[i], crc);
    }
    return crc;
}

/* ---------------------------------------------------------------------------
 * 模块2：I2C 发送命令（2 字节 big-endian）
 * =========================================================================
 * SGP30 所有命令均为 16 位，先发高字节再发低字节。
 * -------------------------------------------------------------------------*/

/**
  * @brief  通过 I2C 向 SGP30 发送 2 字节命令
  * @param  cmd : 16 位命令（如 0x2008）
  * @retval 0=成功, 1=I2C错误
  */
static uint8_t SGP30_SendCmd(uint16_t cmd)
{
    if (SOFT_I2C_WriteCmd(SGP30_SOFT_ADDR, cmd, 2))
    {
        return 1;
    }
    return 0;
}

/**
  * @brief  从 SGP30 读取数据，含 CRC 校验
  * @param  buf    : 数据接收缓冲区
  * @param  words  : 期望读取的 16 位数据个数（如 2 = eCO2+TVOC）
  * @retval 0=成功, 1=I2C错误, 2=CRC错误
  */
static uint8_t SGP30_ReadRaw(uint8_t *raw, uint8_t words)
{
    uint8_t len = words * 3;  /* 每个 word = 2 字节数据 + 1 字节 CRC */
    if (len > 32) return 1;

    if (SOFT_I2C_ReadData(SGP30_SOFT_ADDR, raw, len))
    {
        return 1;  /* I2C 错误 */
    }
    return 0;
}

/**
  * @brief  读取并校验 Sensirion CRC-8，提取有效数据
  * @param  buf    : 输出缓冲区（仅有效数据，不含 CRC）
  * @param  raw    : 原始数据缓冲区（含 CRC）
  * @param  words  : 16 位数据个数
  * @retval 0=成功, 2=CRC错误
  */
static uint8_t SGP30_ExtractData(uint8_t *buf, uint8_t *raw, uint8_t words)
{
    for (uint8_t i = 0; i < words; i++)
    {
        uint8_t *word_raw = raw + i * 3;
        uint8_t calc_crc = CRC8_Calc(word_raw, 2);

        if (calc_crc != word_raw[2])
        {
            return 2;  /* CRC 错误 */
        }

        /* 提取数据（big-endian） */
        buf[i * 2]     = word_raw[0];
        buf[i * 2 + 1] = word_raw[1];
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * 模块3：公开接口
 * -------------------------------------------------------------------------*/

/**
  * @brief  初始化 SGP30 传感器
  *
  *  通过读取序列号验证传感器存在：
  *    - 发送 0x3682 (Get Serial ID)
  *    - 等待 1ms
  *    - 读回 9 字节（3 × [2B数据 + 1B CRC]）
  *    - 验证 CRC
  *
  *  @retval 0=成功, 1=I2C错误, 2=CRC错误
  */
uint8_t SGP30_Init(void)
{
    uint8_t buf[6] = {0};  /* 3 个 word × 2 字节 */

    /* 发送 Get Serial ID 命令 */
    if (SGP30_SendCmd(SGP30_CMD_GET_SERIAL))
    {
        return 1;
    }

    /* 等待传感器处理 */
    HAL_Delay(1);

    /* 读取序列号（3 words, 9 bytes raw → 6 bytes data） */
    {
        uint8_t raw[9];
        if (SGP30_ReadRaw(raw, 3)) return 1;
        if (SGP30_ExtractData(buf, raw, 3)) return 2;
    }

    /* 序列号读取成功即表示传感器存在 */
    /* 启动 IAQ 基线算法（必须！否则 eCO2 始终为 400） */
    if (SGP30_SendCmd(SGP30_CMD_IAQ_INIT))
    {
        return 1;
    }
    HAL_Delay(SGP30_IAQ_INIT_TIME_MS);

    /* 首次 IAQ 测量 — 丢弃预热读数 */
    if (SGP30_SendCmd(SGP30_CMD_IAQ_MEASURE))
    {
        return 1;
    }
    HAL_Delay(SGP30_IAQ_MEASURE_TIME_MS);

    /* 丢弃第一次测量结果（预热） */
    {
        uint8_t raw[6];
        if (SGP30_ReadRaw(raw, 2) == 0)
        {
            SGP30_ExtractData(buf, raw, 2);
        }
    }

    return 0;
}

/**
  * @brief  读取 SGP30 的 TVOC 和 eCO2 浓度
  *
  *  每次调用触发一次 IAQ 测量：
  *    1. 发送 0x2008 (measure IAQ)
  *    2. 等待 12ms
  *    3. 读取 6 字节（eCO2 + TVOC，各附 1 字节 CRC）
  *
  *  @param  data : 输出数据结构体
  *  @retval 0=成功, 1=I2C错误, 2=CRC错误
  */
uint8_t SGP30_Read(SGP30_Data *data)
{
    uint8_t buf[4] = {0};  /* eCO2(2B) + TVOC(2B) */

    if (data == NULL) return 1;

    /* 发送 IAQ 测量命令 */
    if (SGP30_SendCmd(SGP30_CMD_IAQ_MEASURE))
    {
        data->err = 1;
        return 1;
    }

    /* 等待测量完成 */
    HAL_Delay(SGP30_IAQ_MEASURE_TIME_MS);

    /* 读取 eCO2 + TVOC（2 words, 6 bytes raw → 4 bytes data）*/
    {
        uint8_t raw[6];
        uint8_t ret = SGP30_ReadRaw(raw, 2);
        if (ret == 0)
            ret = SGP30_ExtractData(buf, raw, 2);
        data->eco2_ppm = ((uint16_t)buf[0] << 8) | buf[1];
        data->tvoc_ppb = ((uint16_t)buf[2] << 8) | buf[3];
        data->err = ret;
        return ret;
    }
}
