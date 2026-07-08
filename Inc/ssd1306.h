/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ssd1306.h
  * @brief   SSD1306 OLED 128x64 驱动头文件（I2C接口）
  *
  *  【芯片】SSD1306，单色 OLED，128×64 像素
  *  【接口】I2C（地址 0x3C），本工程使用 I2C1 (PB7/PB8)
  *  【显示原理】
  *    屏幕分为 8 个 Page（页），每页 8 像素高：
  *      Page 0: 行 0~7
  *      Page 1: 行 8~15
  *      ...   ...
  *      Page 7: 行 56~63
  *    每个 Page 有 128 个 Column（列），每列 8 个 bit 对应 8 个像素
  *
  *  【帧缓冲】
  *    大小 = 128 × 64 / 8 = 1024 字节
  *    所有绘图操作先写入缓冲区，调用 SSD1306_Update() 统一刷新到屏幕
  *
  *  【字库】
  *    内嵌 5×7 标准 ASCII 字库（95 个可打印字符）
  *    每个字符占 6 像素宽（5 像素字体 + 1 像素间隔）
  *    一行最多显示 128/6 ≈ 21 个字符
  ******************************************************************************
 */
/* USER CODE END Header */

#ifndef __SSD1306_H__
#define __SSD1306_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <string.h>

/* ==========================================================================
 *  I2C 地址
 *  SSD1306 的 I2C 地址由 SA0/DC 引脚决定：
 *    默认 (悬空/GND) → 0x3C
 *    接 VCC           → 0x3D
 *  STM32 HAL 需左移 1 位
 * ==========================================================================*/
#define SSD1306_I2C_ADDR        0x3C
#define SSD1306_I2C_ADDR_WRITE  (SSD1306_I2C_ADDR << 1)

/* ==========================================================================
 *  屏幕参数
 *  分辨率 128×64，分为 8 个 Page（每页 8 行）
 * ==========================================================================*/
#define SSD1306_WIDTH           128     /* 宽度（像素） */
#define SSD1306_HEIGHT          64      /* 高度（像素） */
#define SSD1306_PAGES           (SSD1306_HEIGHT / 8)  /* 页数 = 8 */

/* ==========================================================================
 *  I2C 控制字节
 *  SSD1306 用第一个字节区分命令和数据：
 *    0x00 = 后续字节为命令
 *    0x40 = 后续字节为显示数据
 * ==========================================================================*/
#define SSD1306_CTRL_CMD        0x00    /* 命令模式 */
#define SSD1306_CTRL_DATA       0x40    /* 数据模式 */

/* ==========================================================================
 *  常用命令列表
 *  所有命令分两类：
 *    单字节命令：直接发送 0x00 + cmd
 *    多字节命令：0x00 + cmd + 0x00 + param ...
 * ==========================================================================*/
#define SSD1306_CMD_DISPLAY_OFF         0xAE    /* 关闭显示 */
#define SSD1306_CMD_DISPLAY_ON          0xAF    /* 开启显示 */
#define SSD1306_CMD_SET_CONTRAST        0x81    /* 设置对比度（后跟1字节参数） */
#define SSD1306_CMD_DISPLAY_RAM         0xA4    /* 显示RAM内容 */
#define SSD1306_CMD_DISPLAY_ALL_ON      0xA5    /* 全部点亮 */
#define SSD1306_CMD_NORMAL_DISPLAY      0xA6    /* 正常显示（白底黑字） */
#define SSD1306_CMD_INVERT_DISPLAY      0xA7    /* 反相显示 */
#define SSD1306_CMD_SET_MUX             0xA8    /* 设置多路复用比（后跟参数） */
#define SSD1306_CMD_SET_OFFSET          0xD3    /* 显示偏移 */
#define SSD1306_CMD_SET_START_LINE      0x40    /* 设置起始行 */
#define SSD1306_CMD_SEG_REMAP_L         0xA0    /* 列重映射：左→右 */
#define SSD1306_CMD_SEG_REMAP_R         0xA1    /* 列重映射：右→左 */
#define SSD1306_CMD_COM_SCAN_INC        0xC0    /* COM扫描：递增 */
#define SSD1306_CMD_COM_SCAN_DEC        0xC8    /* COM扫描：递减 */
#define SSD1306_CMD_SET_COM_PINS        0xDA    /* COM引脚配置 */
#define SSD1306_CMD_SET_CLOCK_DIV       0xD5    /* 时钟分频/振荡频率 */
#define SSD1306_CMD_SET_PRECHARGE       0xD9    /* 预充电周期 */
#define SSD1306_CMD_SET_VCOM_DESELECT   0xDB    /* VCOMH电压 */
#define SSD1306_CMD_CHARGE_PUMP         0x8D    /* 电荷泵设置 */
#define SSD1306_CMD_MEMORY_MODE         0x20    /* 内存寻址模式 */
#define SSD1306_CMD_SET_PAGE_ADDR       0xB0    /* 设置页地址 */
#define SSD1306_CMD_SET_COLUMN_ADDR     0x21    /* 设置列地址范围 */
#define SSD1306_CMD_SET_PAGE_RANGE      0x22    /* 设置页地址范围 */

/* 函数声明 */
void    SSD1306_Init(void);                  /* 初始化OLED：发送初始化命令序列 */
void    SSD1306_Clear(void);                 /* 清屏（仅清缓冲区，需调用Update） */
void    SSD1306_Update(void);                /* 刷新：将缓冲区发送到OLED */
void    SSD1306_SetCursor(uint8_t page, uint8_t col); /* 设置光标位置（页,列） */
void    SSD1306_WriteChar(char ch);          /* 在当前光标写一个字符 */
void    SSD1306_WriteString(const char *str);/* 在当前光标写字符串 */
void    SSD1306_SetPixel(uint8_t x, uint8_t y, uint8_t color);
void    SSD1306_WriteChineseStr(uint8_t page, uint8_t col, const char *utf8_str); /* 设置单个像素 */

#ifdef __cplusplus
}
#endif

#endif /* __SSD1306_H__ */
