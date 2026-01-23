#include "task_orchastrator.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "accelerometer.h"
#include "app_config.h"
#include "buzzer.h"
#include "card_reader.h"
#include "queue.h"
#include "time_of_flight.h"

static const char *TAG = "task orchastrator";

TaskHandle_t task_handle;

static bool system_armed = true;
static bool system_trigerred = false;

static void task_orchastrator_handler(void *)
{
    for (;;)
    {
        orchastrator_return_message_t incoming_message;
        BaseType_t ret = xQueueReceive(queue_task_return_handle, &incoming_message, portMAX_DELAY);
        ESP_LOGD(TAG, "Received message with enum code: %d from component with enum code: %d", incoming_message.message, incoming_message.component);

        message_t outgoing_message;
        switch (incoming_message.message)
        {
        case MESSAGE_SENSOR_TRIGGERED:
            ESP_LOGD(TAG, "Received sensor triggered message from component with enum number: %d", incoming_message.component);
            outgoing_message = MESSAGE_BUZZER_ALARM_START;
            ret = xQueueSendToBack(queue_buzzer_handle, &outgoing_message, portMAX_DELAY);
            if (ret != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to send message with code %d to buzzer queue with error code %d", outgoing_message, ret);
            }
            system_trigerred = true;
            break;

        case MESSAGE_CARD_READER_CARD_VALID:
            outgoing_message = MESSAGE_BUZZER_CARD_VALID;
            ret = xQueueSendToBack(queue_buzzer_handle, &outgoing_message, portMAX_DELAY);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to send message with code %d to buzzer queue with error code %d", outgoing_message, ret);
            }

            if (system_trigerred)
            {
                // stop alarm
                outgoing_message = MESSAGE_BUZZER_ALARM_STOP;
                ret = xQueueSendToBack(queue_buzzer_handle, &outgoing_message, portMAX_DELAY);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to send message with code %d to buzzer queue with error code %d", outgoing_message, ret);
                }

                // disable sensors
                outgoing_message = MESSAGE_DISABLE;
                ret = xQueueSendToBack(queue_accelerometer_handle, &outgoing_message, portMAX_DELAY);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to send message with code %d to accelerometer queue with error code %d", outgoing_message, ret);
                }
                ret = xQueueSendToBack(queue_time_of_flight_handle, &outgoing_message, portMAX_DELAY);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to send message with code %d to time of flight sensor queue with error code %d", outgoing_message, ret);
                }

                system_armed = false;
                system_trigerred = false;
            }
            else
            {
                // toggle system arm state
                system_armed = !system_armed;
                outgoing_message = system_armed ? MESSAGE_ENABLE : MESSAGE_DISABLE;
                ret = xQueueSendToBack(queue_accelerometer_handle, &outgoing_message, portMAX_DELAY);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to send message with code %d to accelerometer queue with error code %d", outgoing_message, ret);
                }
                ret = xQueueSendToBack(queue_time_of_flight_handle, &outgoing_message, portMAX_DELAY);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to send message with code %d to time of flight sensor queue with error code %d", outgoing_message, ret);
                }
            }
            break;

        case MESSAGE_CARD_READER_CARD_INVALID:
            outgoing_message = MESSAGE_BUZZER_CARD_INVALID;
            ret = xQueueSendToBack(queue_buzzer_handle, &outgoing_message, portMAX_DELAY);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to send message with code %d to buzzer queue with error code %d", outgoing_message, ret);
            }

            if (system_armed && !system_trigerred)
            {
                // start alarm
                outgoing_message = MESSAGE_BUZZER_ALARM_START;
                ret = xQueueSendToBack(queue_buzzer_handle, &outgoing_message, portMAX_DELAY);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to send message with code %d to buzzer queue with error code %d", outgoing_message, ret);
                }
            }
            break;

        default:
            ESP_LOGE(TAG, "Received unknown message type with enum number: %d", incoming_message.message);
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
    BaseType_t rtos_ret = xTaskCreate(task_orchastrator_handler, "Task Orchastrator", APP_CONFIG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);
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
