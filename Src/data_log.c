/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    data_log.c
  * @brief   传感器数据环形缓冲区实现
  *
  *  环形缓冲区：写入满后自动覆盖最旧数据
  *  支持获取最小最大值（用于 Y 轴自动缩放）
  ******************************************************************************
 */
/* USER CODE END Header */

#include "data_log.h"

/**
  * @brief  初始化数据日志
  */
void DataLog_Init(DataLog *log)
{
    if (log == NULL) return;
    log->head  = 0;
    log->count = 0;
}

/**
  * @brief  写入一条新数据（自动覆盖最旧）
  */
void DataLog_Push(DataLog *log, DataPoint *p)
{
    if (log == NULL || p == NULL) return;

    log->buf[log->head] = *p;
    log->head = (log->head + 1) % DATA_LOG_SIZE;
    if (log->count < DATA_LOG_SIZE) log->count++;
}

/**
  * @brief  获取已存数据条数
  */
uint8_t DataLog_GetCount(DataLog *log)
{
    return (log == NULL) ? 0 : log->count;
}

/**
  * @brief  获取某字段的最小最大值
  * @param  field : FIELD_TEMP / FIELD_HUMI / FIELD_LIGHT / FIELD_PM25
  * @param  min   : 输出最小值
  * @param  max   : 输出最大值
  */
void DataLog_GetMinMax(DataLog *log, uint8_t field, int32_t *min, int32_t *max)
{
    if (log == NULL || log->count == 0) return;

    int32_t vmin = 999999, vmax = -999999;
    uint8_t start = (log->count < DATA_LOG_SIZE) ? 0 : log->head;
    uint8_t n = log->count;

    for (uint8_t i = 0; i < n; i++)
    {
        uint8_t idx = (start + i) % DATA_LOG_SIZE;
        int32_t val = DataLog_GetValue(log, idx, field);
        if (val < vmin) vmin = val;
        if (val > vmax) vmax = val;
    }

    if (min) *min = vmin;
    if (max) *max = vmax;
}

/**
  * @brief  获取缓冲区中某条数据的某字段值
  * @param  idx : 缓冲区索引 (0~DATA_LOG_SIZE-1)
  * @param  field : 字段编号
  */
int32_t DataLog_GetValue(DataLog *log, uint8_t idx, uint8_t field)
{
    if (log == NULL || idx >= DATA_LOG_SIZE) return 0;

    switch (field)
    {
        case FIELD_TEMP:  return log->buf[idx].temp;
        case FIELD_HUMI:  return log->buf[idx].humi;
        case FIELD_LIGHT: return log->buf[idx].light;
        case FIELD_PM25:  return log->buf[idx].pm25;
        default:          return 0;
    }
}
