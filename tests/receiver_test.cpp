#include "../ESP32_LoRa_Receiver/ESP32_LoRa_Receiver.ino"
#include <cassert>
#include <iostream>

static void report(const char* name)
{
  handleStatus();
  assert(server.status == 200 && server.type.value == "application/json");
  assert(server.cache.value == "no-store");
  std::cout << name << " " << server.body.value << "\n";
}
int main()
{
  setup();
  report("boot");
  assert(buildPage().value.find("WAITING") != std::string::npos);
  SensorFrame f = {};
  uint8_t b[SENSOR_FRAME_SIZE];
  f.valid_mask = SENSOR_VALID_ALL;
  f.sequence = 7; f.sample_uptime_ms = 200; f.source_age_ms = 100;
  f.temperature_deci_c = 253; f.humidity_deci_pct = 550;
  f.pressure_pa = 101325; f.light_lux = 0; f.eco2_ppm = 500;
  sensor_encode(b, &f);
  clock_ms = 1000; LoRa.feed(b, sizeof(b)); loop(); report("valid");
  clock_ms += 9900; report("boundary");
  clock_ms += 1; report("stale");
  assert(buildPage().value.find("STALE") != std::string::npos);
  assert(buildPage().value.find("25.3") == std::string::npos);
  f.valid_mask = SENSOR_VALID_LIGHT; f.sequence++;
  sensor_encode(b, &f); LoRa.feed(b, sizeof(b)); loop(); report("partial");
  assert(buildPage().value.find("25.3") == std::string::npos);
  uint32_t previous_update = g_last_update;
  b[2] = 99; clock_ms += 400; LoRa.feed(b, sizeof(b)); loop(); report("malformed");
  assert(g_last_update == previous_update && g_invalid_count == 1);
  LoRa.feed(b, 13); loop(); assert(g_invalid_count == 2);
  f.valid_mask = SENSOR_VALID_ALL; sensor_encode(b, &f);
  LoRa.feed(b + 16, 12); loop(); report("legacy");
  assert(buildPage().value.find("UNKNOWN") != std::string::npos);
  clock_ms = UINT32_MAX - 20; LoRa.feed(b, sizeof(b)); loop();
  clock_ms = 30; report("wrap");
  clock_ms += 10000; report("wrap_stale");
  clock_ms = UINT32_MAX - 20; report("latched_stale");
  f.source_age_ms = UINT32_MAX; sensor_encode(b, &f);
  LoRa.feed(b, sizeof(b)); loop(); clock_ms += 10; report("saturated_age");
  f.source_age_ms = 10; f.valid_mask = 0; sensor_encode(b, &f);
  LoRa.feed(b, sizeof(b)); loop(); report("all_failed");
  return 0;
}
