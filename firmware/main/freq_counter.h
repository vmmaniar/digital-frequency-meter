#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    int input_gpio;          // signal under test
    int gate_seconds;        // gating window length (>=1)
    uint16_t high_limit;     // PCNT internal threshold for overflow accounting
} freq_counter_cfg_t;

typedef struct {
    double frequency_hz;
    uint32_t count;
    int mode;                // 0 = direct frequency, 1 = period (low-frequency)
} freq_reading_t;

esp_err_t freq_counter_init(const freq_counter_cfg_t *cfg);

// Blocks for one full gate interval and writes the result.
esp_err_t freq_counter_measure(freq_reading_t *out);
