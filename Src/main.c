/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    UART/UART_Printf/Src/main.c
  * @author  MCD Application Team
  * @brief   ==============================================================
  *          多传感器数据采集 + OLED 显示 示例程序
  *          ==============================================================
  *
  *  本工程集成了 6 个传感器模块，通过 I2C 和单总线采集数据，
  *  同时输出到串口（115200-7-O-1）和 OLED 屏幕。
  *
  *  【外设清单】
  *    PA2/PA3 — USART2: printf 串口输出到 PC
  *    PB13    — ADC1_IN0: GP2Y1010 粉尘传感器模拟输出
  *    PB1     — GPIO:   GP2Y1010 V-LED 控制
  *    PB3     — GPIO:   DHT11 温湿度传感器（单总线协议）
  *    PB12    — ADC1_IN1: MAX9814/MAX4466 模拟麦克风
  *    PA12/PA15 — 软件 I2C: GY-SGP30 (TVOC/eCO2, 地址 0x58)
  *    PB7/PB8 — I2C1:   挂载三个设备：
  *                        - BMP280 (气压+温度, 地址 0x76)
  *                        - SSD1306 OLED (128x64 显示屏, 地址 0x3C)
  *                        - GY-30/BH1750 (光照度, 地址 0x23)
  *    PA13/PA14 — SWD: 烧录调试接口
  *
  *  【数据流】
  *    主循环每 2 秒执行一次：
  *      1. DHT11_Read()  → 温湿度（单总线时序）
  *      2. BMP280_Read() → 气压 + 温度（I2C, 补偿公式）
  *      3. BH1750_Read() → 光照度（I2C）
  *      4. MIC_Read()    → 声音强度（ADC 突发采样）
  *      5. SGP30_Read()   → TVOC + eCO2（I2C, CRC8 校验）
  *      6. 串口 printf 输出所有数据
  *      7. OLED 刷新显示
  *      8. LED3 翻转指示运行状态
  *
  *  @note USER CODE 区域内的代码不会被 CubeMX 重新生成覆盖
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "app_subghz_phy.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* ========== 用户添加的头文件 ========== */
#include <stdio.h>         /* 标准 I/O，用于 printf */
#include "dht11.h"         /* DHT11 温湿度传感器驱动 */
#include "bmp280.h"        /* BMP280 气压温度传感器驱动 */
#include "ssd1306.h"       /* SSD1306 OLED 显示屏驱动 */
#include "bh1750.h"        /* GY-30 / BH1750 光照传感器驱动 */
#include "gp2y1010.h"
#include "data_log.h"
#include "chart.h"      /* GP2Y1010 粉尘传感器驱动 */
#include "lora_tx.h"       /* LoRa 射频发送驱动 */
#include "mic.h"           /* MAX9814/MAX4466 麦克风驱动 */
#include "sgp30.h"          /* GY-SGP30 TVOC/eCO2 传感器驱动 */
#include "soft_i2c.h"       /* 软件 I2C PA12/PA15 */
#if defined(__ICCARM__)
#include <LowLevelIOInterface.h>
#endif /* __ICCARM__ */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* ========== 全局传感器数据（供 chart/data_log 使用） ========== */
DHT11_Data s_dht;
BMP280_Data s_bmp;
BH1750_Data s_bh;
GP2Y1010_Data s_gp;
MIC_Data s_mic;             /* MAX9814/MAX4466 麦克风声音强度 */
SGP30_Data s_sgp;            /* GY-SGP30 TVOC(ppb) + eCO2(ppm) */
/* ========== 全局变量 ========== */
DataLog s_log;              /* 传感器数据环形缓冲区 */
uint8_t  s_page = 0;        /* OLED 当前页面 (0~3) */
uint8_t  s_btn_last = 1;    /* 按键上一次状态（1=松开） */
uint8_t  s_btn_press = 0;   /* 按键按下标志 */
uint8_t  s_lora_ok_last = 0; /* 上次 LoRa 发送结果：1=成功 */
uint16_t s_lora_count = 0;   /* LoRa 成功发送计数 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* ========== printf 重定向（根据不同编译器选择不同宏） ========== */
#if defined(__ICCARM__)
/* New definition from EWARM V9, compatible with EWARM8 */
int iar_fputc(int ch);
#define PUTCHAR_PROTOTYPE int iar_fputc(int ch)
#elif defined (__CC_ARM) || defined(__ARMCC_VERSION)
/* ARM Compiler 5/6 */
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#elif defined(__GNUC__)
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#endif /* __ICCARM__ */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* ========== 板载按键 B1 (PC6) 初始化 ========== */
static void Button_Init(void)
{
    BSP_PB_Init(BUTTON_SW3, BUTTON_MODE_GPIO);
}

/* ========== 按键扫描 ========== */
static void Button_Scan(void)
{
    uint8_t level = BSP_PB_GetState(BUTTON_SW3); /* 0=按下 */
    /* 消抖 */
    if (level == 0 && s_btn_last == 1) {
        HAL_Delay(20);
        level = BSP_PB_GetState(BUTTON_SW3);
        if (level == 0) {
            s_btn_press = 1;
        }
    }
    s_btn_last = level;
}
static void LogCurrentData(void)
{
    /* 从全局变量读取最新数据（已在 while 中更新） */
    /* 在 main 中调用前需已读取 DHT/BMP/GY */
    extern DHT11_Data s_dht;
    extern BMP280_Data s_bmp;
    extern BH1750_Data s_bh;
    extern GP2Y1010_Data s_gp;

    DataPoint pt;
    pt.temp  = (int16_t)(s_dht.temperature_int * 10 + s_dht.temperature_dec);
    pt.humi  = (uint16_t)(s_dht.humidity_int * 10 + s_dht.humidity_dec);
    pt.light = s_bh.lux;
    pt.pm25  = s_gp.density;
    DataLog_Push(&s_log, &pt);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* STM32WLxx HAL library initialization:
       - Configure the Flash prefetch
       - Systick timer is configured by default as source of time base, but user 
         can eventually implement his proper time base source (a general purpose 
         timer for example or other time source), keeping in mind that Time base 
         duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and 
         handled in milliseconds basis.
       - Set NVIC Group Priority to 4
       - Low Level Initialization
     */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* 初始化板载 LED3（用于指示运行状态） */
  BSP_LED_Init(LED3);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_ADC_Init();
  MX_SubGHz_Phy_Init();
  /* USER CODE BEGIN 2 */
  /* ==================================================================
   *  传感器初始化阶段
   *  按顺序逐个初始化各传感器，每个都有独立的错误处理
   * ==================================================================*/

  /* --- DHT11 温湿度传感器 --- */
  /* 使用 PB3 单总线协议，初始化 DWT 微秒定时器 */
  DHT11_Init();

  /* --- BMP280 气压温度传感器 --- */
  /* I2C 地址 0x76，读取校准系数，配置为 Forced 模式 */
  {
    uint8_t bmp_ret = BMP280_Init();
    if (bmp_ret == BMP280_OK)
      printf("BMP280 init OK\r\n");
    else
      printf("BMP280 init FAIL (err=%d)\r\n", bmp_ret);
  }

  /* --- SSD1306 OLED 显示屏 --- */
  /* I2C 地址 0x3C，128x64 分辨率，初始化后显示标题 */
  SSD1306_Init();

  /* --- GY-30 (BH1750) 光照传感器 --- */
  /* I2C 地址 0x23，连续高分辨率模式 */
  {
    uint8_t bh_ret = BH1750_Init();
    if (bh_ret == 0)
      printf("GY-30 init OK\r\n");
    else
      printf("GY-30 init FAIL\r\n");
  }

  /* --- GP2Y1010 粉尘传感器 --- */
  /* PB1(V-LED) + PB13(ADC)，脉冲驱动，10次采样取平均 */
  GP2Y1010_Init();
  printf("GP2Y1010 init OK\r\n");

  /* --- MAX9814/MAX4466 模拟麦克风 --- */
  /* PB12(ADC1_IN1)，32 次突发采样计算声音强度 */
  MIC_Init();
  printf("MIC init OK (PB12/ADC1_IN1)\r\n");

  /* --- GY-SGP30 TVOC/eCO2 传感器（软件 I2C: PA12/PA15）--- */
  SOFT_I2C_Init();
  {
    uint8_t sgp_ret = SGP30_Init();
    if (sgp_ret == 0)
      printf("SGP30 init OK\r\n");
    else
      printf("SGP30 init FAIL (err=%d)\r\n", sgp_ret);
  }

  /* --- 初始化数据日志和按键 --- */
  DataLog_Init(&s_log);
  Button_Init();

  /* --- LoRa 射频初始化 --- */
  LoRa_TX_Init();
  printf("LoRa TX init OK (SF7/125kHz/CR4_5 @ 433MHz)\r\n");

  printf("System ready, press B1 to switch page\r\n");

  /* OLED 显示标题（初始化完成后显示一次） */
  SSD1306_SetCursor(0, 0);
  SSD1306_WriteString("DHT11+BMP280");
  SSD1306_SetCursor(1, 0);
  SSD1306_WriteString("Press B1:Chart");
  SSD1306_Update();

  /* 串口输出启动标志 */
  printf("\n\r ===== DHT11 + BMP280 + OLED =====\n\r");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /* ==================================================================
   *  主循环：每 2 秒采集一次数据
   *  每次循环执行：
   *    0. 读取 GP2Y1010 → 粉尘浓度（10次脉冲采样平均）
   *    1. 读取 DHT11 → 温湿度
   *    2. 读取 BMP280 → 气压
   *    3. 读取 GY-30  → 光照度（lux）
   *    4. 读取 MAX9814 → 声音强度（32次突发采样）
   *    5. 读取 SGP30   → TVOC + eCO2
   *    6. 串口打印所有数据
   *    7. OLED 刷新显示
   *    8. LED3 翻转
   * ==================================================================*/
  while (1)
  {
    Button_Scan();
    if (s_btn_press)
    {
        s_btn_press = 0;
        s_page = (s_page + 1) % 5;
        printf("Page: %d\r\n", s_page);
    }

    /* ===== Read sensors ===== */
    GP2Y1010_Read(&s_gp);
    uint8_t dht_ok = (DHT11_Read(&s_dht) == 0);
    uint8_t bmp_ok = (BMP280_ReadData(&s_bmp) == BMP280_OK);
    uint8_t bh_ok  = (BH1750_ReadLight(&s_bh) == 0);
    MIC_Read(&s_mic);   /* 32 次突发采样，不影响其他传感器 */
    SGP30_Read(&s_sgp); /* TVOC + eCO2（I2C, 12ms 等待）*/

    /* ===== 串口输出：所有数据整合在一个框里 ===== */
    printf("\r\n+------ Sensor Data ------+\r\n");
    if (dht_ok)
      printf("| Tmp: %d.%dC  Hum: %d.%d%% |\r\n",
             s_dht.temperature_int, s_dht.temperature_dec,
             s_dht.humidity_int, s_dht.humidity_dec);
    else
      printf("| Tmp: --.-C  Hum: --.-%% |\r\n");

    if (bmp_ok)
    {
        /* 合理性检查：气压低于 300hPa 视为无效（地球表面最低约 300hPa） */
        if (s_bmp.pressure < 30000)
            bmp_ok = 0;
        else
            printf("| Prs: %4ld.%02ldhPa        |\r\n",
                   (long)(s_bmp.pressure / 100), (long)(s_bmp.pressure % 100));
    }

    if (!bmp_ok)
    {
        printf("| Prs: -----hPa        |\r\n");
        /* 总线恢复后重新初始化 BMP280 校准系数 */
        BMP280_Init();
    }

    if (bh_ok)
      printf("| Lux: %4dlx            |\r\n", s_bh.lux);
    else
    {
      printf("| Lux: ----lx            |\r\n");
      /* 总线恢复后重新初始化 BH1750 */
      BH1750_Init();
    }

    printf("| eCO2:%5dppm TVOC:%4dppb |\r\n", s_sgp.eco2_ppm, s_sgp.tvoc_ppb);
    printf("| Snd: %4u (%3u%%)       |\r\n", s_mic.level, s_mic.level_pct);
    printf("| LoRa: %s           |\r\n", s_lora_ok_last ? "OK" : "FAIL");
    printf("+-------------------------+\r\n");
    BSP_LED_Toggle(LED3);

    /* ===== LoRa 发送传感器数据 ===== */
    {
        /* 打包格式：Tmp(2B)+Hum(2B)+Prs(4B)+Lux(2B)+eCO2(2B)=12B */
        uint8_t lora_pkt[12];
        int16_t  tmp = (int16_t)(s_dht.temperature_int * 10 + s_dht.temperature_dec);
        uint16_t hum = (uint16_t)(s_dht.humidity_int * 10 + s_dht.humidity_dec);
        uint32_t prs = (uint32_t)s_bmp.pressure;
        uint16_t lux = s_bh.lux;
        uint16_t eco2 = (uint16_t)s_sgp.eco2_ppm;

        lora_pkt[0] = (uint8_t)(tmp >> 8);
        lora_pkt[1] = (uint8_t)(tmp);
        lora_pkt[2] = (uint8_t)(hum >> 8);
        lora_pkt[3] = (uint8_t)(hum);
        lora_pkt[4] = (uint8_t)(prs >> 24);
        lora_pkt[5] = (uint8_t)(prs >> 16);
        lora_pkt[6] = (uint8_t)(prs >> 8);
        lora_pkt[7] = (uint8_t)(prs);
        lora_pkt[8] = (uint8_t)(lux >> 8);
        lora_pkt[9] = (uint8_t)(lux);
        lora_pkt[10] = (uint8_t)(eco2 >> 8);
        lora_pkt[11] = (uint8_t)(eco2);

        if (LoRa_TX_Send(lora_pkt, sizeof(lora_pkt)))
        {
            s_lora_ok_last = 1;
            s_lora_count++;
        }
        else
        {
            s_lora_ok_last = 0;
        }
    }

    /* ===== Save to log ===== */
    LogCurrentData();

    /* ===== Refresh OLED ===== */
    {
        static uint8_t last_page = 255;
        static uint32_t last_draw = 0;
        uint32_t now_tick = HAL_GetTick();

        if (s_page != last_page || now_tick - last_draw > 5000)
        {
            last_page = s_page;
            last_draw = now_tick;
            SSD1306_Clear();

            switch (s_page)
            {
                case 0:
                {
                    char lb[22];
                    SSD1306_SetCursor(0, 0); SSD1306_WriteString("===Sensor===");
                    snprintf(lb, sizeof(lb), "CO2:%05dpp TV:%04d", s_sgp.eco2_ppm, s_sgp.tvoc_ppb);
                    SSD1306_SetCursor(1, 0); SSD1306_WriteString(lb);
                    snprintf(lb, sizeof(lb), "Tmp:%d.%dC", s_dht.temperature_int, s_dht.temperature_dec);
                    SSD1306_SetCursor(2, 0); SSD1306_WriteString(lb);
                    snprintf(lb, sizeof(lb), "Hum:%d%%", s_dht.humidity_int);
                    SSD1306_SetCursor(3, 0); SSD1306_WriteString(lb);
                    snprintf(lb, sizeof(lb), "Prs:%ldhPa", (long)(s_bmp.pressure / 100));
                    SSD1306_SetCursor(4, 0); SSD1306_WriteString(lb);
                    snprintf(lb, sizeof(lb), "Lux:%04dlx", s_bh.lux);
                    SSD1306_SetCursor(5, 0); SSD1306_WriteString(lb);
                    snprintf(lb, sizeof(lb), "Snd:%04u  %3u%%", s_mic.level, s_mic.level_pct);
                    SSD1306_SetCursor(6, 0); SSD1306_WriteString(lb);
                    break;
                }
                case 1:
                {
                    int32_t vals[DATA_LOG_SIZE];
                    uint8_t n = DataLog_GetCount(&s_log);
                    int32_t vmin, vmax;
                    DataLog_GetMinMax(&s_log, FIELD_TEMP, &vmin, &vmax);
                    for (uint8_t i = 0; i < n; i++) vals[i] = DataLog_GetValue(&s_log, i, FIELD_TEMP);
                    Chart_Draw("Temp", "C", vals, n, vmin, vmax, 10);
                    break;
                }
                case 2:
                {
                    int32_t vals[DATA_LOG_SIZE];
                    uint8_t n = DataLog_GetCount(&s_log);
                    int32_t vmin, vmax;
                    DataLog_GetMinMax(&s_log, FIELD_HUMI, &vmin, &vmax);
                    for (uint8_t i = 0; i < n; i++) vals[i] = DataLog_GetValue(&s_log, i, FIELD_HUMI);
                    Chart_Draw("Humi", "%/10", vals, n, vmin, vmax, 10);
                    break;
                }
                case 3:
                {
                    int32_t vals[DATA_LOG_SIZE];
                    uint8_t n = DataLog_GetCount(&s_log);
                    int32_t vmin, vmax;
                    DataLog_GetMinMax(&s_log, FIELD_LIGHT, &vmin, &vmax);
                    for (uint8_t i = 0; i < n; i++) vals[i] = DataLog_GetValue(&s_log, i, FIELD_LIGHT);
                    Chart_Draw("Light", "lx", vals, n, vmin, vmax, 1);
                    break;
                }
                case 4:
                {
                    int32_t vals[DATA_LOG_SIZE];
                    uint8_t n = DataLog_GetCount(&s_log);
                    int32_t vmin, vmax;
                    DataLog_GetMinMax(&s_log, FIELD_PM25, &vmin, &vmax);
                    for (uint8_t i = 0; i < n; i++) vals[i] = DataLog_GetValue(&s_log, i, FIELD_PM25);
                    Chart_Draw("PM2.5", "ug", vals, n, vmin, vmax, 1);
                    break;
                }
            }
            SSD1306_Update();
        }
    }

    HAL_Delay(800);
    /* USER CODE END WHILE */
    MX_SubGHz_Phy_Process();

    /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_8;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 24;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the SYSCLKSource, HCLK, PCLK1 and PCLK2 clocks dividers
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK3|RCC_CLOCKTYPE_HCLK
                              |RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
                              |RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* ==================================================================
 *  printf 重定向实现
 *  将 C 标准库的 printf 输出重定向到 USART2 串口
 *  支持三种编译器：IAR, ARMCC, GCC
 * ==================================================================*/
#if defined(__ICCARM__)
/**
  * @brief  Retargets the C library __write function to the IAR function iar_fputc.
  * @param  file: file descriptor.
  * @param  ptr: pointer to the buffer where the data is stored.
  * @param  len: length of the data to write in bytes.
  * @retval length of the written data in bytes.
  */
size_t __write(int file, unsigned char const *ptr, size_t len)
{
  size_t idx;
  unsigned char const *pdata = ptr;

  for (idx = 0; idx < len; idx++)
  {
    iar_fputc((int)*pdata);
    pdata++;
  }
  return len;
}
#endif /* __ICCARM__ */

/**
  * @brief  Retargets the C library printf function to the USART.
  *        通过 USART2 发送单个字符，实现 printf 的重定向。
  *        串口参数：115200-7-O-1
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART2 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  /* Turn LED3 on */
  BSP_LED_On(LED3);
  while (1);
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
    Button_Scan();  /* 扫描按键 */
    if (s_btn_press)
    {
        s_btn_press = 0;
        s_page = (s_page + 1) % 5;  /* 0~4 循环切换 */
        printf("Page: %d\r\n", s_page);
    }
  }
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
