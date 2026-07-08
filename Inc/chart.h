/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    chart.h
  * @brief   OLED 折线图绘制头文件
  ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __CHART_H__
#define __CHART_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

void Chart_Draw(const char *title, const char *unit,
                int32_t *values, uint8_t n,
                int32_t y_min, int32_t y_max,
                int32_t y_unit_div);

#ifdef __cplusplus
}
#endif

#endif /* __CHART_H__ */
