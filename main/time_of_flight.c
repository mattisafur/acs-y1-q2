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
            esp_err_t tof_err = vl53l1x_read(&sensor, &read, 200);
            if (tof_err = ESP_OK)
            {
                if (r.status == 0)
                {
                    if (r.distance_mm > 200)
                    {
                        const orchestrator_return_message_t tof_message = {
                            .component = COMPONENT_TIME_OF_FLIGHT,
                            .event = MESSAGE_SENSOR_TRIGGERED,
                        };
                        xQueueSendToBack(queue_task_return_handle, &tof_message, portMAX_DELAY);

                        const metric_t metric_tof_distance = {
                            .metric_type = METRIC_TYPE_TIME_OF_FLIGHT_DISTANCE,
                            .timestamp = 0,
                            .float_value = r.distance_mm,
                        };

                        xQueueSendToBack(queue_metric_handle, &metric_tof_distance, portMAX_DELAY);
                    }
                }
                else
                {
                    EPS_LOGE(TAG, "Measurement error with status: %d", r.status);
                    
                }
            }
            else 
            {
                ESP_LOGE(TAG, "Failed to read measurements: %s", esp_err_to_name(err));
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
    vTaskDelete(&task_handle);
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

// #define I2C_PORT_NUM 0
// #define I2C_SCL_GPIO 22
// #define I2C_SDA_GPIO 21
// #define I2C_SPEED_HZ 400000
// #define VL53L1X_ADDR_7BIT 0x29

// static void time_of_flight_handler(void *)
// {
//     bool tof_enabled = true;
//     for (;;)
//     {
//         message_t msg;
//         BaseType_t esp_ret = xQueueReceive(queue_time_of_flight_handle, &msg, 0);
//         if (esp_ret == pdTRUE)
//         {
//             switch (msg)
//             {
//             case MESSAGE_ENABLE:
//                 tof_enabled = true;
//                 break;
//             case MESSAGE_DISABLE:
//                 tof_enabled = false;
//                 break;
//             default:
//                 ESP_LOGE(TAG, "Received invalid message from queue, message num: %d", msg);
//                 break;
//             }
//         }
//         if (tof_enabled)
//         {
//             vl53l1x_result_t r = {0};
//             esp_err_t err = vl53l1x_read(&sensor, &r, 200);
//             if (err == ESP_OK)
//             {
//                 if (r.status == 0)
//                 {
//                     if (r.distance_mm > 200)
//                     {
//                         const orchastrator_return_message_t tof_message = {
//                             .component = COMPONENT_TIME_OF_FLIGHT,
//                             .event = MESSAGE_SENSOR_TRIGGERED,
//                         };
//                         esp_ret = xQueueSendToBack(queue_task_return_handle, &tof_message, portMAX_DELAY);
//                         if (esp_ret != pdPASS)
//                         {
//                             ESP_LOGE(TAG, "Failed to send time of flight triggered message to orchestrator queue");
//                         }
//                     }
//                 }
//                 else
//                 {
//                     ESP_LOGE(TAG, "Measurement error with status: %d", r.status);
//                 }
//             }
//             else
//             {
//                 ESP_LOGE(TAG, "Failed to read measurement: %s", esp_err_to_name(err));
//             }
//             vTaskDelay(pdMS_TO_TICKS(100));
//         }
//     }
//// }

// esp_err_t time_of_flight_init(void)
// {
//     i2c_master_bus_config_t conf = {
//         .mode = I2C_PORT_NUM,
//         .sda_io_num = I2C_SDA_GPIO,
//         .scl_io_num = I2C_SCL_GPIO,
//         .clk_source = I2C_CLK_SRC_DEFAULT,
//         .glitch_ignore_cnt = 7,
//         .flags.enable_internal_pullup = true,
//     };
////     i2c_master_bus_handle_t bus = NULL;

//     esp_err_t ret = i2c_new_master_bus(&conf, &bus);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to initialize I2C bus");
//         return ret;
//  //   }

//     vl53l1x_t sensor = {0};
//     ret = vl53l1x_init(&sensor, bus, VL53L1X_ADDR_7BIT);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to initialize VL53L1X sensor");
//         return ret;
//    // }

//     uint16_t id = 0;
//     ret = vl53l1x_get_sensor_id(&sensor, &id);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to get VL53L1X sensor ID");
//         return ret;
//     }
//     E//SP_LOGI(TAG, "VL53L1X Sensor ID: 0x%04X", id);

//     ret = vl53l1x_sensor_init(&sensor);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to initialize VL53L1X sensor settings");
//         return ret;
//     }/

//     ret = vl53l1x_config_long_100ms(&sensor);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to configure VL53L1X sensor for long range");
//         return ret;
////     }

//     ret = vl53l1x_start(&sensor);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to start VL53L1X sensor");
//         return ret;
////     }

//     ret = vl53l1x_set_macro_timing(&sensor, 16);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to set VL53L1X macro timing");
//         return ret;
//     }

//     ret = vl53l1x_set_intermeasurement_ms(&sensor, 100);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to set VL53L1X inter-measurement period");
//         return ret;
//     }

//     ret = vl53l1x_start(&sensor);
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to start VL53L1X sensor measurements");
//         return ret;
////     }

//     BaseType_t task_created = xTaskCreate(time_of_flight_handler, "Time_of_Flight_task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, task_handle);
//     if (task_created != pdPASS)
//     {
//         ESP_LOGE(TAG, "Failed to create Time of Flight task");
//         goto cleanup;
//     }
////     return ESP_OK;

// cleanup:
//     esp_err_t cleanup_ret = i2c_delete_master_bus(bus);
//     if (cleanup_ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to clean up I2C bus: %s", esp_err_to_name(cleanup_ret));
//         abort();
////     };

//     return ret;
//// }

// esp_err_t time_of_flight_deinit(void)
// {
//     vTaskDelete(task_handle);
//     task_handle = NULL;

//     esp_err_t ret = i2c_delete_master_bus(NULL); // Replace NULL with actual bus handle if stored globally
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to deinitialize I2C bus: %s", esp_err_to_name(ret));
//         return ret;
//     }

//     ret = vl53l1x_deinit(NULL); // Replace NULL with actual sensor handle if stored globally
//     if (ret != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to deinitialize VL53L1X sensor: %s", esp_err_to_name(ret));
//         return ret;
//     }

//     return ESP_OK;
//// }
