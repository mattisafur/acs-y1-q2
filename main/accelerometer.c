#include "accelerometer.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/task.h>
#include <math.h>
#include <mpu6050.h>

#include "queue.h"

#define PIN_I2C_SDA GPIO_NUM_21
#define PIN_I2C_SCL GPIO_NUM_22

#define I2C_NUM I2C_NUM_0
#define I2C_ACCELEROMETER_ADDR 0x68

#define ACCELERATION_THREASHOLD 150
#define ROTATION_THREASHOLD 150

static const char *TAG = "ACCELEROMETER";

static TaskHandle_t task_handle;

static mpu6050_dev_t device;

esp_err_t accelerometer_init(void)
{
    esp_err_t ret = mpu6050_init_desc(&device, I2C_ACCELEROMETER_ADDR, I2C_NUM, PIN_I2C_SDA, PIN_I2C_SCL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed initializing device descriptor: %s", esp_err_to_name(ret));
        goto cleanup_device_descriptor;
    }

    unsigned int failed_tries = 0;
    for (;;)
    {
        ret = i2c_dev_probe(&device.i2c_dev, I2C_DEV_WRITE);
        if (ret == ESP_OK)
        {
            break;
        }

        ESP_LOGW(TAG, "Failed to find device: %s", esp_err_to_name(ret));

        failed_tries++;
        if (failed_tries > 5)
        {
            ESP_LOGE(TAG, "Failed to find device more than 5 times: %s", esp_err_to_name(ret));
            goto cleanup_device_descriptor;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ret = mpu6050_init(&device);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize device: %s", esp_err_to_name(ret));
        goto cleanup_device_descriptor;
    }

cleanup_device_descriptor:
    device = (mpu6050_dev_t){0};
    return ret;
}

static void accelerometer_task_handler(void *)
{
    bool accelerometer_enabled = true;
    for (;;)
    {
        message_t msg;
        BaseType_t ret = xQueueReceive(queue_accelerometer_handle, &msg, 0);

        if (ret == pdTRUE)
        {
            switch (msg)
            {
            case MESSAGE_ENABLE:
                accelerometer_enabled = true;
                break;

            case MESSAGE_DISABLE:
                accelerometer_enabled = false;
                break;

            default:
                ESP_LOGE(TAG, "Received invalid message from queue, message num: %d", msg);
                break;
            }
        }

        if (accelerometer_enabled)
        {
            mpu6050_acceleration_t acceleration;
            mpu6050_rotation_t rotation;

            ret = mpu6050_get_motion(&device, &acceleration, &rotation);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to get motion: %s", esp_err_to_name(ret));
                // do anything else?
            }

            float acceleration_sum = vec3_sum(acceleration.x, acceleration.y, acceleration.z);
            float rotation_sum = vec3_sum(rotation.x, rotation.y, rotation.z);

            if (acceleration_sum > ACCELERATION_THREASHOLD || rotation_sum > ROTATION_THREASHOLD)
            {
                orchastrator_return_message_t alarm_message = {
                    .component = COMPONENT_ACCELEROMETER,
                    .message = MESSAGE_SENSOR_TRIGGERED,
                };
                xQueueSendToBack(queue_task_return_handle, &alarm_message, portMAX_DELAY);
            }

            metric_t metric_acceleration_x = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ACCELERATION_X,
                .timestamp = 0,
                .value.float_value = acceleration.x,
            };
            xQueueSendToBack(queue_metric_handle, &metric_acceleration_x, portMAX_DELAY);
            metric_t metric_acceleration_y = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ACCELERATION_Y,
                .timestamp = 0,
                .value.float_value = acceleration.y,
            };
            xQueueSendToBack(queue_metric_handle, &metric_acceleration_y, portMAX_DELAY);
            metric_t metric_acceleration_z = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ACCELERATION_Z,
                .timestamp = 0,
                .value.float_value = acceleration.z,
            };
            xQueueSendToBack(queue_metric_handle, &metric_acceleration_z, portMAX_DELAY);
            metric_t metric_acceleration_total = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ACCELERATION_TOTAL,
                .timestamp = 0,
                .value.float_value = acceleration_sum,
            };
            xQueueSendToBack(queue_metric_handle, &metric_acceleration_total, portMAX_DELAY);
            metric_t metric_rotation_x = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ROTATION_X,
                .timestamp = 0,
                .value.float_value = rotation.x,
            };
            xQueueSendToBack(queue_metric_handle, &metric_rotation_x, portMAX_DELAY);
            metric_t metric_rotation_y = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ROTATION_Y,
                .timestamp = 0,
                .value.float_value = rotation.y,
            };
            xQueueSendToBack(queue_metric_handle, &metric_rotation_y, portMAX_DELAY);
            metric_t metric_rotation_z = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ROTATION_Z,
                .timestamp = 0,
                .value.float_value = rotation.z,
            };
            xQueueSendToBack(queue_metric_handle, &metric_rotation_z, portMAX_DELAY);
            metric_t metric_rotation_total = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ROTATION_TOTAL,
                .timestamp = 0,
                .value.float_value = rotation_sum,
            };
            xQueueSendToBack(queue_metric_handle, &metric_rotation_total, portMAX_DELAY);
        }
    }
}

static float vec3_sum(float x, float y, float z) { return sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2)); }
