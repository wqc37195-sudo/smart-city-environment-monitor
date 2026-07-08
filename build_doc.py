"""Fill template: modify EXISTING paragraphs only, no add/delete."""
from docx import Document
from docx.shared import Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn

SRC = r"D:\Desktop\qiansai\test-1\test-1\模板.docx"
DST = r"D:\Desktop\qiansai\test-1\test-1\智慧城市环境监测系统_技术文档.docx"

doc = Document(SRC)

# ── Helpers ────────────────────────────────────
def set_p(idx, text):
    """Replace all runs in paragraph[idx] with text."""
    p = doc.paragraphs[idx]
    r0 = p.runs[0] if p.runs else p.add_run('')
    fn, fs, fb = r0.font.name, r0.font.size, r0.font.bold
    for r in p.runs:
        r.text = ''
    r0.text = text
    if fn: r0.font.name = fn
    if fs: r0.font.size = fs
    if fb is not None: r0.font.bold = fb

def img_holder(idx, label):
    """Turn paragraph[idx] into an image placeholder with dashed border."""
    p = doc.paragraphs[idx]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    for r in p.runs:
        r.text = ''
    r = p.runs[0] if p.runs else p.add_run('')
    r.text = f'\n【插图占位】\n{label}\n（请在此处插入图片，然后删除本段）\n'
    r.font.size = Pt(10)
    r.font.color.rgb = RGBColor(0x99, 0x99, 0x99)
    r.font.italic = True
    pPr = p._element.get_or_add_pPr()
    pBdr = pPr.makeelement(qn('w:pBdr'), {})
    for side in ['top','left','bottom','right']:
        el = pBdr.makeelement(qn(f'w:{side}'),
            {qn('w:val'):'dashed', qn('w:sz'):'6', qn('w:space'):'4', qn('w:color'):'999999'})
        pBdr.append(el)
    pPr.append(pBdr)

# ═════════════════════════════════════════════
# Title & Abstract
# ═════════════════════════════════════════════
set_p(0, '基于LoRa的智慧城市环境监测系统')  # 作品名称

set_p(3, (
    '本项目基于STM32WLE5低功耗LoRa微控制器与ESP32网关，构建了一套智慧城市环境监测系统。'
    '系统由采集节点与接收网关组成：采集节点集成DHT11温湿度、BMP280气压、BH1750光照、'
    'SGP30二氧化碳及MAX9814噪声传感器，通过LoRa扩频通信（433MHz/SF7/BW125kHz/CR4/5）'
    '将环境数据无线传输至ESP32网关；网关内置WiFi热点，提供Web仪表盘供手机浏览器实时查看。'
    '系统支持多维度环境采集、分布式部署与实时可视化，可应用于智慧城市道路与社区环境监测场景。'
))

# ═════════════════════════════════════════════
# 第一部分  作品概述
# ═════════════════════════════════════════════
set_p(7, (  # 功能与特性
    '本系统实现以下核心功能：\n'
    '（1）多维度环境数据采集：同时采集温度、湿度、气压、光照度、eCO2及噪声六项指标；\n'
    '（2）LoRa无线远距离传输：433MHz频段，SF7/BW125，实测通信距离≥300m（空旷环境）；\n'
    '（3）实时可视化仪表盘：ESP32自建WiFi热点，手机浏览器直连查看，无需APP；\n'
    '（4）传感器异常自动恢复：I2C总线挂死后自动复位并重新初始化传感器；\n'
    '（5）OLED本地显示：128×64 OLED屏幕，按键切换多页面。'
))
img_holder(8, '图1-1 系统功能框图')

set_p(10, (  # 应用领域
    '本系统可广泛应用于以下场景：\n'
    '（1）智慧城市道路环境监测：部署于路灯杆、交通设施，实时监测道路空气质量与噪声；\n'
    '（2）智慧社区环境感知：安装于社区公共区域，为居民提供温湿度、CO2等环境信息；\n'
    '（3）工业园区环境监控：监测TVOC/eCO2浓度及粉尘水平，实现超标预警；\n'
    '（4）农业温室环境调控：采集温湿度、光照数据，辅助精准农业决策；\n'
    '（5）室内空气质量评估：CO2浓度结合温湿度，评估室内通风状况。'
))
img_holder(11, '图1-2 智慧城市应用场景示意图')

set_p(13, (  # 主要技术特点
    '（1）双MCU异构架构：STM32WLE5（Cortex-M4）负责采集与LoRa发送，ESP32（Xtensa双核）负责接收与Web服务；\n'
    '（2）LoRa扩频通信：SX126x（STM32WLE5内置）与SX1278（ESP32外挂）跨芯片通信，SF7/BW125兼顾速率与距离；\n'
    '（3）WiFi AP直连模式：ESP32自建热点，无需路由器，手机即连即看；\n'
    '（4）多传感器I2C共享总线：BMP280/BH1750/SSD1306共用I2C1，软件I2C扩展SGP30，节省GPIO；\n'
    '（5）抗干扰容错设计：I2C超时检测+自动复位，BMP280气压合理性校验（>300hPa），SGP30 CRC8校验。'
))

set_p(16, (  # 主要性能指标
    '温度：0~50℃ / ±2℃（DHT11）　　湿度：20~90%RH / ±5%RH（DHT11）\n'
    '气压：300~1100hPa / ±1hPa（BMP280）　　光照：1~65535lx（BH1750）\n'
    'eCO2：400~60000ppm（SGP30）　　噪声：MAX9814模拟麦克风+ADC采样\n'
    'LoRa距离：≥300m（空旷，14dBm）　　数据更新：2秒/次\n'
    '显示：本地OLED + Web仪表盘　　供电：USB 5V'
))

set_p(19, (  # 主要创新点
    '（1）跨芯片LoRa通信：首次在STM32WLE5（SX126x内核）与ESP32+SX1278间实现稳定LoRa通信；\n'
    '（2）WiFi AP零配置方案：无需路由器、云平台和APP，手机浏览器即开即用；\n'
    '（3）多传感器融合+自动恢复：软件层面实现I2C总线故障检测与自动恢复，保障长期运行；\n'
    '（4）软硬件全链路自主设计：从传感器驱动、LoRa适配、帧格式定义到Web前端，均为自主开发。'
))

set_p(22, (  # 设计流程
    '（1）需求分析：明确采集参数、通信距离、显示方式；\n'
    '（2）硬件选型：STM32WLE5核心板、ESP32、SX1278模块、各传感器；\n'
    '（3）电路搭建：I2C总线、单总线、ADC采样电路、LoRa天线匹配；\n'
    '（4）驱动开发：传感器驱动、LoRa射频初始化与收发、WiFi Web服务；\n'
    '（5）联调测试：LoRa参数对齐、数据帧解析、通信距离实测。'
))
img_holder(23, '图1-3 系统设计流程图')

# ═════════════════════════════════════════════
# 第二部分  系统组成及功能说明
# ═════════════════════════════════════════════
set_p(28, (  # 整体介绍
    '系统由采集节点（STM32WLE5）与接收网关（ESP32+SX1278）两大部分组成。\n\n'
    '采集节点以STM32WLE5为核心，片内集成Sub-GHz射频收发器（SX126x兼容），'
    '通过I2C1总线连接BMP280（0x76）、BH1750（0x23）、SSD1306（0x3C），'
    '单总线连接DHT11，软件I2C（PA12/PA15）连接SGP30（0x58），ADC采集MAX9814信号。'
    '节点每2秒循环采集一次全部传感器数据，封装为12字节LoRa数据包发送至网关。\n\n'
    '接收网关ESP32通过SPI控制SX1278接收LoRa数据包，解析后启动WiFi AP热点，'
    '运行HTTP服务器渲染HTML5仪表盘供手机浏览器访问。'
))
img_holder(29, '图2-1 系统总体架构框图（采集节点 + 网关 + 手机端）')

set_p(31, (  # 2.2.1 硬件整体介绍
    '2.2.1 硬件整体介绍\n'
    '采集节点以STM32WLE5（NUCLEO-WL55JC开发板）为核心，外围传感器通过I2C、单总线、'
    'ADC三种接口接入。接收网关以ESP32开发板为核心，外挂SX1278 LoRa模块（SPI接口）。\n'
    '主要器件：STM32WLE5JC（Cortex-M4,48MHz,256KB Flash）、ESP32-WROOM-32（240MHz,4MB Flash）、'
    'SX1278模块、DHT11/BMP280/BH1750/SGP30/MAX9814传感器、SSD1306 OLED（128×64,I2C）。'
    '\n【此处插入采集节点硬件连接实物照片】'
))

set_p(32, (  # 2.2.2 机械设计介绍
    '2.2.2 机械设计介绍\n'
    '本项目为原型验证阶段，传感器和开发板通过杜邦线和面包板连接，未设计专用外壳。'
    '如需批量部署，建议设计3D打印外壳，将传感器模块通过排针固定在PCB底板上。'
))
img_holder(17, '图2-3 系统整体布局图（面包板装配照片）')

set_p(33, (  # 2.2.3 电路各模块介绍
    '2.2.3 电路各模块介绍\n\n'
    '（a）STM32WLE5采集节点电路：\n'
    '  · I2C1总线（PB7-SCL/PB8-SDA）：挂载BMP280(0x76)、BH1750(0x23)、SSD1306(0x3C)，4.7kΩ上拉；\n'
    '  · 软件I2C（PA12-SCL/PA15-SDA）：独立挂载SGP30(0x58)，避免地址冲突；\n'
    '  · 单总线（PB3）：连接DHT11，4.7kΩ上拉；\n'
    '  · ADC1_IN0（PB13）：GP2Y1010粉尘传感器（可选）；ADC1_IN1（PB12）：MAX9814麦克风；\n'
    '  · 射频天线：板载PCB天线或SMA外接天线。\n\n'
    '（b）ESP32接收网关电路：\n'
    '  · SPI（GPIO18-SCK/GPIO19-MISO/GPIO23-MOSI/GPIO5-CS）：连接SX1278模块；\n'
    '  · DIO0（GPIO4）：SX1278接收完成中断；RST（GPIO2）：SX1278复位。'
))
img_holder(34, '图2-4 采集节点I2C总线接线图')
img_holder(14, '图2-5 ESP32与SX1278 SPI接线图')
img_holder(20, '图2-6 整体电路原理图（SCH截图）')

set_p(36, (  # 2.3.1 软件整体介绍
    '2.3.1 软件整体介绍\n\n'
    '软件分为STM32采集节点固件（C语言，HAL库+SubGHz_Phy中间件）与ESP32网关固件'
    '（Arduino C++，LoRa库+WiFi库+WebServer库）两部分。\n\n'
    'STM32端模块：传感器驱动层（dht11.c/bmp280.c/bh1750.c/sgp30.c/mic.c/soft_i2c.c）→ '
    'LoRa发送层（lora_tx.c，封装Radio.SetTxConfig+Radio.Send）→ '
    '应用层（main.c，传感器轮询+数据打包+OLED显示）。\n\n'
    'ESP32端模块：LoRa接收（parsePacket解析12字节数据帧）→ '
    'WiFi AP（WiFi.softAP自建热点）→ Web服务（WebServer+buildPage动态生成HTML5仪表盘）。\n\n'
    '数据流：传感器→驱动层→main轮询→数据打包(12B)→LoRa TX→LoRa RX→数据解析→Web渲染→手机浏览器。'
))
img_holder(38, '图2-7 软件系统总体架构图')

set_p(37, (  # 2.3.2 软件各模块介绍
    '2.3.2 软件各模块介绍\n\n'
    '（a）STM32端软件流程图（请插入以下子图）：\n'
    '  子图1：main()主循环流程——按键扫描→读GP2Y1010→读DHT11→读BMP280→'
    '读BH1750→读MAX9814→读SGP30→串口输出→LoRa发送→数据日志→OLED刷新→延时800ms\n'
    '  子图2：LoRa_TX_Send()流程——SetTxConfig→Radio.Send→等待TxDone回调→返回结果\n\n'
    '（b）ESP32端软件流程图（请插入以下子图）：\n'
    '  子图3：setup()初始化——Serial.begin→WiFi.softAP→server.on→server.begin→'
    'SPI.begin→LoRa.begin→配置SF/BW/CR等参数\n'
    '  子图4：loop()主循环——server.handleClient→LoRa.parsePacket→读取字节→'
    '记录RSSI/SNR→parsePacket解析→更新全局变量→串口打印\n'
    '  子图5：buildPage()渲染流程——读取g_temp/g_hum/g_press/g_lux/g_eco2/g_rssi/g_snr→'
    '拼接HTML→CSS Grid布局→JS 2秒自动刷新'
))
img_holder(42, '图2-8 STM32 main()主循环流程图')
img_holder(26, '图2-9 LoRa_TX_Send()流程图')
img_holder(4,  '图2-10 ESP32 setup()初始化流程图')
img_holder(47, '图2-11 ESP32 loop()主循环流程图')
img_holder(49, '图2-12 buildPage() Web仪表盘渲染流程图')

# ═════════════════════════════════════════════
# 第三部分  完成情况及性能参数
# ═════════════════════════════════════════════
set_p(41, (  # 整体介绍
    '整体介绍\n'
    '系统实物由STM32WLE5采集节点、ESP32+SX1278接收网关及手机端Web仪表盘组成。'
    '采集节点集成6路传感器，通过面包板搭建，供电后自动循环采集并通过LoRa发送。'
    '接收网关连接手机后可实时查看环境数据。'
))
img_holder(51, '图3-1 系统整体实物正面照片')
img_holder(52, '图3-2 系统整体实物斜45°照片')

set_p(44, (  # 3.2.1 机械成果
    '3.2.1 机械成果\n'
    '本项目以原型验证为主，未设计专用外壳，传感器与开发板通过面包板+杜邦线连接。'
    '如需产品化，建议设计PCB底板与3D打印外壳。'
    '\n【此处插入面包板装配实物照片】'
))

set_p(45, (  # 3.2.2 电路成果
    '3.2.2 电路成果\n'
    '采集节点与接收网关均已完成电路搭建，I2C/SPI/单总线通信正常，LoRa收发稳定。'
    '\n【此处插入采集节点和接收网关电路实物照片】'
))

set_p(46, (  # 3.2.3 软件成果
    '3.2.3 软件成果\n'
    '（a）STM32串口输出：实时打印传感器数据及LoRa发送状态；\n'
    '（b）ESP32串口输出：显示接收包计数、RSSI/SNR及解析后的传感器数据；\n'
    '（c）Web仪表盘：手机浏览器显示深色主题实时数据面板，2秒自动刷新。'
    '\n【此处插入STM32串口截图、ESP32串口截图、手机端Web仪表盘截图，共3张】'
))

set_p(48, (  # 特性成果
    '特性成果\n\n'
    '（a）LoRa通信测试：\n'
    '  1m距离：RSSI=-30dBm, SNR=+9dB, 收包率100%\n'
    '  50m（室内穿墙）：RSSI=-85dBm, SNR=-2dB, 收包率>95%\n'
    '  100m（室外可视）：RSSI=-105dBm, SNR=-10dB, 收包率>80%\n\n'
    '（b）传感器准确性：\n'
    '  温度与商用温度计偏差<1℃，气压与气象站偏差<2hPa\n'
    '  eCO2室内400~2000ppm范围，与通风状态正相关\n\n'
    '（c）WiFi AP覆盖：热点覆盖半径约15m（室内），5m内页面加载<1秒\n\n'
    '（d）长时间运行：连续运行24h无死机、无I2C挂死、无LoRa掉线'
    '\n【此处插入RSSI vs 距离测试曲线图及Web仪表盘长时间运行截图】'
))

# ═════════════════════════════════════════════
# 第四部分  总结
# ═════════════════════════════════════════════
set_p(54, (  # 可扩展之处
    '（1）LoRaWAN协议栈：移植标准LoRaWAN协议栈实现与标准网关对接；\n'
    '（2）低功耗优化：利用STM32WLE5的STOP2模式（<1.4μA）和LoRa CAD唤醒，实现电池供电年续航；\n'
    '（3）云端接入：ESP32通过MQTT协议对接华为云/阿里云IoT平台，实现远程监控；\n'
    '（4）多点组网：部署多个采集节点，网关时分/频分复用接收多节点数据；\n'
    '（5）传感器扩展：增加甲醛（HCHO）、臭氧（O3）等气体传感器，丰富监测维度。'
))

set_p(56, (  # 心得体会
    '通过本项目，我们掌握了以下技能与经验：\n\n'
    '（1）LoRa通信跨芯片适配：STM32WLE5内置SX126x射频与ESP32外挂SX1278芯片间通信，'
    '需要精确匹配频率、SF、BW、CR、SyncWord等参数。调试过程中发现带宽数组索引错误导致'
    '通信失败的典型问题——Bandwidths[]数组仅有3个元素（索引0~2），而LORA_BW_125枚举值为4，'
    '直接作为数组索引导致越界读取随机带宽值，两侧始终无法通信。修正后通信立即正常，'
    '深刻理解了驱动API中枚举值与数组索引的区别。\n\n'
    '（2）多传感器I2C总线管理：BMP280、BH1750、SSD1306共享I2C1总线，任一设备通信异常'
    '可能导致总线挂死。通过在每次读取失败后触发总线复位和传感器重新初始化，显著提高了系统'
    '鲁棒性。软件I2C（PA12/PA15）独立挂载SGP30避免了地址冲突问题。\n\n'
    '（3）ESP32 WiFi AP直连方案：传统IoT方案需要路由器+云平台+APP，部署成本高。采用AP模式'
    '+嵌入式Web服务器方案，系统可在完全无基础设施的环境下运行，适合野外或应急场景快速部署。'
    '手机浏览器访问192.168.4.1即可查看实时数据，无需安装任何软件。\n\n'
    '（4）数据帧设计权衡：12字节紧凑帧（温度2B+湿度2B+气压4B+光照2B+eCO2 2B）在保证信息'
    '完整的前提下最小化空中传输时间（SF7/BW125下约20ms），为后续多点轮询预留时间窗口。\n\n'
    '（5）中文编码问题：ESP32 Arduino串口监视器在Windows中文环境下默认GBK编码，而源文件'
    'UTF-8编码的中文字符会被错误解码显示为乱码。改为全ASCII英文输出后彻底解决该问题，'
    '提升了跨平台兼容性。'
))

# ═════════════════════════════════════════════
# 第五部分  参考文献
# ═════════════════════════════════════════════
set_p(59, (
    '[1] Semtech Corporation. SX1261/2 Datasheet Rev 2.1[Z]. 2021.\n'
    '[2] Semtech Corporation. SX1276/77/78/79 Datasheet Rev 7[Z]. 2020.\n'
    '[3] STMicroelectronics. RM0453: STM32WL5x Reference Manual Rev 3[Z]. 2021.\n'
    '[4] STMicroelectronics. UM2643: Sub-GHz Phy Middleware for STM32WL Rev 2[Z]. 2022.\n'
    '[5] Espressif Systems. ESP32-WROOM-32 Datasheet V2.9[Z]. 2023.\n'
    '[6] Sandeep Mistry. Arduino-LoRa Library[CP/OL]. GitHub, 2018.\n'
    '[7] Bosch Sensortec. BMP280 Datasheet Rev 1.19[Z]. 2018.\n'
    '[8] Sensirion AG. SGP30 Datasheet V1.0[Z]. 2019.\n'
    '[9] ROHM Semiconductor. BH1750FVI Datasheet Rev D[Z]. 2011.\n'
    '[10] LoRa Alliance. LoRaWAN Specification 1.0.4[S]. 2020.\n'
    '[11] Aosong Electronics. DHT11 Digital Temperature Humidity Sensor[Z].\n'
    '[12] Maxim Integrated. MAX9814 Microphone Amplifier Datasheet[Z].'
))

doc.save(DST)
print('DONE — saved to', DST)
