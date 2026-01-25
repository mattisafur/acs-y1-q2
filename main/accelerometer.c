#include "accelerometer.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <i2cdev.h>
#include <math.h>
#include <mpu6050.h>

#include "app_config.h"
#include "queue.h"

static const char *TAG = "accelerometer";

#define ACCELEROMETER_I2C_PORT_NUM I2C_NUM_1
#define ACCELEROMETER_I2C_GPIO_SDA GPIO_NUM_32
#define ACCELEROMETER_I2C_GPIO_SCL GPIO_NUM_33
#define ACCELEROMETER_I2C_ADDR 0x68
#define ACCELERATION_THREASHOLD_ACCELERATION 80
#define ACCELERATION_THREASHOLD_ROTATION 80

static TaskHandle_t task_handle;

static mpu6050_dev_t device_descriptor;

/**
 * @brief Calculates the Euclidean norm of a 3D vector.
 *
 * @param x X component.
 * @param y Y component.
 * @param z Z component.
 *
 * @return The norm.
 */
static float vec3_sum(float x, float y, float z) { return sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2)); }

/**
 * @brief Task handler for accelerometer monitoring.
 * Continuously reads sensor data and sends alerts if thresholds are exceeded.
 *
 * @param pvParameters Unused.
 */
static void accelerometer_task_handler(void *)
{
    bool enabled = true;
    for (;;)
    {
        message_t message;
        BaseType_t ret = xQueueReceive(queue_handle_accelerometer, &message, 0);

        if (ret == pdTRUE)
        {
            switch (message.type)
            {
            case MESSAGE_TYPE_ENABLE:
                enabled = true;
                break;

            case MESSAGE_TYPE_DISABLE:
                enabled = false;
                break;

            default:
                ESP_LOGE(TAG, "Received invalid message from queue, message num: %d", message);
                break;
            }
        }

        if (enabled)
        {
            mpu6050_acceleration_t acceleration;
            mpu6050_rotation_t rotation;

            ret = mpu6050_get_motion(&device_descriptor, &acceleration, &rotation);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to get motion: %s", esp_err_to_name(ret));
                // do anything else?
            }

            const float acceleration_sum = vec3_sum(acceleration.x, acceleration.y, acceleration.z);
            const float rotation_sum = vec3_sum(rotation.x, rotation.y, rotation.z);

            ESP_LOGD(TAG, "Acceleration sum: %.2f", acceleration_sum);
            ESP_LOGD(TAG, "Rotation sum: %.2f", rotation_sum);

            if (acceleration_sum > ACCELERATION_THREASHOLD_ACCELERATION || rotation_sum > ACCELERATION_THREASHOLD_ROTATION)
            {
                const message_t alarm_message = {
                    .component = COMPONENT_ACCELEROMETER,
                    .type = MESSAGE_TYPE_SENSOR_TRIGGERED,
                };
                xQueueSendToBack(queue_handle_task_orchastrator, &alarm_message, portMAX_DELAY);
            }

            const metric_t metric_acceleration_x = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ACCELERATION_X,
                .timestamp = time(NULL),
                .float_value = acceleration.x,
            };
            xQueueSendToBack(queue_handle_metrics, &metric_acceleration_x, 0);
            const metric_t metric_acceleration_y = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ACCELERATION_Y,
                .timestamp = time(NULL),
                .float_value = acceleration.y,
            };
            xQueueSendToBack(queue_handle_metrics, &metric_acceleration_y, 0);
            const metric_t metric_acceleration_z = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ACCELERATION_Z,
                .timestamp = time(NULL),
                .float_value = acceleration.z,
            };
            xQueueSendToBack(queue_handle_metrics, &metric_acceleration_z, 0);
            const metric_t metric_acceleration_total = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ACCELERATION_TOTAL,
                .timestamp = time(NULL),
                .float_value = acceleration_sum,
            };
            xQueueSendToBack(queue_handle_metrics, &metric_acceleration_total, 0);
            const metric_t metric_rotation_x = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ROTATION_X,
                .timestamp = time(NULL),
                .float_value = rotation.x,
            };
            xQueueSendToBack(queue_handle_metrics, &metric_rotation_x, 0);
            const metric_t metric_rotation_y = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ROTATION_Y,
                .timestamp = time(NULL),
                .float_value = rotation.y,
            };
            xQueueSendToBack(queue_handle_metrics, &metric_rotation_y, 0);
            const metric_t metric_rotation_z = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ROTATION_Z,
                .timestamp = time(NULL),
                .float_value = rotation.z,
            };
            xQueueSendToBack(queue_handle_metrics, &metric_rotation_z, 0);
            const metric_t metric_rotation_total = {
                .metric_type = METRIC_TYPE_ACCELEROMETER_ROTATION_TOTAL,
                .timestamp = time(NULL),
                .float_value = rotation_sum,
            };
            xQueueSendToBack(queue_handle_metrics, &metric_rotation_total, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t accelerometer_init(void)
{
    esp_err_t esp_ret;
    BaseType_t rtos_ret;
    esp_err_t cleanup_ret;

    ESP_LOGD(TAG, "Intializing i2cdev...");
    esp_ret = i2cdev_init();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize i2cdev: %s", esp_err_to_name(esp_ret));
        goto cleanup_nothing;
    }

    ESP_LOGD(TAG, "Initializing device descriptor...");
    esp_ret = mpu6050_init_desc(&device_descriptor, ACCELEROMETER_I2C_ADDR, ACCELEROMETER_I2C_PORT_NUM, ACCELEROMETER_I2C_GPIO_SDA, ACCELEROMETER_I2C_GPIO_SCL);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed initializing device descriptor: %s", esp_err_to_name(esp_ret));
        goto cleanup_nothing;
    }

    unsigned int failed_tries = 0;
    for (;;)
    {
        ESP_LOGD(TAG, "Probing for device...");
        esp_ret = i2c_dev_probe(&device_descriptor.i2c_dev, I2C_DEV_WRITE);
        if (esp_ret == ESP_OK)
        {
            ESP_LOGD(TAG, "Device probed successfully");
            break;
        }

        ESP_LOGW(TAG, "Failed to find device: %s", esp_err_to_name(esp_ret));

        failed_tries++;
        if (failed_tries > 5)
        {
            ESP_LOGE(TAG, "Failed to find device more than 5 times: %s", esp_err_to_name(esp_ret));
            goto cleanup_device_descriptor;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGD(TAG, "Initializing initializing device...");
    esp_ret = mpu6050_init(&device_descriptor);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize device: %s", esp_err_to_name(esp_ret));
        goto cleanup_device_descriptor;
    }

    ESP_LOGD(TAG, "Initializing accelerometer freertos task...");
    rtos_ret = xTaskCreate(accelerometer_task_handler, "Accelerometer", APP_CONFIG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task with error code %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_device_descriptor;
    }

    return ESP_OK;

cleanup_device_descriptor:
    ESP_LOGI(TAG, "Freeing device descriptor...");
    cleanup_ret = mpu6050_free_desc(&device_descriptor);
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to free device descriptor: %s", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_nothing:
    return esp_ret;
}

esp_err_t accelerometer_deinit(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Deleting task...");
    vTaskDelete(task_handle);
    task_handle = NULL;

    ESP_LOGI(TAG, "Freeing device descriptor...");
    ret = mpu6050_free_desc(&device_descriptor);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to free device descriptor: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
