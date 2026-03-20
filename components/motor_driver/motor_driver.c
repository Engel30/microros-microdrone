#include "motor_driver.h"
#include "drone_config.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "motors";

// Mappatura canale LEDC → pin GPIO
static const struct {
    gpio_num_t pin;
    ledc_channel_t channel;
} motor_map[4] = {
    { PIN_MOTOR_FL, LEDC_CHANNEL_0 },  // Front-Left
    { PIN_MOTOR_RL, LEDC_CHANNEL_1 },  // Rear-Left
    { PIN_MOTOR_RR, LEDC_CHANNEL_2 },  // Rear-Right
    { PIN_MOTOR_FR, LEDC_CHANNEL_3 },  // Front-Right
};

esp_err_t motors_init(void)
{
    // Timer condiviso per tutti e 4 i canali
    ledc_timer_config_t timer_conf = {
        .speed_mode      = MOTOR_LEDC_MODE,
        .timer_num       = MOTOR_LEDC_TIMER,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .freq_hz         = MOTOR_PWM_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config fallita: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configura 4 canali, tutti a duty 0
    for (int i = 0; i < 4; i++) {
        ledc_channel_config_t ch_conf = {
            .speed_mode = MOTOR_LEDC_MODE,
            .channel    = motor_map[i].channel,
            .timer_sel  = MOTOR_LEDC_TIMER,
            .intr_type  = LEDC_INTR_DISABLE,
            .gpio_num   = motor_map[i].pin,
            .duty       = 0,
            .hpoint     = 0,
        };
        ret = ledc_channel_config(&ch_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LEDC channel %d config fallita: %s", i, esp_err_to_name(ret));
            return ret;
        }
    }

    ESP_LOGI(TAG, "Motors init OK — 4ch LEDC @ %d Hz, %d bit",
             MOTOR_PWM_FREQ_HZ, MOTOR_PWM_RESOLUTION);
    return ESP_OK;
}

esp_err_t motors_set(const motor_cmd_t *cmd)
{
    for (int i = 0; i < 4; i++) {
        // Clamp 0-100%
        float pct = cmd->motor[i];
        if (pct < 0.0f) pct = 0.0f;
        if (pct > 100.0f) pct = 100.0f;

        uint32_t duty = (uint32_t)(pct * MOTOR_PWM_MAX_DUTY / 100.0f);

        ledc_set_duty(MOTOR_LEDC_MODE, motor_map[i].channel, duty);
        ledc_update_duty(MOTOR_LEDC_MODE, motor_map[i].channel);
    }
    return ESP_OK;
}

esp_err_t motors_stop(void)
{
    for (int i = 0; i < 4; i++) {
        ledc_set_duty(MOTOR_LEDC_MODE, motor_map[i].channel, 0);
        ledc_update_duty(MOTOR_LEDC_MODE, motor_map[i].channel);
    }
    ESP_LOGI(TAG, "All motors stopped");
    return ESP_OK;
}
