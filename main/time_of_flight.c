#include "time_of_flight.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_err.h>
#include <esp_log.h>
#include <vl53l1x.h>

#include "i2c_master_bus.h"
#include "queue.h"

static const char *TAG = "TIME OF FLIGHT";

#define TIME_OF_FLIGHT_I2C_ADDR 0x29
#define TIME_OF_FLIGHT_MACRO_TIMING 16
#define TIME_OF_FLIGHT_INTERMEASUREMENT_MS 100

static TaskHandle_t task_handle;

static vl53l1x_t device_descriptor;

static void time_of_flight_handler(void *)
{
    bool tof_enabled = true;
    for (;;)
    {
        message_t msg;
        BaseType_t esp_ret = xQueueReceive(queue_time_of_flight_handle, &msg, 0);
        if (esp_ret == pdTRUE)
        {
            switch (msg)
            {
            case MESSAGE_ENABLE:
                tof_enabled = true;
                break;
            case MESSAGE_DISABLE:
                tof_enabled = false;
                break;

            default:
                ESP_LOGE(TAG, "Received invalid message from queue, message num: %d", msg);
                break;
            }
        }
        if (tof_enabled)
        {
            vl53l1x_result_t read = {0};
            esp_err_t tof_err = vl53l1x_read(&device_descriptor, &read, 200);
            if (tof_err == ESP_OK)
            {
                if (read.status == 0)
                {
                    if (read.distance_mm > 200)
                    {
                        const orchastrator_return_message_t tof_message = {
                            .component = COMPONENT_TIME_OF_FLIGHT,
                            .message = MESSAGE_SENSOR_TRIGGERED,
                        };
                        xQueueSendToBack(queue_task_return_handle, &tof_message, portMAX_DELAY);

                        const metric_t metric_tof_distance = {
                            .metric_type = METRIC_TYPE_TIME_OF_FLIGHT_DISTANCE,
                            .timestamp = 0,
                            .float_value = read.distance_mm,
                        };

                        xQueueSendToBack(queue_metrics_handle, &metric_tof_distance, portMAX_DELAY);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "Measurement error with status: %d", read.status);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Failed to read measurements: %s", esp_err_to_name(tof_err));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t time_of_flight_init(void)
{

    esp_err_t esp_ret = vl53l1x_init(&device_descriptor, i2c_master_bus_handle, TIME_OF_FLIGHT_I2C_ADDR);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize device descriptor: %s", esp_err_to_name(esp_ret));
        goto cleanup_none;
    }

    esp_ret = vl53l1x_sensor_init(&device_descriptor);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize sensor: %s", esp_err_to_name(esp_ret));
        goto cleanup_device_descriptor;
    }

    esp_ret = vl53l1x_config_long_100ms(&device_descriptor);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure sendor for long range: %s", esp_err_to_name(esp_ret));
        goto cleanup_device_descriptor;
    }

    esp_ret = vl53l1x_start(&device_descriptor);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start sensor: %s", esp_err_to_name(esp_ret));
        goto cleanup_device_descriptor;
    }

    esp_ret = vl53l1x_set_macro_timing(&device_descriptor, TIME_OF_FLIGHT_MACRO_TIMING);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set macro timing: %s", esp_err_to_name(esp_ret));
        goto cleanup_start;
    }

    esp_ret = vl53l1x_set_intermeasurement_ms(&device_descriptor, TIME_OF_FLIGHT_INTERMEASUREMENT_MS);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set intermeasurement period: %s", esp_err_to_name(esp_ret));
        goto cleanup_start;
    }

    esp_ret = vl53l1x_start(&device_descriptor);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start sensor: %s", esp_err_to_name(esp_ret));
        goto cleanup_start;
    }

    BaseType_t rtos_ret = xTaskCreate(time_of_flight_handler, "Time of Flight", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task with error code: %d", rtos_ret);
        esp_ret = ESP_OK;
        goto cleanup_start;
    }

    return ESP_OK;

cleanup_start:
    esp_err_t cleanup_ret = vl53l1x_stop(&device_descriptor);
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop sensor: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_device_descriptor:
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
    vTaskDelete(task_handle);
    task_handle = NULL;

    esp_err_t ret = vl53l1x_stop(&device_descriptor);
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
