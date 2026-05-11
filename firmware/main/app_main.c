#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "freq_counter.h"

#define INPUT_GPIO   34
#define GATE_SECONDS 1

static const char *TAG = "freq";

static void fmt_frequency(double hz, char *out, size_t cap)
{
    if (hz >= 1.0e6)      snprintf(out, cap, "%8.4f MHz", hz / 1.0e6);
    else if (hz >= 1.0e3) snprintf(out, cap, "%8.3f kHz", hz / 1.0e3);
    else                  snprintf(out, cap, "%8.2f Hz",  hz);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Digital frequency meter starting (PCNT + gptimer)");

    freq_counter_cfg_t cfg = {
        .input_gpio = INPUT_GPIO,
        .gate_seconds = GATE_SECONDS,
        .high_limit = 30000,
    };
    ESP_ERROR_CHECK(freq_counter_init(&cfg));

    char buf[32];
    while (1) {
        freq_reading_t r;
        if (freq_counter_measure(&r) == ESP_OK) {
            fmt_frequency(r.frequency_hz, buf, sizeof(buf));
            printf("[%s mode] count=%u  f=%s\n",
                   r.mode == 0 ? "freq" : "period",
                   (unsigned)r.count, buf);
        }
    }
}
