#include "task_orchastrator.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "accelerometer.h"
#include "buzzer.h"
#include "card_reader.h"

static const char *TAG = "TASK ORCHASTRATOR";

TaskHandle_t task_handle;

static void task_orchastrator_handler(void *) {}

esp_err_t task_orchastrator_init(void)
{
    esp_err_t esp_ret = accelerometer_init();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize accelerometer: %s", esp_err_to_name(esp_ret));
        goto cleanup_nothing;
    }

    esp_ret = buzzer_init();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize buzzer: %s", esp_err_to_name(esp_ret));
        goto cleanup_accelerometer;
    }

    esp_ret = card_reader_init();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize card_reader: %s", esp_err_to_name(esp_ret));
        goto cleanup_buzzer;
    }

    BaseType_t rtos_ret = xTaskCreate(task_orchastrator_handler, "Task Orchastrator", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_card_reader;
    }

    return ESP_OK;

cleanup_card_reader:
    esp_err_t cleanup_esp_ret = card_reader_deinit();
    if (cleanup_esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize card_reader: %s. Aborting program.", esp_err_to_name(cleanup_esp_ret));
        abort();
    }
cleanup_buzzer:
    cleanup_esp_ret = buzzer_deinit();
    if (cleanup_esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize buzzer: %s. Aborting program.", esp_err_to_name(cleanup_esp_ret));
        abort();
    }
cleanup_accelerometer:
    cleanup_esp_ret = accelerometer_deinit();
    if (cleanup_esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize accelerometer: %s. Aborting program.", esp_err_to_name(cleanup_esp_ret));
        abort();
    }
cleanup_nothing:
    return esp_ret;
}
