#include "task_orchastrator.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "accelerometer.h"
#include "buzzer.h"
#include "card_reader.h"
#include "queue.h"

static const char *TAG = "TASK ORCHASTRATOR";

TaskHandle_t task_handle;

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

static void task_orchastrator_handler(void *)
{

    task_orchastrator_init();
    buzzer_init();
    card_reader_init();

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
            }
            break;
            
        default:
            ESP_LOGE(TAG, "Received unknown message type");
            break;
        }
        
    }
}
