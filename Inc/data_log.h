/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    data_log.h
  * @brief   传感器数据环形缓冲区（存储最近 N 条记录）
  *
  *  最多存储 30 条记录（每条 = 温度+湿度+光照+粉尘）
  *  绘图时自动取最小最大值做 Y 轴缩放
  ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __DATA_LOG_H__
#define __DATA_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define DATA_LOG_SIZE  30   /* 存储最近 30 条（=60秒） */

/** 单条传感器数据 */
typedef struct {
    int16_t  temp;     /* 温度，单位 0.1°C (如 270 = 27.0°C) */
    uint16_t humi;     /* 湿度 0.1% (如 450 = 45.0%) */
    uint16_t light;    /* 光照 lx */
    uint16_t pm25;     /* 粉尘 ug/m³ */
} DataPoint;

/** 数据日志 */
typedef struct {
    DataPoint buf[DATA_LOG_SIZE];  /* 环形缓冲区 */
    uint8_t  head;                 /* 当前写入位置 */
    uint8_t  count;                /* 已存数据条数 (≤ DATA_LOG_SIZE) */
} DataLog;

void   DataLog_Init(DataLog *log);
void   DataLog_Push(DataLog *log, DataPoint *p);
uint8_t DataLog_GetCount(DataLog *log);
void   DataLog_GetMinMax(DataLog *log, uint8_t field,
                         int32_t *min, int32_t *max);
int32_t DataLog_GetValue(DataLog *log, uint8_t idx, uint8_t field);

/* field 取值 */
#define FIELD_TEMP   0
#define FIELD_HUMI   1
#define FIELD_LIGHT  2
#define FIELD_PM25   3

#ifdef __cplusplus
}
#endif

#endif /* __DATA_LOG_H__ */
