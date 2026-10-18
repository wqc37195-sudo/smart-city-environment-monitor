#ifndef SENSOR_PROTOCOL_H
#define SENSOR_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>

#define SENSOR_LEGACY_SIZE 12u
#define SENSOR_FRAME_SIZE 28u
#define SENSOR_VERSION 1u
#define SENSOR_VALID_TEMP  0x01u
#define SENSOR_VALID_HUM   0x02u
#define SENSOR_VALID_PRESS 0x04u
#define SENSOR_VALID_LIGHT 0x08u
#define SENSOR_VALID_ECO2  0x10u
#define SENSOR_VALID_ALL   0x1fu

typedef struct {
  int16_t temperature_deci_c;
  uint16_t humidity_deci_pct;
  uint32_t pressure_pa;
  uint16_t light_lux;
  uint16_t eco2_ppm;
  uint8_t version;
  uint8_t valid_mask;
  uint32_t sequence;
  uint32_t sample_uptime_ms;
  uint32_t source_age_ms;
} SensorFrame;

static inline uint16_t sensor_u16(const uint8_t *b)
{
  return (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
}
static inline uint32_t sensor_u32(const uint8_t *b)
{
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
         ((uint32_t)b[2] << 8) | b[3];
}
static inline void sensor_put16(uint8_t *b, uint16_t v)
{
  b[0] = (uint8_t)(v >> 8); b[1] = (uint8_t)v;
}
static inline void sensor_put32(uint8_t *b, uint32_t v)
{
  b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16);
  b[2] = (uint8_t)(v >> 8); b[3] = (uint8_t)v;
}

/* Decode atomically. Legacy frames cannot assert measurement validity. */
static inline int sensor_decode(const uint8_t *b, size_t n, SensorFrame *out)
{
  SensorFrame f = {0};
  const uint8_t *p;
  uint16_t raw_temp;
  if (!b || !out) return 0;
  if (n == SENSOR_FRAME_SIZE) {
    if (b[0] != 'S' || b[1] != 'C' || b[2] != SENSOR_VERSION ||
        (b[3] & ~SENSOR_VALID_ALL)) return 0;
    f.version = b[2]; f.valid_mask = b[3];
    f.sequence = sensor_u32(b + 4);
    f.sample_uptime_ms = sensor_u32(b + 8);
    f.source_age_ms = sensor_u32(b + 12);
    p = b + 16;
  } else if (n == SENSOR_LEGACY_SIZE) {
    p = b;
  } else return 0;
  raw_temp = sensor_u16(p);
  f.temperature_deci_c = (int16_t)(raw_temp <= 32767u ? (int32_t)raw_temp : (int32_t)raw_temp - 65536);
  f.humidity_deci_pct = sensor_u16(p + 2);
  f.pressure_pa = sensor_u32(p + 4);
  f.light_lux = sensor_u16(p + 8);
  f.eco2_ppm = sensor_u16(p + 10);
  if ((f.version == 0 || (f.valid_mask & SENSOR_VALID_HUM)) &&
      f.humidity_deci_pct > 1000u) return 0;
  *out = f;
  return 1;
}

/* Invalid values are zeroed on the wire; only the mask grants validity. */
static inline void sensor_encode(uint8_t b[SENSOR_FRAME_SIZE], const SensorFrame *f)
{
  b[0] = 'S'; b[1] = 'C'; b[2] = SENSOR_VERSION;
  b[3] = f->valid_mask & SENSOR_VALID_ALL;
  sensor_put32(b + 4, f->sequence);
  sensor_put32(b + 8, f->sample_uptime_ms);
  sensor_put32(b + 12, f->source_age_ms);
  sensor_put16(b + 16, (f->valid_mask & SENSOR_VALID_TEMP) ? (uint16_t)f->temperature_deci_c : 0);
  sensor_put16(b + 18, (f->valid_mask & SENSOR_VALID_HUM) ? f->humidity_deci_pct : 0);
  sensor_put32(b + 20, (f->valid_mask & SENSOR_VALID_PRESS) ? f->pressure_pa : 0);
  sensor_put16(b + 24, (f->valid_mask & SENSOR_VALID_LIGHT) ? f->light_lux : 0);
  sensor_put16(b + 26, (f->valid_mask & SENSOR_VALID_ECO2) ? f->eco2_ppm : 0);
}
#endif
