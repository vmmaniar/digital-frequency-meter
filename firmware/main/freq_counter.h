#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    FREQ_MODE_DIRECT = 0,   // direct frequency counting (>= 100 Hz)
    FREQ_MODE_PERIOD = 1,   // reciprocal-period measurement (< 100 Hz)
} freq_mode_t;

typedef struct {
    int input_gpio;          // signal under test
    int gate_seconds;        // gating window length (>=1)
    uint16_t high_limit;     // PCNT internal threshold for overflow accounting
} freq_counter_cfg_t;

typedef struct {
    double frequency_hz;     // measured frequency
    double elapsed_s;        // actual gate window (sanity check)
    uint32_t count;          // total counted edges in the gate window
    freq_mode_t mode;        // direct vs period
} freq_reading_t;

esp_err_t freq_counter_init(const freq_counter_cfg_t *cfg);

// Blocks for one full gate interval and writes the result.
esp_err_t freq_counter_measure(freq_reading_t *out);

// Apply a calibration offset in parts-per-million. A +10 ppm offset means
// the reference clock is faster than nominal by 10 ppm, and the measured
// frequency should be reduced by the same fraction. Stored in NVS by the
// caller; this function only configures the in-RAM state.
esp_err_t freq_counter_set_offset_ppm(int32_t ppm);

// Gate runtime control (1 - 60 s).
esp_err_t freq_counter_set_gate(int seconds);
int       freq_counter_get_gate(void);
