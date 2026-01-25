#include "task_orchastrator.h"

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "accelerometer.h"
#include "app_config.h"
#include "app_wifi.h"
#include "buzzer.h"
#include "card_reader.h"
#include "metrics_publisher.h"
#include "queue.h"
#include "time_of_flight.h"
#include "time_sync.h"

static const char *TAG = "task orchastrator";

TaskHandle_t task_handle;

static bool system_armed = true;
static bool system_trigerred = false;

static void task_orchastrator_handler(void *)
{
    for (;;)
    {
        message_t incoming_message;
        BaseType_t ret = xQueueReceive(queue_handle_task_orchastrator, &incoming_message, portMAX_DELAY);
        ESP_LOGD(TAG, "Received message \"%s\" from component \"%s\"", queue_message_type_to_name(incoming_message.type), queue_component_to_name(incoming_message.component));

        message_t outgoing_message = {
            .component = COMPONENT_TASK_ORCHASTRATOR,
        };
        switch (incoming_message.type)
        {
        case MESSAGE_TYPE_SENSOR_TRIGGERED:
            outgoing_message.type = MESSAGE_TYPE_BUZZER_ALARM_START;
            ret = xQueueSendToBack(queue_handle_buzzer, &outgoing_message, portMAX_DELAY);
            if (ret != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to send message \"%s\" to buzzer queue with error code %d", queue_message_type_to_name(outgoing_message.type), ret);
            }
            system_trigerred = true;
            break;

        case MESSAGE_TYPE_CARD_READER_CARD_VALID:
            outgoing_message.type = MESSAGE_TYPE_BUZZER_CARD_VALID;
            ret = xQueueSendToBack(queue_handle_buzzer, &outgoing_message, portMAX_DELAY);
            if (ret != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to send message \"%s\" to buzzer queue with error code %d", queue_message_type_to_name(outgoing_message.type), ret);
            }

            if (system_trigerred)
            {
                // stop alarm
                outgoing_message.type = MESSAGE_TYPE_BUZZER_ALARM_STOP;
                ret = xQueueSendToBack(queue_handle_buzzer, &outgoing_message, portMAX_DELAY);
                if (ret != pdTRUE)
                {
                    ESP_LOGE(TAG, "Failed to send message \"%s\" to buzzer queue with error code %d", queue_message_type_to_name(outgoing_message.type), ret);
                }

                // disable sensors
                outgoing_message.type = MESSAGE_TYPE_DISABLE;
                ret = xQueueSendToBack(queue_handle_accelerometer, &outgoing_message, portMAX_DELAY);
                if (ret != pdTRUE)
                {
                    ESP_LOGE(TAG, "Failed to send message \"%s\" to accelerometer queue with error code %d", queue_message_type_to_name(outgoing_message.type), ret);
                }
                ret = xQueueSendToBack(queue_handle_time_of_flight, &outgoing_message, portMAX_DELAY);
                if (ret != pdTRUE)
                {
                    ESP_LOGE(TAG, "Failed to send message \"%s\" to time of flight sensor queue with error code %d", queue_message_type_to_name(outgoing_message.type), ret);
                }

                system_armed = false;
                system_trigerred = false;
            }
            else
            {
                // toggle system arm state
                system_armed = !system_armed;
                outgoing_message.type = system_armed ? MESSAGE_TYPE_ENABLE : MESSAGE_TYPE_DISABLE;
                ret = xQueueSendToBack(queue_handle_accelerometer, &outgoing_message, portMAX_DELAY);
                if (ret != pdTRUE)
                {
                    ESP_LOGE(TAG, "Failed to send message \"%s\" to accelerometer queue with error code %d", queue_message_type_to_name(outgoing_message.type), ret);
                }
                ret = xQueueSendToBack(queue_handle_time_of_flight, &outgoing_message, portMAX_DELAY);
                if (ret != pdTRUE)
                {
                    ESP_LOGE(TAG, "Failed to send message \"%s\" to time of flight sensor queue with error code %d", queue_message_type_to_name(outgoing_message.type), ret);
                }
            }
            break;

        case MESSAGE_TYPE_CARD_READER_CARD_INVALID:
            outgoing_message.type = MESSAGE_TYPE_BUZZER_CARD_INVALID;
            ret = xQueueSendToBack(queue_handle_buzzer, &outgoing_message, portMAX_DELAY);
            if (ret != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to send message \"%s\" to card buzzer queue with error code %d", queue_message_type_to_name(outgoing_message.type), ret);
            }

            if (system_armed && !system_trigerred)
            {
                // start alarm
                outgoing_message.type = MESSAGE_TYPE_BUZZER_ALARM_START;
                ret = xQueueSendToBack(queue_handle_buzzer, &outgoing_message, portMAX_DELAY);
                if (ret != pdTRUE)
                {
                    ESP_LOGE(TAG, "Failed to send message \"%s\" to buzzer queue with error code %d", queue_message_type_to_name(outgoing_message.type), ret);
                }
            }
            break;

        default:
            ESP_LOGE(TAG, "Received unknown message type \"%s\"", queue_message_type_to_name(incoming_message.type));
            break;
        }
    }
}

esp_err_t task_orchastrator_init(void)
{
    esp_err_t esp_ret;
    BaseType_t rtos_ret;
    esp_err_t cleanup_ret;

    ESP_LOGD(TAG, "Initializing accelerometer...");
    esp_ret = accelerometer_init();
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

    esp_ret = metrics_publisher_init();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize metrics publisher: %s", esp_err_to_name(esp_ret));
        goto cleanup_time_of_flight;
    }

    ESP_LOGD(TAG, "creating task orchastrator freertos task...");
    rtos_ret = xTaskCreate(task_orchastrator_handler, "Task Orchastrator", APP_CONFIG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_metrics_publisher;
    }

    return ESP_OK;

cleanup_metrics_publisher:
    ESP_LOGI(TAG, "Deinitializing metrics publisher...");
    cleanup_ret = metrics_publisher_deinit();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize metrics publisher: %s .aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_time_of_flight:
    ESP_LOGI(TAG, "Deinitializing time of flight...");
    cleanup_ret = time_of_flight_deinit();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize time_of_flight: %s. Aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_card_reader:
    ESP_LOGI(TAG, "Deinitializing card reader...");
    cleanup_ret = card_reader_deinit();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize card_reader: %s. Aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_buzzer:
    ESP_LOGI(TAG, "Deinitializing buzzer...");
    cleanup_ret = buzzer_deinit();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize buzzer: %s. Aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_accelerometer:
    ESP_LOGI(TAG, "Deinitializing accelerometer...");
    cleanup_ret = accelerometer_deinit();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize accelerometer: %s. Aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_nothing:
    return esp_ret;
}
