#include "queue.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "app_config.h"

static const char *TAG = "queue";

QueueHandle_t queue_handle_task_orchastrator;
QueueHandle_t queue_handle_card_reader;
QueueHandle_t queue_handle_buzzer;
QueueHandle_t queue_handle_accelerometer;
QueueHandle_t queue_handle_time_of_flight;
QueueHandle_t queue_handle_metrics;

esp_err_t queue_init(void)
{
    queue_handle_task_orchastrator = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(message_t));
    if (queue_handle_task_orchastrator == 0)
    {
        ESP_LOGE(TAG, "Failed to create task_return queue");
        return ESP_FAIL;
    }

    queue_handle_card_reader = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(message_t));
    if (queue_handle_card_reader == 0)
    {
        ESP_LOGE(TAG, "Failed to create card_reader queue");
        return ESP_FAIL;
    }

    queue_handle_buzzer = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(message_t));
    if (queue_handle_buzzer == 0)
    {
        ESP_LOGE(TAG, "Failed to create buzzer queue");
        return ESP_FAIL;
    }

    queue_handle_accelerometer = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(message_t));
    if (queue_handle_accelerometer == 0)
    {
        ESP_LOGE(TAG, "Failed to create accelerometer queue");
        return ESP_FAIL;
    }

    queue_handle_time_of_flight = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(message_t));
    if (queue_handle_time_of_flight == 0)
    {
        ESP_LOGE(TAG, "Failed to create time of flight queue");
        return ESP_FAIL;
    }

    queue_handle_metrics = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(metric_t));
    if (queue_handle_metrics == 0)
    {
        ESP_LOGE(TAG, "Failed to create metric queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void queue_deinit(void)
{
    vQueueDelete(queue_handle_task_orchastrator);
    queue_handle_task_orchastrator = NULL;

    vQueueDelete(queue_handle_card_reader);
    queue_handle_card_reader = NULL;

    vQueueDelete(queue_handle_buzzer);
    queue_handle_buzzer = NULL;

    vQueueDelete(queue_handle_accelerometer);
    queue_handle_accelerometer = NULL;

    vQueueDelete(queue_handle_metrics);
    queue_handle_metrics = NULL;
}

const char *queue_message_type_to_name(message_type_t type)
{
    switch (type)
    {
    case MESSAGE_TYPE_ENABLE:
        return "MESSAGE_TYPE_ENABLE";
    case MESSAGE_TYPE_DISABLE:
        return "MESSAGE_TYPE_DISABLE";
    case MESSAGE_TYPE_SENSOR_TRIGGERED:
        return "MESSAGE_TYPE_SENSOR_TRIGGERED";
    case MESSAGE_TYPE_BUZZER_ALARM_START:
        return "MESSAGE_TYPE_BUZZER_ALARM_START";
    case MESSAGE_TYPE_BUZZER_ALARM_STOP:
        return "MESSAGE_TYPE_BUZZER_ALARM_STOP";
    case MESSAGE_TYPE_BUZZER_CARD_VALID:
        return "MESSAGE_TYPE_BUZZER_CARD_VALID";
    case MESSAGE_TYPE_BUZZER_CARD_INVALID:
        return "MESSAGE_TYPE_BUZZER_CARD_INVALID";
    case MESSAGE_TYPE_CARD_READER_CARD_VALID:
        return "MESSAGE_TYPE_CARD_READER_CARD_VALID";
    case MESSAGE_TYPE_CARD_READER_CARD_INVALID:
        return "MESSAGE_TYPE_CARD_READER_CARD_INVALID";
    default:
        ESP_LOGE(TAG, "Received invalid message type, enum code %d.", type);
        return "INVALID_MESSAGE_TYPE";
    }
}

const char *queue_component_to_name(component_t component)
{
    switch (component)
    {
    case COMPONENT_TASK_ORCHASTRATOR:
        return "COMPONENT_TASK_ORCHASTRATOR";
    case COMPONENT_METRICS_PUBLISHER:
        return "COMPONENT_METRICS_PUBLISHER";
    case COMPONENT_BUZZER:
        return "COMPONENT_BUZZER";
    case COMPONENT_CARD_READER:
        return "COMPONENT_CARD_READER";
    case COMPONENT_ACCELEROMETER:
        return "COMPONENT_ACCELEROMETER";
    case COMPONENT_TIME_OF_FLIGHT:
        return "COMPONENT_TIME_OF_FLIGHT";
    default:
        ESP_LOGE(TAG, "Received invalid component, enum code %d.", component);
        return "INVALID_COMPONENT";
    }
}

const char *queue_metric_type_to_name(metric_type_t metric_type)
{
    switch (metric_type)
    {
    case METRIC_TYPE_ACCELEROMETER_ACCELERATION_X:
        return "METRIC_TYPE_ACCELEROMETER_ACCELERATION_X";
    case METRIC_TYPE_ACCELEROMETER_ACCELERATION_Y:
        return "METRIC_TYPE_ACCELEROMETER_ACCELERATION_Y";
    case METRIC_TYPE_ACCELEROMETER_ACCELERATION_Z:
        return "METRIC_TYPE_ACCELEROMETER_ACCELERATION_Z";
    case METRIC_TYPE_ACCELEROMETER_ACCELERATION_TOTAL:
        return "METRIC_TYPE_ACCELEROMETER_ACCELERATION_TOTAL";
    case METRIC_TYPE_ACCELEROMETER_ROTATION_X:
        return "METRIC_TYPE_ACCELEROMETER_ROTATION_X";
    case METRIC_TYPE_ACCELEROMETER_ROTATION_Y:
        return "METRIC_TYPE_ACCELEROMETER_ROTATION_Y";
    case METRIC_TYPE_ACCELEROMETER_ROTATION_Z:
        return "METRIC_TYPE_ACCELEROMETER_ROTATION_Z";
    case METRIC_TYPE_ACCELEROMETER_ROTATION_TOTAL:
        return "METRIC_TYPE_ACCELEROMETER_ROTATION_TOTAL";
    case METRIC_TYPE_TIME_OF_FLIGHT_DISTANCE:
        return "METRIC_TYPE_TIME_OF_FLIGHT_DISTANCE";
    case METRIC_TYPE_CARD_READER_VALID:
        return "METRIC_TYPE_CARD_READER_VALID";
    default:
        ESP_LOGE(TAG, "Received invalid metric type, enum code %d.", metric_type);
        return "INVALID_METRIC_TYPE";
    }
}
