#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

extern QueueHandle_t queue_handle_task_orchastrator;
extern QueueHandle_t queue_handle_card_reader;
extern QueueHandle_t queue_handle_buzzer;
extern QueueHandle_t queue_handle_accelerometer;
extern QueueHandle_t queue_handle_time_of_flight;
extern QueueHandle_t queue_handle_metrics;

typedef enum
{
    MESSAGE_TYPE_ENABLE,
    MESSAGE_TYPE_DISABLE,
    MESSAGE_TYPE_SENSOR_TRIGGERED,
    MESSAGE_TYPE_BUZZER_ALARM_START,
    MESSAGE_TYPE_BUZZER_ALARM_STOP,
    MESSAGE_TYPE_BUZZER_CARD_VALID,
    MESSAGE_TYPE_BUZZER_CARD_INVALID,
    MESSAGE_TYPE_CARD_READER_CARD_VALID,
    MESSAGE_TYPE_CARD_READER_CARD_INVALID,
} message_type_t;

typedef enum
{
    COMPONENT_TASK_ORCHASTRATOR,
    COMPONENT_METRICS_PUBLISHER,
    COMPONENT_BUZZER,
    COMPONENT_CARD_READER,
    COMPONENT_ACCELEROMETER,
    COMPONENT_TIME_OF_FLIGHT,
} component_t;

typedef struct
{
    component_t component;
    message_type_t type;
} message_t;

typedef enum
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

typedef struct
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

const char *queue_message_type_to_name(message_type_t message_type);
const char *queue_component_to_name(component_t component);
const char *queue_metric_type_to_name(metric_type_t metric_type);
