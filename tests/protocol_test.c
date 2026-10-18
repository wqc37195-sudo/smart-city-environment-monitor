#include "../Inc/sensor_protocol.h"
#include <assert.h>
#include <string.h>

int main(void)
{
  SensorFrame f = {0}, decoded = {0}, saved;
  uint8_t b[SENSOR_FRAME_SIZE];
  const uint8_t golden[] = {
    0x53,0x43,1,31, 1,2,3,4, 0,0,3,232, 0,0,0,100,
    0xff,0xc9, 0x02,0x2b, 0,1,0x8b,0xcd, 0,0, 1,0xf4
  };
  f.valid_mask = SENSOR_VALID_ALL;
  f.sequence = 0x01020304; f.sample_uptime_ms = 1000; f.source_age_ms = 100;
  f.temperature_deci_c = -55; f.humidity_deci_pct = 555;
  f.pressure_pa = 101325; f.light_lux = 0; f.eco2_ppm = 500;
  sensor_encode(b, &f);
  assert(sizeof(golden) == sizeof(b) && memcmp(golden, b, sizeof(b)) == 0);
  assert(sensor_decode(b, sizeof(b), &decoded));
  assert(decoded.temperature_deci_c == -55 && decoded.sequence == 0x01020304);
  assert(decoded.pressure_pa == 101325 && decoded.valid_mask == SENSOR_VALID_ALL);
  saved = decoded;
  for (size_t n = 0; n < 40; ++n) {
    if (n == SENSOR_LEGACY_SIZE || n == SENSOR_FRAME_SIZE) continue;
    assert(!sensor_decode(b, n, &decoded));
    assert(memcmp(&saved, &decoded, sizeof(saved)) == 0);
  }
  assert(!sensor_decode(NULL, sizeof(b), &decoded));
  assert(!sensor_decode(b, sizeof(b), NULL));
  b[0] = 0; assert(!sensor_decode(b, sizeof(b), &decoded)); b[0] = 'S';
  b[2] = 2; assert(!sensor_decode(b, sizeof(b), &decoded)); b[2] = 1;
  b[3] = 128; assert(!sensor_decode(b, sizeof(b), &decoded));
  f.humidity_deci_pct = 1001;
  sensor_encode(b, &f); assert(!sensor_decode(b, sizeof(b), &decoded));
  f.valid_mask = SENSOR_VALID_LIGHT; /* stale numbers must not escape as readings */
  sensor_encode(b, &f); assert(sensor_decode(b, sizeof(b), &decoded));
  assert(decoded.temperature_deci_c == 0 && decoded.humidity_deci_pct == 0);
  assert(decoded.pressure_pa == 0 && decoded.eco2_ppm == 0);
  assert(decoded.valid_mask == SENSOR_VALID_LIGHT && decoded.light_lux == 0);
  assert(sensor_decode(golden + 16, 12, &decoded));
  assert(decoded.version == 0 && decoded.valid_mask == 0 && decoded.sequence == 0);
  return 0;
}
