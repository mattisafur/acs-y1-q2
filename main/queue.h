#pragma once

#include <freertos/FreeRTOS.h>

extern QueueHandle_t queue_task_return_handle;
extern QueueHandle_t queue_card_reader_handle;
extern QueueHandle_t queue_buzzer_handle;
extern QueueHandle_t queue_accelerometer_handle;
extern QueueHandle_t queue_time_of_flight_handle;
extern QueueHandle_t queue_metric_handle;

typedef enum queue_task_message_t
{
    QUEUE_MESSAGE_RFID_CARD_VALID,
    QUEUE_MESSAGE_RFID_CARD_INVALID,
    QUEUE_MESSAGE_BUZZER_ALARM_START,
    QUEUE_MESSAGE_BUZZER_ALARM_STOP,
    QUEUE_MESSAGE_BUZZER_BEEP_CARD_VALID,
    QUEUE_MESSAGE_BUZZER_BEEP_CARD_INVALID,
} queue_task_message_t;

typedef struct queue_metric_message_t
{
    char *task_name;
    float value;
} queue_metric_message_t;

void queue_init(void);
