#include "task_orchastrator.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "accelerometer.h"
#include "buzzer.h"
#include "card_reader.h"
#include "queue.h"
#include "time_of_flight.h"

static const char *TAG = "task orchastrator";

TaskHandle_t task_handle;

static void task_orchastrator_handler(void *)
{
    for (;;)
    {
        message_t msg;
        BaseType_t ret = xQueueReceive(queue_task_return_handle, &msg, portMAX_DELAY);

        switch (msg)
        {
        case MESSAGE_SENSOR_TRIGGERED:
            ESP_LOGD(TAG, "Received sensor triggered message");
            msg = MESSAGE_BUZZER_ALARM_START;
            ret = xQueueSendToBack(queue_buzzer_handle, &msg, portMAX_DELAY);
            if (ret != pdPASS)
            {
                ESP_LOGE(TAG, "Failed to send message with code %d to buzzer queue with error code %d", msg, ret);
            }
            break;

        case MESSAGE_CARD_READER_CARD_VALID:
            ESP_LOGD(TAG, "Received card reader valid message");
            msg = MESSAGE_BUZZER_ALARM_STOP;
            ret = xQueueSendToBack(queue_buzzer_handle, &msg, portMAX_DELAY);
            if (ret != pdPASS)
            {
                ESP_LOGE(TAG, "Failed to send message with code %d to buzzer queue with error code %d", msg, ret);
            }
            break;

        case MESSAGE_CARD_READER_CARD_INVALID:
            ESP_LOGD(TAG, "Received card reader invalid message");
            msg = MESSAGE_BUZZER_ALARM_START;
            ret = xQueueSendToBack(queue_buzzer_handle, &msg, portMAX_DELAY);
            if (ret != pdPASS)
            {
                ESP_LOGE(TAG, "Failed to send message with code %d to buzzer queue with error code %d", msg, ret);
            }
            break;

        default:
            ESP_LOGE(TAG, "Received unknown message type");
            break;
        }
    }
}

esp_err_t task_orchastrator_init(void)
{
    ESP_LOGD(TAG, "Initializing accelerometer...");
    esp_err_t esp_ret = accelerometer_init();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize accelerometer: %s", esp_err_to_name(esp_ret));
        goto cleanup_nothing;
    }

    ESP_LOGD(TAG, "Initializing buzzer...");
    esp_ret = buzzer_init();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize buzzer: %s", esp_err_to_name(esp_ret));
        goto cleanup_accelerometer;
    }

    ESP_LOGD(TAG, "Initializing card reader");
    esp_ret = card_reader_init();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize card reader: %s", esp_err_to_name(esp_ret));
        goto cleanup_buzzer;
    }

    ESP_LOGD(TAG, "Initializing time of flight sensor...");
    esp_ret = time_of_flight_init();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize time of flight sensor: %s", esp_err_to_name(esp_ret));
        goto cleanup_card_reader;
    }

    ESP_LOGD(TAG, "creating task orchastrator freertos task...");
    BaseType_t rtos_ret = xTaskCreate(task_orchastrator_handler, "Task Orchastrator", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_time_of_flight;
    }

    return ESP_OK;

cleanup_time_of_flight:
    esp_err_t cleanup_esp_ret = time_of_flight_deinit();
    if (cleanup_esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize time_of_flight: %s. Aborting program.", esp_err_to_name(cleanup_esp_ret));
        abort();
    }
cleanup_card_reader:
    cleanup_esp_ret = card_reader_deinit();
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
