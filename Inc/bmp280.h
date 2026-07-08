/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bmp280.h
  * @brief   BMP280 大气压强传感器驱动头文件
  *
  *  【通信接口】I2C1，地址 0x76（ADDR 接 GND 或悬空）
  *  【测量内容】大气压强 + 温度
  *  【数据输出】气压：Pa（需除以 100 得 hPa），温度：0.01°C
  *  【校准方式】出厂时写入 OTP，启动时通过 I2C 读取 24 字节校准系数
  *  【补偿公式】Bosch 官方 64 位整数补偿算法
  *  【测量模式】Forced 模式：每次读取前触发单次测量
  ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __BMP280_H__
#define __BMP280_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ==========================================================================
 *  器件地址
 *  BMP280 I2C 地址由 SDO/ADDR 引脚决定：
 *    ADDR 接 GND 或悬空 → 0x76 （默认，最常见）
 *    ADDR 接 VCC        → 0x77
 *  STM32 HAL 的地址参数需左移 1 位（7位地址左对齐）
 * ==========================================================================*/
#define BMP280_ADDR             0x76
#define BMP280_ADDR_WRITE       (BMP280_ADDR << 1)     /* 0xEC */
#define BMP280_ADDR_READ        ((BMP280_ADDR << 1) | 1) /* 0xED */

/* ==========================================================================
 *  寄存器地址映射
 *  ================
 *  0xD0 : ID          — 只读，固定值 0x58（BMP280 标识）
 *  0xE0 : RESET       — 写 0xB6 执行软复位
 *  0xF4 : CTRL_MEAS   — 控制测量：过采样率 + 模式
 *  0xF5 : CONFIG      — 待机时间 + 滤波系数 + SPI 模式
 *  0xF7~0xF9 : PRESS  — 20位气压原始数据（MSB+LSB+XLSB）
 *  0xFA~0xFC : TEMP   — 20位温度原始数据（MSB+LSB+XLSB）
 *  0x88~0x9F : CALIB  — 24字节出厂校准系数
 * ==========================================================================*/
#define BMP280_REG_ID           0xD0    /* 芯片ID，应返回 0x58 */
#define BMP280_REG_RESET        0xE0    /* 软复位 */
#define BMP280_REG_CTRL_MEAS    0xF4    /* 控制测量寄存器 */
#define BMP280_REG_CONFIG       0xF5    /* 配置寄存器 */
#define BMP280_REG_PRESS_MSB    0xF7    /* 压力数据 MSB */
#define BMP280_REG_TEMP_MSB     0xFA    /* 温度数据 MSB */

/* BMP280 校准数据起始地址（24字节） */
#define BMP280_REG_CALIB_T1     0x88

/* 测量模式（CTRL_MEAS 寄存器的 bit[1:0]） */
#define BMP280_MODE_SLEEP       0x00    /* 休眠 */
#define BMP280_MODE_FORCED      0x01    /* 强制：单次测量后自动休眠 */
#define BMP280_MODE_NORMAL      0x03    /* 正常：连续自动测量 */

/* 过采样配置（CTRL_MEAS 寄存器的 bit[7:5]=温度, bit[4:2]=气压） */
#define BMP280_OSS_1            0x01    /* 过采样 ×1 */
#define BMP280_OSS_2            0x02    /* 过采样 ×2 */
#define BMP280_OSS_4            0x03    /* 过采样 ×4 */
#define BMP280_OSS_8            0x04    /* 过采样 ×8 */
#define BMP280_OSS_16           0x05    /* 过采样 ×16 */

/* 滤波器系数（CONFIG 寄存器的 bit[4:2]） */
#define BMP280_FILTER_OFF       0x00
#define BMP280_FILTER_2         0x01
#define BMP280_FILTER_4         0x02
#define BMP280_FILTER_8         0x03
#define BMP280_FILTER_16        0x04

/* 函数返回值 */
#define BMP280_OK               0       /* 成功 */
#define BMP280_ERR_I2C          1       /* I2C 通信错误 */
#define BMP280_ERR_ID           2       /* 芯片 ID 不匹配（不是 BMP280） */

/** BMP280 输出数据结构体 */
typedef struct {
    int32_t  temperature;   /* 温度，单位 0.01°C（如 2501 = 25.01°C） */
    uint32_t pressure;      /* 气压，单位 Pa（如 101325 = 1013.25hPa） */
    uint8_t  err;           /* 错误码：0=成功 */
} BMP280_Data;

/** BMP280 校准系数结构体（内部使用，由驱动自动填充） */
typedef struct {
    uint16_t dig_T1;        /* 温度校准系数 T1 — unsigned */
    int16_t  dig_T2;        /* 温度校准系数 T2 — signed */
    int16_t  dig_T3;        /* 温度校准系数 T3 — signed */
    uint16_t dig_P1;        /* 气压校准系数 P1 — unsigned */
    int16_t  dig_P2;        /* 气压校准系数 P2 — signed */
    int16_t  dig_P3;        /* 气压校准系数 P3 — signed */
    int16_t  dig_P4;        /* 气压校准系数 P4 — signed */
    int16_t  dig_P5;        /* 气压校准系数 P5 — signed */
    int16_t  dig_P6;        /* 气压校准系数 P6 — signed */
    int16_t  dig_P7;        /* 气压校准系数 P7 — signed */
    int16_t  dig_P8;        /* 气压校准系数 P8 — signed */
    int16_t  dig_P9;        /* 气压校准系数 P9 — signed */
    int32_t  t_fine;        /* 温度补偿中间值（供气压补偿使用） */
} BMP280_Calib;

/* 函数声明 */
uint8_t BMP280_Init(void);           /* 初始化：读ID→读校准→配滤波 */
uint8_t BMP280_ReadData(BMP280_Data *data); /* 触发测量→读数据→补偿计算 */

#ifdef __cplusplus
}
#endif

#endif /* __BMP280_H__ */
