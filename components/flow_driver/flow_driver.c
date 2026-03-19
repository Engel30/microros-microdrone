#include "flow_driver.h"
#include "drone_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"

static const char *TAG = "flow";

// Protocollo CXOF
#define CXOF_HEADER     0xFE
#define CXOF_PROTO_ID   0x04
#define CXOF_FOOTER     0xAA
#define CXOF_FRAME_SIZE 11

// State machine parser
static uint8_t buf[CXOF_FRAME_SIZE];
static uint8_t idx = 0;

// Ultimo frame valido
static flow_data_t last_frame;
static bool new_data = false;
static int64_t prev_timestamp_us = 0;

// Posizione integrata (debug)
static float integrated_x = 0.0f;
static float integrated_y = 0.0f;

static void process_frame(void)
{
    int64_t now = esp_timer_get_time();

    // Raw pixel delta (little-endian)
    int16_t raw_x = (int16_t)(buf[2] | (buf[3] << 8));
    int16_t raw_y = (int16_t)(buf[4] | (buf[5] << 8));

    // Distanza ToF in cm → mm
    uint16_t range_mm = (uint16_t)(buf[6] | (buf[7] << 8)) * 10;

    // Salva raw per debug
    last_frame.raw_x = raw_x;
    last_frame.raw_y = raw_y;
    last_frame.range_mm = range_mm;
    last_frame.quality = buf[9];

    // Converti in spostamento lineare e velocità nel body frame
    // spostamento = pixel_delta * FLOW_SCALE_RAD * altitude
    // La scala FLOW_SCALE_RAD converte count → radianti.
    // Moltiplicando per altitudine si ottiene lo spostamento lineare in metri.
    float dt = (prev_timestamp_us > 0) ? (now - prev_timestamp_us) / 1e6f : 0.05f;
    if (dt < 0.001f) dt = 0.001f;

    float altitude_m = range_mm / 1000.0f;

    // Applica correzione segno per body frame (X=avanti, Y=destra)
    float corrected_x = (float)FLOW_SIGN_X(raw_x);
    float corrected_y = (float)FLOW_SIGN_Y(raw_y);

    // Spostamento in metri per questo frame
    float dx = corrected_x * FLOW_SCALE_RAD * altitude_m;
    float dy = corrected_y * FLOW_SCALE_RAD * altitude_m;

    last_frame.vel_x = dx / dt;
    last_frame.vel_y = dy / dt;

    // Integra posizione
    integrated_x += dx;
    integrated_y += dy;
    last_frame.pos_x = integrated_x;
    last_frame.pos_y = integrated_y;

    last_frame.timestamp_us = now;
    prev_timestamp_us = now;
    new_data = true;
}

static void parse_byte(uint8_t c)
{
    if (idx == 0) {
        if (c == CXOF_HEADER) {
            buf[idx++] = c;
        }
    } else if (idx == 1) {
        if (c == CXOF_PROTO_ID) {
            buf[idx++] = c;
        } else {
            if (c == CXOF_HEADER) {
                buf[0] = c;
                idx = 1;
            } else {
                idx = 0;
            }
        }
    } else {
        buf[idx++] = c;

        if (idx == CXOF_FRAME_SIZE) {
            if (buf[10] == CXOF_FOOTER) {
                uint8_t ck = 0;
                for (int i = 2; i <= 7; i++) {
                    ck += buf[i];
                }
                if (ck == buf[8]) {
                    process_frame();
                }
            }
            idx = 0;
        }
    }
}

esp_err_t flow_init(void)
{
    ESP_LOGI(TAG, "Flow scale: %.5f rad/count (CXOF)", FLOW_SCALE_RAD);

    uart_config_t uart_conf = {
        .baud_rate = FLOW_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret;

    ret = uart_param_config(FLOW_UART_NUM, &uart_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore config UART: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(FLOW_UART_NUM, PIN_FLOW_TX, PIN_FLOW_RX,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore set pin UART: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_driver_install(FLOW_UART_NUM, 256, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Errore install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }

    idx = 0;
    new_data = false;
    prev_timestamp_us = 0;
    integrated_x = 0.0f;
    integrated_y = 0.0f;

    ESP_LOGI(TAG, "Optical flow UART inizializzata: %d baud su TX=D6, RX=D7",
             FLOW_BAUD_RATE);
    return ESP_OK;
}

esp_err_t flow_read(flow_data_t *data)
{
    uint8_t rx_buf[64];
    int len = uart_read_bytes(FLOW_UART_NUM, rx_buf, sizeof(rx_buf), 0);

    if (len > 0) {
        for (int i = 0; i < len; i++) {
            parse_byte(rx_buf[i]);
        }
    }

    if (new_data) {
        *data = last_frame;
        new_data = false;
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}
