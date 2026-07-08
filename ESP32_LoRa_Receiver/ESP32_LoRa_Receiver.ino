/**
  ******************************************************************************
  * @file    ESP32_LoRa_Receiver.ino
  * @brief   ESP32 + SX1278 receives sensor data from STM32WL55
  *          + WiFi AP hotspot + Web dashboard for phone
  *
  *  [Hardware Wiring]
  *    ESP32          SX1278
  *    GPIO5    ->    NSS/CS
  *    GPIO18   ->    SCK
  *    GPIO23   ->    MOSI
  *    GPIO19   ->    MISO
  *    GPIO2    ->    RST
  *    GPIO4    ->    DIO0
  *
  *  [Usage]
  *    1. Power on ESP32
  *    2. Phone connects to WiFi: "LoRa-Sensor" password: 12345678
  *    3. Browser open: http://192.168.4.1
  *
  *  [LoRa Parameters] (must match STM32)
  *    Freq:     433 MHz
  *    SF:       7
  *    BW:       125 kHz
  *    CR:       4/5
  *    CRC:      ON
  *
  *  [Data Format (12 bytes)]
  *    [0..1]   Temperature:  int16,  big-endian, value = C * 10
  *    [2..3]   Humidity:     uint16, big-endian, value = % * 10
  *    [4..7]   Pressure:     uint32, big-endian, unit Pa
  *    [8..9]   Light:        uint16, big-endian, unit lux
  *    [10..11] eCO2:         uint16, big-endian, unit ppm
  *
  *  [Required Libraries]
  *    - LoRa by Sandeep Mistry
  *    - WiFi (built-in)
  *    - WebServer (built-in)
  ******************************************************************************
 */

#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>

// ===== WiFi AP settings =====
const char* WIFI_SSID     = "LoRa-Sensor";
const char* WIFI_PASSWORD = "12345678";

WebServer server(80);

// ===== SPI pin definitions =====
#define LORA_CS   5
#define LORA_SCK  18
#define LORA_MOSI 23
#define LORA_MISO 19
#define LORA_RST  2
#define LORA_DIO0 4

// ===== LoRa parameters =====
#define LORA_FREQ       433E6
#define LORA_SF         7
#define LORA_BW         125E3
#define LORA_CR         5
#define LORA_PREAMBLE   8
#define LORA_TX_POWER   17

// ===== Latest sensor data (updated by LoRa Rx, read by web server) =====
static uint32_t    g_packet_count = 0;
static int         g_rssi         = 0;
static float       g_snr          = 0.0;
static float       g_temp         = 0.0;
static float       g_hum          = 0.0;
static uint32_t    g_press_hpa    = 0;
static uint16_t    g_press_dec    = 0;
static uint16_t    g_lux          = 0;
static uint16_t    g_eco2         = 0;
static uint32_t    g_last_update  = 0;

/* ================================================================
 *  Parse received LoRa packet and update global sensor values
 * ================================================================ */
void parsePacket(uint8_t *buf, uint8_t len)
{
  if (len < 12) return;

  int16_t  temp_raw = (int16_t)((buf[0] << 8) | buf[1]);
  uint16_t hum_raw  = (uint16_t)((buf[2] << 8) | buf[3]);
  uint32_t press    = (uint32_t)((buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7]);
  uint16_t lux      = (uint16_t)((buf[8] << 8) | buf[9]);
  uint16_t eco2     = (uint16_t)((buf[10] << 8) | buf[11]);

  g_temp      = temp_raw / 10.0f;
  g_hum       = hum_raw / 10.0f;
  g_press_hpa = press / 100;
  g_press_dec = press % 100;
  g_lux       = lux;
  g_eco2      = eco2;
  g_last_update = millis();
}

/* ================================================================
 *  HTML dashboard page (mobile-friendly, auto-refresh every 2s)
 * ================================================================ */
String buildPage()
{
  uint32_t secs = (millis() - g_last_update) / 1000;

  String html = R"raw(<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>LoRa Sensor Dashboard</title>
<style>
  *{margin:0;padding:0;box-sizing:border-box;}
  body{font-family:'Segoe UI',Arial,sans-serif;background:#0f1923;color:#e0e0e0;
       min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:10px;}
  h1{font-size:1.3em;color:#00e5ff;margin:10px 0;}
  .grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;width:100%;max-width:400px;}
  .card{background:#1a2733;border-radius:12px;padding:14px;text-align:center;
        border:1px solid #2a3a4a;}
  .card.full{grid-column:1/-1;}
  .val{font-size:2em;font-weight:bold;margin:6px 0;}
  .lbl{font-size:0.8em;color:#8899aa;text-transform:uppercase;letter-spacing:1px;}
  .unit{font-size:0.7em;color:#667788;}
  .tmp .val{color:#ff7b42;}
  .hum .val{color:#42c6ff;}
  .prs .val{color:#ffd740;}
  .lux .val{color:#ffe57f;}
  .co2 .val{color:#69f0ae;}
  .sig .val{font-size:1em;color:#69f0ae;}
  .info{font-size:0.7em;color:#556677;margin-top:10px;text-align:center;}
  .bar{height:4px;background:#00e5ff;border-radius:2px;}
</style>
</head>
<body>
<h1>LoRa Sensor</h1>
<div class="grid">
)raw";

  html += "<div class='card tmp'><div class='lbl'>Temperature</div>";
  html += "<div class='val'>" + String(g_temp, 1) + "<span class='unit'> &deg;C</span></div></div>";

  html += "<div class='card hum'><div class='lbl'>Humidity</div>";
  html += "<div class='val'>" + String(g_hum, 1) + "<span class='unit'> %</span></div></div>";

  html += "<div class='card prs'><div class='lbl'>Pressure</div>";
  html += "<div class='val'>" + String(g_press_hpa) + "." + String(g_press_dec / 10) + String(g_press_dec % 10) + "<span class='unit'> hPa</span></div></div>";

  html += "<div class='card lux'><div class='lbl'>Light</div>";
  html += "<div class='val'>" + String(g_lux) + "<span class='unit'> lx</span></div></div>";

  html += "<div class='card co2 full'><div class='lbl'>eCO2</div>";
  html += "<div class='val'>" + String(g_eco2) + "<span class='unit'> ppm</span></div></div>";

  html += "<div class='card sig full'><div class='lbl'>Signal &amp; Packets</div>";
  html += "<div class='val'>RSSI " + String(g_rssi) + " dBm &nbsp; SNR " + String(g_snr, 1) + " dB</div>";
  html += "<div class='lbl'>Packets: " + String(g_packet_count) + " &nbsp; Last: " + String(secs) + "s ago</div></div>";

  html += "</div>";
  html += "<div class='info'>Auto-refresh every 2s &bull; LoRa 433MHz SF7</div>";
  html += "<script>setTimeout(function(){location.reload();},2000);</script>";
  html += "</body></html>";

  return html;
}

/* ================================================================
 *  Web server handlers
 * ================================================================ */
void handleRoot()
{
  server.send(200, "text/html; charset=utf-8", buildPage());
}

void handleNotFound()
{
  server.send(404, "text/plain", "404 Not Found");
}

/* ================================================================
 *  Initialization
 * ================================================================ */
void setup()
{
  Serial.begin(115200);

  // --- WiFi AP ---
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.println();
  Serial.println("=== WiFi AP Started ===");
  Serial.print("SSID: "); Serial.println(WIFI_SSID);
  Serial.print("IP:   "); Serial.println(ip);
  Serial.println("=======================");

  // --- Web Server ---
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started on port 80");

  // --- LoRa ---
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQ))
  {
    Serial.println("[FAIL] LoRa init failed!");
    while (1) delay(1000);
  }

  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setPreambleLength(LORA_PREAMBLE);
  LoRa.setTxPower(LORA_TX_POWER);
  LoRa.enableCrc();

  Serial.println("[OK] LoRa receiver ready");
  Serial.println("[OK] Open http://" + ip.toString() + " on your phone");
  Serial.println();
}

/* ================================================================
 *  Main loop
 * ================================================================ */
void loop()
{
  // --- Handle web clients ---
  server.handleClient();

  // --- Handle LoRa packets ---
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0)
  {
    uint8_t buf[64];
    uint8_t idx = 0;

    while (LoRa.available() && idx < sizeof(buf))
    {
      buf[idx++] = (uint8_t)LoRa.read();
    }

    g_rssi = LoRa.packetRssi();
    g_snr  = LoRa.packetSnr();
    g_packet_count++;

    parsePacket(buf, idx);

    // Serial output
    Serial.printf("[#%u] RSSI=%d SNR=%.1f | T=%.1fC H=%.1f%% P=%u.%02uhPa L=%ulx CO2=%uppm\r\n",
                  g_packet_count, g_rssi, g_snr,
                  g_temp, g_hum, g_press_hpa, g_press_dec, g_lux, g_eco2);
  }
}
