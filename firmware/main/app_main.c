#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/uart.h"

#include "freq_counter.h"

#define INPUT_GPIO   4
#define DEFAULT_GATE_SECONDS 1
#define NVS_NS    "freq"
#define NVS_KEY_OFFSET "cal_ppm"
#define NVS_KEY_GATE   "gate_s"

static const char *TAG = "freq";

static void fmt_frequency(double hz, char *out, size_t cap)
{
    if (hz >= 1.0e6)      snprintf(out, cap, "%8.4f MHz", hz / 1.0e6);
    else if (hz >= 1.0e3) snprintf(out, cap, "%8.3f kHz", hz / 1.0e3);
    else                  snprintf(out, cap, "%8.2f Hz",  hz);
}

static void save_cal_ppm(int32_t ppm)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i32(h, NVS_KEY_OFFSET, ppm);
    nvs_commit(h);
    nvs_close(h);
}

static int32_t load_cal_ppm(void)
{
    nvs_handle_t h;
    int32_t v = 0;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, NVS_KEY_OFFSET, &v);
        nvs_close(h);
    }
    return v;
}

static void save_gate(int seconds)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i32(h, NVS_KEY_GATE, seconds);
    nvs_commit(h);
    nvs_close(h);
}

static int load_gate(void)
{
    nvs_handle_t h;
    int32_t v = DEFAULT_GATE_SECONDS;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_i32(h, NVS_KEY_GATE, &v);
        nvs_close(h);
    }
    return (int)v;
}

// Drain UART bytes (non-blocking). Returns the first command-character
// observed, or 0 if none. Handles single-char commands: g=cycle gate,
// c=calibrate to 1 MHz, r=reset cal, h=help.
static char poll_command(void)
{
    char ch = 0;
    int len = uart_read_bytes(UART_NUM_0, (uint8_t *)&ch, 1, 0);
    if (len > 0) return ch;
    return 0;
}

static void print_help(void)
{
    printf("\n--- Commands (single keypress) ---\n"
           "  g : cycle gate window (1 -> 5 -> 10 -> 1 s)\n"
           "  c : calibrate -- treat current input as 1.000 MHz\n"
           "  r : reset calibration offset to 0\n"
           "  h : print this help\n"
           "----------------------------------\n");
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_LOGI(TAG, "Digital frequency meter starting (PCNT + gptimer)");

    int gate_s = load_gate();
    int32_t cal_ppm = load_cal_ppm();
    ESP_LOGI(TAG, "Loaded gate=%d s, cal_ppm=%d", gate_s, (int)cal_ppm);

    freq_counter_cfg_t cfg = {
        .input_gpio = INPUT_GPIO,
        .gate_seconds = gate_s,
        .high_limit = 30000,
    };
    ESP_ERROR_CHECK(freq_counter_init(&cfg));
    freq_counter_set_offset_ppm(cal_ppm);

    print_help();

    char buf[32];
    int gate_options[] = { 1, 5, 10 };
    int gate_idx = 0;
    for (int i = 0; i < 3; i++) {
        if (gate_options[i] == gate_s) { gate_idx = i; break; }
    }

    while (1) {
        // Handle keypress before each measurement to keep UX snappy.
        char cmd = poll_command();
        if (cmd == 'g' || cmd == 'G') {
            gate_idx = (gate_idx + 1) % 3;
            freq_counter_set_gate(gate_options[gate_idx]);
            save_gate(gate_options[gate_idx]);
            printf("\n[gate=%d s]\n", gate_options[gate_idx]);
        } else if (cmd == 'c' || cmd == 'C') {
            // Take a measurement, treat it as 1 MHz, compute ppm offset.
            freq_reading_t r;
            if (freq_counter_measure(&r) == ESP_OK && r.frequency_hz > 0) {
                double err_hz = r.frequency_hz - 1.0e6;
                int32_t ppm = (int32_t)(err_hz);  // 1 ppm of 1 MHz = 1 Hz
                save_cal_ppm(ppm);
                freq_counter_set_offset_ppm(ppm);
                printf("[cal] %.3f Hz @ 1 MHz target -> offset %d ppm saved\n",
                       err_hz, (int)ppm);
            }
            continue;
        } else if (cmd == 'r' || cmd == 'R') {
            save_cal_ppm(0);
            freq_counter_set_offset_ppm(0);
            printf("[cal reset]\n");
        } else if (cmd == 'h' || cmd == 'H') {
            print_help();
        }

        freq_reading_t r;
        if (freq_counter_measure(&r) == ESP_OK) {
            fmt_frequency(r.frequency_hz, buf, sizeof(buf));
            // DATA-tagged line is parsed by host_tools/serial_display.py
            printf("DATA %s mode=%s count=%u gate=%ds elapsed=%.4fs\n",
                   buf,
                   r.mode == FREQ_MODE_DIRECT ? "F" : "P",
                   (unsigned)r.count,
                   freq_counter_get_gate(),
                   r.elapsed_s);
        }
    }
}
