#include "time_of_flight.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "app_config.h"
#include "queue.h"

#define I2C_NUwM I2C_NUM_0
#define I2C_SDA GPIO_NUM_21
#define I2C_SCL GPIO_NUM_22

static const char *TAG = "TIME OF FLIGHT";

static TaskHandle_t time_of_flight_task_handle;
static QueueHandle_t time_of_flight_queue_handle;

typedef enum time_of_flight_task_queue_message_t
{
    TIME_OF_FLIGHT_TASK_QUEUE_MESSAGE_ENABLE,
    TIME_OF_FLIGHT_TASK_QUEUE_MESSAGE_DISABLE,
    TIME_OF_FLIGHT_TASK_QUEUE_MESSAGE_TRIGGERD,
} time_of_flight_task_queue_message_t;

