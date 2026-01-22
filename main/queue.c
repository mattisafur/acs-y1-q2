#include "queue.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#define QUEUE_SIZE_ITEMS 16

static const char *TAG = "QUEUE";

QueueHandle_t queue_task_return_handle;
QueueHandle_t queue_card_reader_handle;
QueueHandle_t queue_buzzer_handle;
QueueHandle_t queue_accelerometer_handle;
QueueHandle_t queue_time_of_flight_handle;
QueueHandle_t queue_metrics_handle;

esp_err_t queue_init(void)
{
    queue_task_return_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(orchastrator_return_message_t));
    if (queue_task_return_handle == 0)
    {
        ESP_LOGE(TAG, "Failed to create task_return queue");
        return ESP_FAIL;
    }

    queue_card_reader_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(message_t));
    if (queue_card_reader_handle == 0)
    {
        ESP_LOGE(TAG, "Failed to create card_reader queue");
        return ESP_FAIL;
    }

    queue_buzzer_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(message_t));
    if (queue_buzzer_handle == 0)
    {
        ESP_LOGE(TAG, "Failed to create buzzer queue");
        return ESP_FAIL;
    }

    queue_accelerometer_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(message_t));
    if (queue_accelerometer_handle == 0)
    {
        ESP_LOGE(TAG, "Failed to create accelerometer queue");
        return ESP_FAIL;
    }

    queue_time_of_flight_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(message_t));
    if (queue_time_of_flight_handle == 0)
    {
        ESP_LOGE(TAG, "Failed to create time of flight queue");
        return ESP_FAIL;
    }

    queue_metrics_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(metric_t));
    if (queue_metrics_handle == 0)
    {
        ESP_LOGE(TAG, "Failed to create metric queue");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void queue_deinit(void)
{
    vQueueDelete(queue_task_return_handle);
    queue_task_return_handle = NULL;

    vQueueDelete(queue_card_reader_handle);
    queue_card_reader_handle = NULL;

    vQueueDelete(queue_buzzer_handle);
    queue_buzzer_handle = NULL;

    vQueueDelete(queue_accelerometer_handle);
    queue_accelerometer_handle = NULL;

    vQueueDelete(queue_metrics_handle);
    queue_metrics_handle = NULL;
}
