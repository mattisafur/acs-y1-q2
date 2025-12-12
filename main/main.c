#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>

static const char *TAG = "learn/buzzer";

#define GPIO_BUZZER GPIO_NUM_25

void app_main(void)
{
    ESP_LOGI(TAG, "initializing gpio");
    gpio_config_t buzzer_gpio_config = {
        .pin_bit_mask = 1UL << GPIO_BUZZER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&buzzer_gpio_config));
    ESP_LOGI(TAG, "initialized gpio");

    for (;;)
    {
        ESP_ERROR_CHECK(gpio_set_level(GPIO_BUZZER, 0));
        ESP_LOGI(TAG, "set gpio low");
        vTaskDelay(pdMS_TO_TICKS(250));

        ESP_ERROR_CHECK(gpio_set_level(GPIO_BUZZER, 1));
        ESP_LOGI(TAG, "set gpio high");
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
