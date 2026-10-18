#pragma once
#include "LoRa.h"
#define HTTP_GET 0
class WebServer {
public:
  String body, type, cache;
  int status = 0;
  WebServer(int) {}
  void on(const char*, void(*)()) {}
  void on(const char*, int, void(*)()) {}
  void onNotFound(void(*)()) {}
  void begin() {}
  void handleClient() {}
  void sendHeader(const char*, const char* value) { cache = value; }
  void send(int code, const char* mime, String data) { status=code; type=mime; body=data; }
};
