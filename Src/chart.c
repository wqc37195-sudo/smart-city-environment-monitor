/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    chart.c
  * @brief   OLED 折线图绘制
  *
  *  在 128x64 OLED 上绘制简单的折线统计图：
  *    - 标题行（含 Y 轴最大/最小值）
  *    - X/Y 坐标轴
  *    - 折线连接各数据点
  *    - 底部 X 轴刻度标签
  *
  *  图表区域布局（8 行 × 128 列）：
  *    行0: 标题 + Y 轴最大值  "Temp(C)  30-"
  *    行1~5: 图表主体 (40px高)
  *    行6: X 轴刻度线
  *    行7: X 轴标签（采样序号）
  *
  *  @note 依赖 ssd1306.h 的 s_buffer 帧缓冲
  ******************************************************************************
 */
/* USER CODE END Header */

#include "ssd1306.h"
#include <stdio.h>
#include <stdlib.h>

/* 图表的屏幕坐标区域 */
#define CHART_LEFT      14   /* Y轴标签宽度 */
#define CHART_RIGHT     126  /* 右边界 */
#define CHART_TOP       0    /* 第0行是标题 */
#define CHART_BOTTOM    47   /* 第5行底部（5行×8px + 8px标题行偏移） */
#define CHART_WIDTH     (CHART_RIGHT - CHART_LEFT)   /* 112px */
#define CHART_HEIGHT    (CHART_BOTTOM - 8 + 1)       /* 40px */
/* 行0(8-15px) = 标题行, 行1~5(16-55px) = 图表 */

/* ---------------------------------------------------------------------------
 * 辅助：画两点之间的直线（简易 Bresenham）
 * -------------------------------------------------------------------------*/
static void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy, e2;

    while (1)
    {
        SSD1306_SetPixel((uint8_t)x0, (uint8_t)y0, 1);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ---------------------------------------------------------------------------
 * 主函数：绘制折线图
 * -------------------------------------------------------------------------*/

/**
  * @brief  在 OLED 上绘制折线图
  *
  * @param  title : 标题文字（如 "Temp"）
  * @param  unit  : 单位（如 "C"）
  * @param  values : 数据值数组
  * @param  n      : 数据点个数 (≤30)
  * @param  y_min  : Y 轴最小值（缩放用）
  * @param  y_max  : Y 轴最大值（缩放用）
  * @param  y_unit_div : Y 轴刻度单位除数
  *         例如 temp 是 0.1°C → 显示时除以10 → y_unit_div=10
  *
  *  图表自动从 y_min 缩放到 y_max
  */
void Chart_Draw(const char *title, const char *unit,
                int32_t *values, uint8_t n,
                int32_t y_min, int32_t y_max,
                int32_t y_unit_div)
{
    if (n < 2 || y_max == y_min) return;

    char line[22];

    /* ========== 行0：标题 + Y 轴范围 ========== */
    SSD1306_SetCursor(0, 0);
    snprintf(line, sizeof(line), "%s(%s)", title, unit);
    SSD1306_WriteString(line);

    /* 右侧显示 Y 轴最大值 */
    SSD1306_SetCursor(0, 72);
    snprintf(line, sizeof(line), "%ld-", (long)(y_max / y_unit_div));
    SSD1306_WriteString(line);

    /* 右下显示 Y 轴最小值 */
    SSD1306_SetCursor(7, 72);
    snprintf(line, sizeof(line), "%ld-", (long)(y_min / y_unit_div));
    SSD1306_WriteString(line);

    /* ========== 行6：X 轴刻度线 ========== */
    SSD1306_SetCursor(6, 0);
    /* 画底部水平线 */
    for (uint8_t x = CHART_LEFT; x <= CHART_RIGHT; x++)
        SSD1306_SetPixel(x, 48, 1);
    /* 起点和终点刻度 */
    SSD1306_SetPixel(CHART_LEFT, 49, 1);
    SSD1306_SetPixel(CHART_RIGHT, 49, 1);

    /* ========== 行7：X 轴标签 ========== */
    SSD1306_SetCursor(7, 0);
    snprintf(line, sizeof(line), "0");
    SSD1306_WriteString(line);
    SSD1306_SetCursor(7, 90);
    snprintf(line, sizeof(line), "%d", n - 1);
    SSD1306_WriteString(line);

    /* ========== Y 轴辅助线（虚线） ========== */
    /* 行1~5 范围：y=8~47 */
    uint8_t chart_y0 = 8;   /* 图表顶部 Y 坐标 */
    uint8_t chart_h  = 40;  /* 图表高度 */

    /* 中间画一条水平虚线（中值线） */
    uint8_t mid_y = chart_y0 + chart_h / 2;
    for (uint8_t x = CHART_LEFT; x <= CHART_RIGHT; x += 4)
        SSD1306_SetPixel(x, mid_y, 1);

    /* ========== 画折线 ========== */
    int16_t prev_x = -1, prev_y = -1;

    for (uint8_t i = 0; i < n; i++)
    {
        int32_t val = values[i];
        if (val < y_min) val = y_min;
        if (val > y_max) val = y_max;

        /* X 坐标：均匀分布在图表宽度内 */
        int16_t px = CHART_LEFT + (int16_t)((int32_t)i * (CHART_WIDTH - 2) / (n - 1));
        /* Y 坐标：值映射到像素（值大→像素靠上） */
        int16_t py = chart_y0 + chart_h - (int16_t)((val - y_min) * chart_h / (y_max - y_min));

        if (px > CHART_RIGHT) px = CHART_RIGHT;
        if (py < chart_y0)    py = chart_y0;
        if (py > chart_y0 + chart_h - 1) py = chart_y0 + chart_h - 1;

        if (prev_x >= 0)
            draw_line(prev_x, prev_y, px, py);
        else
            SSD1306_SetPixel((uint8_t)px, (uint8_t)py, 1);  /* 第一个点 */

        prev_x = px;
        prev_y = py;
    }
}
