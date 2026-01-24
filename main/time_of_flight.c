#include "time_of_flight.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <vl53l1x.h>

#include "app_config.h"
#include "queue.h"

static const char *TAG = "time of flight";

#define TIME_OF_FLIGHT_I2C_PORT_NUM I2C_NUM_0
#define TIME_OF_FLIGHT_I2C_GPIO_SDA GPIO_NUM_21
#define TIME_OF_FLIGHT_I2C_GPIO_SCL GPIO_NUM_22
#define TIME_OF_FLIGHT_I2C_ADDR 0x29
#define TIME_OF_FLIGHT_MACRO_TIMING 16
#define TIME_OF_FLIGHT_INTERMEASUREMENT_MS 100
#define TIME_OF_FLIGHT_DISTANCE_THREASHOLD_MM 200

static TaskHandle_t task_handle;

i2c_master_bus_handle_t i2c_master_bus_handle;
static vl53l1x_t device_descriptor;

/**
 * @brief Task handler for time of flight sensor.
 * Monitors distance and sends alerts if threshold exceeded.
 *
 * @param pvParameters Unused.
 */
static void time_of_flight_handler(void *)
{
    bool tof_enabled = true;
    for (;;)
    {
        message_t message;
        BaseType_t esp_ret = xQueueReceive(queue_handle_time_of_flight, &message, 0);
        if (esp_ret == pdTRUE)
        {
            switch (message.type)
            {
            case MESSAGE_TYPE_ENABLE:
                tof_enabled = true;
                break;
            case MESSAGE_TYPE_DISABLE:
                tof_enabled = false;
                break;

            default:
                ESP_LOGE(TAG, "Received invalid message from queue, message num: %d", message);
                break;
            }
        }
        if (tof_enabled)
        {
            vl53l1x_result_t read = {0};
            esp_err_t tof_err = vl53l1x_read(&device_descriptor, &read, 200);
            if (tof_err == ESP_OK)
            {
                if (read.status != 0)
                {
                    ESP_LOGE(TAG, "Failed to read measurements: %s", esp_err_to_name(tof_err));
                    continue;
                }

                if (read.distance_mm > TIME_OF_FLIGHT_DISTANCE_THREASHOLD_MM)
                {
                    const message_t tof_message = {
                        .component = COMPONENT_TIME_OF_FLIGHT,
                        .type = MESSAGE_TYPE_SENSOR_TRIGGERED,
                    };
                    xQueueSendToBack(queue_handle_task_orchastrator, &tof_message, portMAX_DELAY);

                    const metric_t metric_tof_distance = {
                        .metric_type = METRIC_TYPE_TIME_OF_FLIGHT_DISTANCE,
                        .timestamp = time(NULL),
                        .float_value = read.distance_mm,
                    };

                    xQueueSendToBack(queue_handle_metrics, &metric_tof_distance, 0);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t time_of_flight_init(void)
{
    esp_err_t esp_ret;
    BaseType_t rtos_ret;
    esp_err_t cleanup_ret;

    ESP_LOGD(TAG, "Creating new I2C master bus...");
    i2c_master_bus_config_t i2c_master_bus_config = {
        .i2c_port = TIME_OF_FLIGHT_I2C_PORT_NUM,
        .sda_io_num = TIME_OF_FLIGHT_I2C_GPIO_SDA,
        .scl_io_num = TIME_OF_FLIGHT_I2C_GPIO_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_ret = i2c_new_master_bus(&i2c_master_bus_config, &i2c_master_bus_handle);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create new I2C master bus: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    ESP_LOGD(TAG, "Initializing device descriptor...");
    esp_ret = vl53l1x_init(&device_descriptor, i2c_master_bus_handle, TIME_OF_FLIGHT_I2C_ADDR);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize device descriptor: %s", esp_err_to_name(esp_ret));
        goto cleanup_none;
    }

    ESP_LOGD(TAG, "Initializing sensor...");
    esp_ret = vl53l1x_sensor_init(&device_descriptor);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize sensor: %s", esp_err_to_name(esp_ret));
        goto cleanup_device_descriptor;
    }

    ESP_LOGD(TAG, "Configuring sensor for long range...");
    esp_ret = vl53l1x_config_long_100ms(&device_descriptor);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure sensor for long range: %s", esp_err_to_name(esp_ret));
        goto cleanup_device_descriptor;
    }

    ESP_LOGD(TAG, "Starting sensor...");
    esp_ret = vl53l1x_start(&device_descriptor);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start sensor: %s", esp_err_to_name(esp_ret));
        goto cleanup_device_descriptor;
    }

    ESP_LOGD(TAG, "Setting macro timing...");
    esp_ret = vl53l1x_set_macro_timing(&device_descriptor, TIME_OF_FLIGHT_MACRO_TIMING);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set macro timing: %s", esp_err_to_name(esp_ret));
        goto cleanup_start;
    }

    ESP_LOGD(TAG, "setting intermeasurement period...");
    esp_ret = vl53l1x_set_intermeasurement_ms(&device_descriptor, TIME_OF_FLIGHT_INTERMEASUREMENT_MS);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set intermeasurement period: %s", esp_err_to_name(esp_ret));
        goto cleanup_start;
    }

    ESP_LOGD(TAG, "starting sensor");
    esp_ret = vl53l1x_start(&device_descriptor);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start sensor: %s", esp_err_to_name(esp_ret));
        goto cleanup_start;
    }

    ESP_LOGD(TAG, "creating freertos task...");
    rtos_ret = xTaskCreate(time_of_flight_handler, "Time of Flight", APP_CONFIG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task with error code: %d", rtos_ret);
        esp_ret = ESP_OK;
        goto cleanup_start;
    }

    return ESP_OK;

cleanup_start:
    ESP_LOGD(TAG, "cleanup: stopping sensor...");
    cleanup_ret = vl53l1x_stop(&device_descriptor);
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop sensor: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_device_descriptor:
    ESP_LOGD(TAG, "cleanup: deinitializing device...");
    cleanup_ret = vl53l1x_deinit(&device_descriptor);
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize device descriptor: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_none:
    return esp_ret;
}

esp_err_t time_of_flight_deinit(void)
{
    esp_err_t ret;

    vTaskDelete(task_handle);
    task_handle = NULL;

    ret = vl53l1x_stop(&device_descriptor);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop sensor: %s.", esp_err_to_name(ret));
        return ret;
    }

    ret = vl53l1x_deinit(&device_descriptor);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize device descriptor: %s.", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
