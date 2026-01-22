#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <time.h>

extern QueueHandle_t queue_task_return_handle;
extern QueueHandle_t queue_card_reader_handle;
extern QueueHandle_t queue_buzzer_handle;
extern QueueHandle_t queue_accelerometer_handle;
extern QueueHandle_t queue_time_of_flight_handle;
extern QueueHandle_t queue_metrics_handle;

typedef enum message_t
{
    MESSAGE_ENABLE,
    MESSAGE_DISABLE,
    MESSAGE_SENSOR_TRIGGERED,
    MESSAGE_BUZZER_ALARM_START,
    MESSAGE_BUZZER_ALARM_STOP,
    MESSAGE_BUZZER_CARD_VALID,
    MESSAGE_BUZZER_CARD_INVALID,
    MESSAGE_CARD_READER_CARD_VALID,
    MESSAGE_CARD_READER_CARD_INVALID,
} message_t;

typedef enum component_t
{
    COMPONENT_BUZZER,
    COMPONENT_CARD_READER,
    COMPONENT_ACCELEROMETER,
    COMPONENT_TIME_OF_FLIGHT,
} component_t;

typedef struct orchastrator_return_message_t
{
    component_t component;
    message_t message;
} orchastrator_return_message_t;

typedef enum metric_type_t
{
    METRIC_TYPE_ACCELEROMETER_ACCELERATION_X,
    METRIC_TYPE_ACCELEROMETER_ACCELERATION_Y,
    METRIC_TYPE_ACCELEROMETER_ACCELERATION_Z,
    METRIC_TYPE_ACCELEROMETER_ACCELERATION_TOTAL,
    METRIC_TYPE_ACCELEROMETER_ROTATION_X,
    METRIC_TYPE_ACCELEROMETER_ROTATION_Y,
    METRIC_TYPE_ACCELEROMETER_ROTATION_Z,
    METRIC_TYPE_ACCELEROMETER_ROTATION_TOTAL,
    METRIC_TYPE_TIME_OF_FLIGHT_DISTANCE,
    METRIC_TYPE_CARD_READER_VALID,
} metric_type_t;

typedef struct metric_t
{
    metric_type_t metric_type;
    time_t timestamp;
    union
    {
        float float_value;
        bool bool_value;
        uint16_t uint16_value;
    };
} metric_t;

esp_err_t queue_init(void);
void queue_deinit(void);
