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
static int s_input_gpio = 34;
static volatile uint32_t s_overflow_count;

static bool pcnt_overflow_cb(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *evt, void *user)
{
    (void)unit; (void)user;
    if (evt->watch_point_value > 0) {
        s_overflow_count++;
    }
    return false;
}

esp_err_t freq_counter_init(const freq_counter_cfg_t *cfg)
{
    s_gate_seconds = cfg->gate_seconds < 1 ? 1 : cfg->gate_seconds;
    s_input_gpio = cfg->input_gpio;

    pcnt_unit_config_t unit_cfg = {
        .high_limit = cfg->high_limit ? cfg->high_limit : 30000,
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

    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(s_unit, unit_cfg.high_limit));
    pcnt_event_callbacks_t cbs = { .on_reach = pcnt_overflow_cb };
    ESP_ERROR_CHECK(pcnt_unit_register_event_callbacks(s_unit, &cbs, NULL));

    ESP_ERROR_CHECK(pcnt_unit_enable(s_unit));
    ESP_LOGI(TAG, "PCNT ready on GPIO %d", s_input_gpio);
    return ESP_OK;
}

esp_err_t freq_counter_measure(freq_reading_t *out)
{
    s_overflow_count = 0;
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(s_unit));

    int64_t t_start = esp_timer_get_time();
    vTaskDelay(pdMS_TO_TICKS(s_gate_seconds * 1000));
    int64_t t_end = esp_timer_get_time();

    ESP_ERROR_CHECK(pcnt_unit_stop(s_unit));
    int cur = 0;
    ESP_ERROR_CHECK(pcnt_unit_get_count(s_unit, &cur));

    pcnt_unit_config_t cfg_dummy;
    (void)cfg_dummy;
    uint32_t high_limit = 30000;
    uint64_t total = (uint64_t)s_overflow_count * high_limit + (uint32_t)cur;

    double elapsed_s = (t_end - t_start) / 1.0e6;
    out->count = (uint32_t)total;
    out->frequency_hz = total / elapsed_s;
    out->mode = (total > 100) ? 0 : 1;
    return ESP_OK;
}
