#include "queue.h"

#include "app_config.h"

static const char *TAG = "QUEUE";

QueueHandle_t queue_task_return_handle;
QueueHandle_t queue_card_reader_handle;
QueueHandle_t queue_buzzer_handle;
QueueHandle_t queue_accelerometer_handle;
QueueHandle_t queue_time_of_flight_handle;
QueueHandle_t queue_metric_handle;

void queue_init(void)
{
    queue_task_return_handle = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(queue_task_message_t));
    queue_card_reader_handle = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(queue_task_message_t));
    queue_buzzer_handle = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(queue_task_message_t));
    queue_accelerometer_handle = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(queue_task_message_t));
    queue_time_of_flight_handle = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(queue_task_message_t));
    queue_metric_handle = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(queue_metric_message_t));
}
