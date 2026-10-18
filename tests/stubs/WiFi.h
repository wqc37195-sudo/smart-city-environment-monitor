#pragma once
#include "LoRa.h"
struct IPAddress { String toString() { return "192.168.4.1"; } };
struct WiFiMock {
  void softAP(const char*, const char*) {}
  IPAddress softAPIP() { return {}; }
};
static WiFiMock WiFi;
