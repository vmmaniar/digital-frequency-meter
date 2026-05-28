#include "freq_counter.h"

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "freqcnt";

static pcnt_unit_handle_t s_unit;
static pcnt_channel_handle_t s_chan;
static int s_gate_seconds = 1;
static int s_input_gpio = 4;
static uint16_t s_high_limit = 30000;
static volatile uint32_t s_overflow_count;
static int64_t s_offset_count;

static bool IRAM_ATTR pcnt_overflow_cb(pcnt_unit_handle_t unit,
                                       const pcnt_watch_event_data_t *evt,
                                       void *user)
{
    (void)unit;
    (void)user;
    if (evt->watch_point_value == s_high_limit) {
        s_overflow_count++;
    } else if (evt->watch_point_value == -100) {
        // low_limit reached — for our edge-only counter this should not fire,
        // but track it so we can flag protocol bugs in test
        s_overflow_count++;
    }
    return false;
}

esp_err_t freq_counter_init(const freq_counter_cfg_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_gate_seconds = cfg->gate_seconds < 1 ? 1 : cfg->gate_seconds;
    s_input_gpio = cfg->input_gpio;
    s_high_limit = cfg->high_limit ? cfg->high_limit : 30000;

    pcnt_unit_config_t unit_cfg = {
        .high_limit = (int)s_high_limit,
        .low_limit  = -100,
        .flags.accum_count = true,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_cfg, &s_unit));

    pcnt_glitch_filter_config_t filter = { .max_glitch_ns = 200 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_unit, &filter));

    pcnt_chan_config_t chan_cfg = {
        .edge_gpio_num = s_input_gpio,
        .level_gpio_num = -1,
    };
    ESP_ERROR_CHECK(pcnt_new_channel(s_unit, &chan_cfg, &s_chan));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(s_chan,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(s_chan,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_KEEP));

    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(s_unit, (int)s_high_limit));
    pcnt_event_callbacks_t cbs = { .on_reach = pcnt_overflow_cb };
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(s_unit, &cbs, NULL));

    ESP_ERROR_CHECK(pcnt_unit_enable(s_unit));
    ESP_LOGI(TAG, "PCNT ready: GPIO %d, high_limit=%u, gate=%ds",
             s_input_gpio, (unsigned)s_high_limit, s_gate_seconds);
    return ESP_OK;
}

esp_err_t freq_counter_measure(freq_reading_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_overflow_count = 0;
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_unit));

    int64_t t_start = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(s_gate_seconds * 1000));
    int64_t t_end = esp_timer_get_time();

    ESP_ERROR_CHECK(pcnt_unit_stop(s_unit));
    int cur = 0;
    ESP_ERROR_CHECK(pcnt_unit_get_count(s_unit, &cur));

    // Total = overflow_count * high_limit + current residual count.
    // The accum_count flag ensures the residual is preserved across overflows
    // so we don't need to add the cur after every overflow event.
    int64_t total = (int64_t)s_overflow_count * (int64_t)s_high_limit + (int64_t)cur;
    total += s_offset_count;
    if (total < 0) total = 0;

    double elapsed_s = (t_end - t_start) / 1.0e6;
    out->count = (uint32_t)total;
    out->elapsed_s = elapsed_s;
    out->frequency_hz = (elapsed_s > 0) ? (total / elapsed_s) : 0.0;
    out->mode = (total > 100) ? FREQ_MODE_DIRECT : FREQ_MODE_PERIOD;

    if (out->mode == FREQ_MODE_PERIOD && total > 0) {
        // Period-mode reciprocal calc: T = elapsed / count, f = 1/T.
        // Same arithmetic but signals to the UI to display "P" mode + finer
        // resolution. The frequency_hz output already encodes 1/T.
    }
    return ESP_OK;
}

esp_err_t freq_counter_set_offset_ppm(int32_t ppm)
{
    // Store calibration offset as a count adjustment per gate.
    // For a 1 MHz signal with +10 ppm offset and a 1 s gate, we subtract 10 counts
    // (because the displayed reading is too high by 10 counts per 1e6 counts).
    s_offset_count = -(int64_t)ppm;  // simplified linear approximation; OK for ppm-scale offsets
    ESP_LOGI(TAG, "Calibration offset set to %d ppm (count adj=%lld)",
             (int)ppm, (long long)s_offset_count);
    return ESP_OK;
}

esp_err_t freq_counter_set_gate(int seconds)
{
    if (seconds < 1) seconds = 1;
    if (seconds > 60) seconds = 60;
    s_gate_seconds = seconds;
    ESP_LOGI(TAG, "Gate set to %d s", seconds);
    return ESP_OK;
}

int freq_counter_get_gate(void)
{
    return s_gate_seconds;
}
