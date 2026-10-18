#pragma once
// Host-only doubles: compile and exercise the actual sketch without hardware.
#include <stdint.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <vector>
class String {
public:
  std::string value;
  String() = default;
  String(const char *s): value(s) {}
  String(std::string s): value(s) {}
  template<class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
  String(T n): value(std::to_string(n)) {}
  String(float n, int decimals) {
    std::ostringstream out; out << std::fixed << std::setprecision(decimals) << n;
    value = out.str();
  }
  String& operator+=(const String& s) { value += s.value; return *this; }
};
inline String operator+(const String& a, const String& b) { return a.value + b.value; }
static uint32_t clock_ms;
inline uint32_t millis() { return clock_ms; }
inline void delay(uint32_t n) { clock_ms += n; }
struct SerialMock {
  void begin(int) {}
  template<class... T> void print(T...) {}
  template<class... T> void println(T...) {}
  template<class... T> void printf(T...) {}
};
static SerialMock Serial;
struct SPIMock { void begin(int,int,int,int) {} };
static SPIMock SPI;
struct LoRaMock {
  std::vector<uint8_t> incoming;
  size_t pos = 0;
  void setPins(int,int,int) {}
  bool begin(double) { return true; }
  void setSpreadingFactor(int) {}
  void setSignalBandwidth(double) {}
  void setCodingRate4(int) {}
  void setPreambleLength(int) {}
  void setTxPower(int) {}
  void enableCrc() {}
  int parsePacket() { return pos == 0 ? (int)incoming.size() : 0; }
  int available() { return (int)(incoming.size() - pos); }
  int read() { return incoming.at(pos++); }
  int packetRssi() { return -75; }
  float packetSnr() { return 8.5f; }
  void feed(const uint8_t *b, size_t n) { incoming.assign(b, b + n); pos = 0; }
};
static LoRaMock LoRa;
