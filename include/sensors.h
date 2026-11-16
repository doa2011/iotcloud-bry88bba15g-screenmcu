#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Stock config +4/+6/+8/+10, signed values in tenths. Do not invent board defaults. */
typedef struct { int16_t temperature, secondary, hcho_heat, humidity; } sensor_sht_config;
typedef struct {
 uint32_t samples;
 double base, hcho;
 bool settled;
} sensor_sht_state;
typedef struct {
 int32_t temperature10, humidity10, raw_temperature10, raw_humidity10, unclamped_temperature10;
} sensor_sht_result;
/* One successful SHT measurement per call, not one call per RTOS tick.
 * CRC failure leaves both state and result unchanged. ramp/force reflect stock
 * flags +0x64/+0x65. No elapsed-wall-time approximation is substituted. */
bool sensor_sht_compensate(sensor_sht_state *,const sensor_sht_config *,
 const uint8_t frame[6],uint8_t hcho_status,bool ramp,bool force,sensor_sht_result *);
/* UART3 CMD=1 value and raw status. Wire units intentionally not guessed. */
bool sensor_hcho(const uint8_t *,unsigned,uint16_t *,uint8_t *);
/* Original 400 ms slots: HCHO, CO2, SHT command/read, AM1008L.
 * A successful SHT result therefore advances compensation every 3.2 s. */
enum { SENSOR_POLL_HCHO, SENSOR_POLL_CO2, SENSOR_POLL_SHT_COMMAND,
       SENSOR_POLL_AIR, SENSOR_POLL_SHT_READ };
typedef struct { uint8_t slot; bool sht_read; } sensor_poll_state;
unsigned sensor_poll_next(sensor_poll_state *);
uint16_t sensor_pm25(uint16_t raw, uint16_t coefficient);
uint16_t sensor_co2(uint16_t raw);
/* SV.04.00.04.00 0x4e374: mode 3/2/1 requests 60/30/5 minute warm-up.
 * UTC/retained stops use seconds; warmup uses wrap-safe monotonic milliseconds.
 * Only a transition from ready to stopped requests a
 * retained timestamp write; callers must not save on every poll. */
typedef struct {
 uint32_t polls, started, last_stop;
 int16_t warmup_minutes; /* -1=factory phase, 1..60=custom measurement warmup */
 uint8_t previous, phase, mode, received, missed, pm_status, tvoc_status;
} sensor_air_state;
void sensor_air_init(sensor_air_state *, uint32_t last_stop);
void sensor_air_receive(sensor_air_state *, bool measurement);
/* Measurement warmup, independent of the calibration admission timer.
 * False means stopped/unknown; zero remaining means the phase has elapsed,
 * not that a fresh valid measurement has already arrived. */
bool sensor_air_warmup(const sensor_air_state *, uint32_t monotonic_ms,
 uint32_t *duration_ms, uint32_t *elapsed_ms, uint32_t *remaining_ms);
bool sensor_air_update(sensor_air_state *, uint32_t utc, uint32_t monotonic_ms);
unsigned sensor_air_poll(sensor_air_state *, uint32_t utc, uint32_t monotonic_ms, uint16_t co2,
 uint16_t hcho, uint8_t hcho_status, int32_t temperature10, int32_t humidity10,
 uint8_t frame[12], bool *save_stop);
