/**
  ******************************************************************************
  * @file    soft_i2c.c
  * @brief   软件 I2C（GPIO 模拟）驱动实现
  *
  *  使用 PA12(SCL) + PA15(SDA) 模拟 I2C 协议。
  *  所有操作使用 GPIO 寄存器直接控制，不依赖 HAL I2C。
  *
  *  I2C 时序（标准模式 100kHz）：
  *    SCL 频率 ≈ 1 / (2 × SOFT_I2C_DELAY × 2 × CPU_CYCLES)
  *    当前约 50~100kHz，满足 SGP30 要求（≤400kHz）
  *
  *  【GPIO 寄存器操作】
  *    BSRR  — 位设置/复位：低16位置位，高16位复位
  *    IDR   — 输入数据寄存器：读引脚电平
  *    MODER — 模式：00=输入, 01=输出
  *    PUPDR — 上下拉：01=上拉
  ******************************************************************************
 */

#include "soft_i2c.h"

/* ---------------------------------------------------------------------------
 * GPIO 寄存器快捷操作（PA12 / PA15）
 * -------------------------------------------------------------------------*/

/* SCL 置高/低 */
#define SCL_H()     SOFT_I2C_SCL_PORT->BSRR = SOFT_I2C_SCL_PIN
#define SCL_L()     SOFT_I2C_SCL_PORT->BSRR = (SOFT_I2C_SCL_PIN << 16)

/* SDA 置高/低 */
#define SDA_H()     SOFT_I2C_SDA_PORT->BSRR = SOFT_I2C_SDA_PIN
#define SDA_L()     SOFT_I2C_SDA_PORT->BSRR = (SOFT_I2C_SDA_PIN << 16)

/* 读 SDA 电平 */
#define SDA_READ()  ((SOFT_I2C_SDA_PORT->IDR & SOFT_I2C_SDA_PIN) != 0)

/* ---------------------------------------------------------------------------
 * 初始化
 * -------------------------------------------------------------------------*/

/**
  * @brief  初始化软件 I2C 引脚
  *         将 PA12(SCL) + PA15(SDA) 配置为开漏输出 + 上拉
  */
void SOFT_I2C_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio.Pin = SOFT_I2C_SCL_PIN | SOFT_I2C_SDA_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;    /* 开漏输出 */
    gpio.Pull = GPIO_PULLUP;            /* 内部上拉 */
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SOFT_I2C_SCL_PORT, &gpio);

    /* 初始状态：SCL=高, SDA=高（总线空闲） */
    SCL_H();
    SDA_H();
    SOFT_I2C_DELAY();
}

/* ---------------------------------------------------------------------------
 * I2C 时序原语
 * -------------------------------------------------------------------------*/

/**
  * @brief  产生 I2C START 条件：SCL 高时 SDA 从高→低
  */
void SOFT_I2C_Start(void)
{
    SDA_H();        /* 确保 SDA 高 */
    SOFT_I2C_DELAY();
    SCL_H();        /* SCL 高 */
    SOFT_I2C_DELAY();
    SDA_L();        /* SDA 拉低 → START */
    SOFT_I2C_DELAY();
    SCL_L();        /* SCL 拉低（准备传输数据）*/
    SOFT_I2C_DELAY();
}

/**
  * @brief  产生 I2C STOP 条件：SCL 高时 SDA 从低→高
  */
void SOFT_I2C_Stop(void)
{
    SDA_L();        /* 确保 SDA 低 */
    SOFT_I2C_DELAY();
    SCL_H();        /* SCL 高 */
    SOFT_I2C_DELAY();
    SDA_H();        /* SDA 拉高 → STOP */
    SOFT_I2C_DELAY();
}

/**
  * @brief  发送 1 字节数据（MSB 先）
  * @param  data : 要发送的字节
  * @retval 0 = 收到 ACK, 1 = 收到 NACK
  */
uint8_t SOFT_I2C_SendByte(uint8_t data)
{
    /* 发送 8 个 bit */
    for (int i = 0; i < 8; i++)
    {
        if (data & 0x80)
            SDA_H();
        else
            SDA_L();
        data <<= 1;

        SOFT_I2C_DELAY();
        SCL_H();    /* SCL 高 → 从机采样 */
        SOFT_I2C_DELAY();
        SCL_L();    /* SCL 低 */
        SOFT_I2C_DELAY();
    }

    /* 释放 SDA（开漏输出写 1 = 高阻），等待从机 ACK */
    SDA_H();
    SOFT_I2C_DELAY();
    SCL_H();        /* SCL 高 → 从机发 ACK */
    SOFT_I2C_DELAY();

    uint8_t ack = SDA_READ();  /* 0=ACK, 1=NACK */

    SCL_L();        /* SCL 低 */
    SOFT_I2C_DELAY();

    return ack;
}

/**
  * @brief  接收 1 字节数据（MSB 先）
  * @param  ack : 0 发 ACK（继续读）, 1 发 NACK（停止）
  * @retval 接收到的字节
  */
uint8_t SOFT_I2C_RecvByte(uint8_t ack)
{
    uint8_t data = 0;

    /* 释放 SDA，让从机控制 */
    SDA_H();
    SOFT_I2C_DELAY();

    /* 接收 8 个 bit */
    for (int i = 0; i < 8; i++)
    {
        data <<= 1;
        SCL_H();    /* SCL 高 → 从机输出数据 */
        SOFT_I2C_DELAY();
        if (SDA_READ())
            data |= 1;
        SCL_L();    /* SCL 低 */
        SOFT_I2C_DELAY();
    }

    /* 发送 ACK 或 NACK */
    if (ack)
        SDA_H();    /* NACK：释放 SDA */
    else
        SDA_L();    /* ACK：拉低 SDA */
    SOFT_I2C_DELAY();

    SCL_H();        /* SCL 高 → 从机读取 ACK */
    SOFT_I2C_DELAY();
    SCL_L();        /* SCL 低 */
    SOFT_I2C_DELAY();

    SDA_H();        /* 释放 SDA */
    SOFT_I2C_DELAY();

    return data;
}

/* ---------------------------------------------------------------------------
 * 便捷封装
 * -------------------------------------------------------------------------*/

/**
  * @brief  向设备写命令（含设备地址 + R/W 位）
  * @param  dev_addr : 7 位设备地址（不左移，如 0x58）
  * @param  cmd      : 16 位命令
  * @param  cmd_len  : 命令字节数（1 或 2）
  * @retval 0 = 成功, 1 = NACK
  */
uint8_t SOFT_I2C_WriteCmd(uint8_t dev_addr, uint16_t cmd, uint8_t cmd_len)
{
    SOFT_I2C_Start();

    /* 发送设备地址 + W */
    if (SOFT_I2C_SendByte(dev_addr << 1))
    {
        SOFT_I2C_Stop();
        return 1;
    }

    /* 发送命令字节 */
    if (cmd_len >= 2)
    {
        if (SOFT_I2C_SendByte((uint8_t)(cmd >> 8)))  /* 高字节 */
        {
            SOFT_I2C_Stop();
            return 1;
        }
    }
    if (SOFT_I2C_SendByte((uint8_t)(cmd & 0xFF)))   /* 低字节 */
    {
        SOFT_I2C_Stop();
        return 1;
    }

    SOFT_I2C_Stop();
    return 0;
}

/**
  * @brief  从设备读取数据（独立事务：START + 地址+R + 读取 + STOP）
  * @param  dev_addr : 7 位设备地址
  * @param  buf      : 数据缓冲区
  * @param  len      : 要读取的字节数
  * @retval 0 = 成功, 1 = NACK
  */
uint8_t SOFT_I2C_ReadData(uint8_t dev_addr, uint8_t *buf, uint8_t len)
{
    /* 重新发送 START + 设备地址 + R */
    SOFT_I2C_Start();

    if (SOFT_I2C_SendByte((dev_addr << 1) | 1))  /* 地址 + R */
    {
        SOFT_I2C_Stop();
        return 1;
    }

    /* 读取数据 */
    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t last = (i == len - 1);
        buf[i] = SOFT_I2C_RecvByte(last ? 1 : 0);  /* 最后一字节发 NACK */
    }

    SOFT_I2C_Stop();
    return 0;
}
