#include "queue.h"

#include "app_config.h"

#define QUEUE_SIZE_ITEMS 16

static const char *TAG = "QUEUE";

QueueHandle_t queue_task_return_handle;
QueueHandle_t queue_card_reader_handle;
QueueHandle_t queue_buzzer_handle;
QueueHandle_t queue_accelerometer_handle;
QueueHandle_t queue_metric_handle;

void queue_init(void)
{
    queue_task_return_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(orchastrator_return_message_t));
    queue_card_reader_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(message_t));
    queue_buzzer_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(message_t));
    queue_accelerometer_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(message_t));
    queue_metric_handle = xQueueCreate(QUEUE_SIZE_ITEMS, sizeof(metric_t));
}
