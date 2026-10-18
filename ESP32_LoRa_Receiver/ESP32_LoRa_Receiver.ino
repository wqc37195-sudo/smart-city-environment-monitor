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
  *  [Data Format]
  *    v1: 28 bytes, version + per-field validity + sequence + sample timing.
  *    Legacy 12-byte frames are accepted with UNKNOWN measurement validity.
  *    See README.md and sensor_protocol.h for the exact wire format.
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
#include "sensor_protocol.h"

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
static SensorFrame g_frame = {};
static bool        g_expired = true;
static bool        g_has_sample   = false;
static uint32_t    g_invalid_count = 0;
static const uint32_t STALE_AFTER_MS = 10000;

/* ================================================================
 *  Parse received LoRa packet and update global sensor values
 * ================================================================ */
bool parsePacket(uint8_t *buf, uint8_t len)
{
  SensorFrame f;
  if (!sensor_decode(buf, len, &f)) return false;

  int16_t temp_raw = f.temperature_deci_c;
  uint16_t hum_raw = f.humidity_deci_pct;
  uint32_t press = f.pressure_pa;
  uint16_t lux = f.light_lux;
  uint16_t eco2 = f.eco2_ppm;

  g_frame = f;
  g_expired = false;
  g_temp      = temp_raw / 10.0f;
  g_hum       = hum_raw / 10.0f;
  g_press_hpa = press / 100;
  g_press_dec = press % 100;
  g_lux       = lux;
  g_eco2      = eco2;
  g_last_update = millis();
  g_has_sample = true;
  return true;
}

/* Age includes acquisition time before transmission, but not radio airtime.
 * Unsigned subtraction handles millis() wrap. Expiry latches until a new frame. */
uint32_t sampleAge(uint32_t now)
{
  uint32_t elapsed = (uint32_t)(now - g_last_update);
  if (UINT32_MAX - elapsed < g_frame.source_age_ms) return UINT32_MAX;
  return elapsed + g_frame.source_age_ms;
}

bool sampleStale(uint32_t now)
{
  if (!g_has_sample || sampleAge(now) > STALE_AFTER_MS) g_expired = true;
  return g_expired;
}

uint8_t usableMask(uint32_t now)
{
  return (!sampleStale(now) && g_frame.version == SENSOR_VERSION) ? g_frame.valid_mask : 0;
}

/* ================================================================
 *  HTML dashboard page (mobile-friendly, auto-refresh every 2s)
 * ================================================================ */
String buildPage()
{
  uint32_t now = millis();
  uint32_t secs = sampleAge(now) / 1000;
  uint8_t valid = usableMask(now);

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
  html += "<div class='val'>" + ((valid & SENSOR_VALID_TEMP) ? String(g_temp, 1) : String("--")) + "<span class='unit'> &deg;C</span></div></div>";

  html += "<div class='card hum'><div class='lbl'>Humidity</div>";
  html += "<div class='val'>" + ((valid & SENSOR_VALID_HUM) ? String(g_hum, 1) : String("--")) + "<span class='unit'> %</span></div></div>";

  html += "<div class='card prs'><div class='lbl'>Pressure</div>";
  html += "<div class='val'>" + ((valid & SENSOR_VALID_PRESS) ? String(g_frame.pressure_pa / 100.0f, 2) : String("--")) + "<span class='unit'> hPa</span></div></div>";

  html += "<div class='card lux'><div class='lbl'>Light</div>";
  html += "<div class='val'>" + ((valid & SENSOR_VALID_LIGHT) ? String(g_lux) : String("--")) + "<span class='unit'> lx</span></div></div>";

  html += "<div class='card co2 full'><div class='lbl'>eCO2</div>";
  html += "<div class='val'>" + ((valid & SENSOR_VALID_ECO2) ? String(g_eco2) : String("--")) + "<span class='unit'> ppm</span></div></div>";

  html += "<div class='card sig full'><div class='lbl'>Signal &amp; Packets</div>";
  html += "<div class='val'>RSSI " + String(g_rssi) + " dBm &nbsp; SNR " + String(g_snr, 1) + " dB</div>";
  html += "<div class='lbl'>Packets: " + String(g_packet_count) + " &nbsp; Last: " + (g_has_sample ? String(secs) : String("--")) + "s ago</div></div>";

  html += "</div>";
  html += "<p>State: " + String(!g_has_sample ? "WAITING / no sample" :
            (sampleStale(now) ? "STALE" :
             (g_frame.version == 0 ? "UNKNOWN / legacy frame" :
              (valid == SENSOR_VALID_ALL ? "VALID" : "PARTIAL / sensor unavailable")))) + "</p>";
  html += "<p>eCO2 is an air-quality estimate. Not a gas-leak alarm.</p>";
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

void handleStatus()
{
  uint32_t now = millis();
  bool stale = sampleStale(now);
  uint8_t valid = usableMask(now);
  bool versioned = g_has_sample && g_frame.version == SENSOR_VERSION;
  String body = "{\"schema\":\"sensor-status-v1\",\"has_sample\":";
  body += g_has_sample ? "true" : "false";
  body += ",\"protocol_version\":" + (g_has_sample ? String(g_frame.version) : String("null"));
  body += ",\"stale\":";
  body += stale ? "true" : "false";
  body += ",\"age_ms\":" + (g_has_sample ? String(sampleAge(now)) : String("null"));
  body += ",\"received_age_ms\":" + (g_has_sample ? String((uint32_t)(now - g_last_update)) : String("null"));
  body += ",\"sequence\":" + (versioned ? String(g_frame.sequence) : String("null"));
  body += ",\"sample_uptime_ms\":" + (versioned ? String(g_frame.sample_uptime_ms) : String("null"));
  body += ",\"source_age_ms\":" + (versioned ? String(g_frame.source_age_ms) : String("null"));
  body += ",\"source_valid_mask\":" + (versioned ? String(g_frame.valid_mask) : String("null"));
  body += ",\"valid\":{\"temperature_c\":";
  body += (valid & SENSOR_VALID_TEMP) ? "true" : "false";
  body += ",\"humidity_pct\":";
  body += (valid & SENSOR_VALID_HUM) ? "true" : "false";
  body += ",\"pressure_pa\":";
  body += (valid & SENSOR_VALID_PRESS) ? "true" : "false";
  body += ",\"light_lux\":";
  body += (valid & SENSOR_VALID_LIGHT) ? "true" : "false";
  body += ",\"eco2_ppm\":";
  body += (valid & SENSOR_VALID_ECO2) ? "true" : "false";
  body += "}";
  body += ",\"valid_packets\":" + String(g_packet_count);
  body += ",\"invalid_packets\":" + String(g_invalid_count);
  body += ",\"temperature_c\":" + ((valid & SENSOR_VALID_TEMP) ? String(g_temp, 1) : String("null"));
  body += ",\"humidity_pct\":" + ((valid & SENSOR_VALID_HUM) ? String(g_hum, 1) : String("null"));
  body += ",\"pressure_pa\":" + ((valid & SENSOR_VALID_PRESS) ? String(g_frame.pressure_pa) : String("null"));
  body += ",\"light_lux\":" + ((valid & SENSOR_VALID_LIGHT) ? String(g_lux) : String("null"));
  body += ",\"eco2_ppm\":" + ((valid & SENSOR_VALID_ECO2) ? String(g_eco2) : String("null"));
  body += ",\"rssi_dbm\":" + (g_has_sample ? String(g_rssi) : String("null"));
  body += ",\"snr_db\":" + (g_has_sample ? String(g_snr, 1) : String("null"));
  body += ",\"authenticated\":false,\"safety_control_eligible\":false}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
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
  server.on("/api/status", HTTP_GET, handleStatus);
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
  sampleStale(millis());
  server.handleClient();

  // --- Handle LoRa packets ---
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0)
  {
    if (packetSize != SENSOR_LEGACY_SIZE && packetSize != SENSOR_FRAME_SIZE)
    {
      while (LoRa.available()) LoRa.read();
      g_invalid_count++;
      return;
    }
    uint8_t buf[64];
    uint8_t idx = 0;

    while (LoRa.available() && idx < sizeof(buf))
    {
      buf[idx++] = (uint8_t)LoRa.read();
    }

    if (!parsePacket(buf, idx))
    {
      g_invalid_count++;
      return;
    }
    g_rssi = LoRa.packetRssi();
    g_snr  = LoRa.packetSnr();
    g_packet_count++;

    // Serial output
    Serial.printf("[#%u] RSSI=%d SNR=%.1f | T=%.1fC H=%.1f%% P=%u.%02uhPa L=%ulx CO2=%uppm\r\n",
                  g_packet_count, g_rssi, g_snr,
                  g_temp, g_hum, g_press_hpa, g_press_dec, g_lux, g_eco2);
  }
}
