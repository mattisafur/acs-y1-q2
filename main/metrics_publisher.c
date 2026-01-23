#include "metrics_publisher.h"
#include "wifi_manager.h"

#include <cJSON.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "queue.h"

static const char *TAG = "METRICS_PUBLISHER";
static TaskHandle_t task_handle;

static cJSON *metric_to_cjson(metric_t *metric)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "timestamp", (double)metric->timestamp);

    switch (metric->metric_type)
    {
    case METRIC_TYPE_ACCELEROMETER_ACCELERATION_X:
        cJSON_AddStringToObject(json, "metric_type", "METRIC_TYPE_ACCELEROMETER_ACCELERATION_X");
        cJSON_AddNumberToObject(json, "float_value", metric->float_value);
        break;

    case METRIC_TYPE_ACCELEROMETER_ACCELERATION_Y:
        cJSON_AddStringToObject(json, "metric_type", "METRIC_TYPE_ACCELEROMETER_ACCELERATION_Y");
        cJSON_AddNumberToObject(json, "float_value", metric->float_value);
        break;

    case METRIC_TYPE_ACCELEROMETER_ACCELERATION_Z:
        cJSON_AddStringToObject(json, "metric_type", "METRIC_TYPE_ACCELEROMETER_ACCELERATION_Z");
        cJSON_AddNumberToObject(json, "float_value", metric->float_value);
        break;

    case METRIC_TYPE_ACCELEROMETER_ACCELERATION_TOTAL:
        cJSON_AddStringToObject(json, "metric_type", "METRIC_TYPE_ACCELEROMETER_ACCELERATION_TOTAL");
        cJSON_AddNumberToObject(json, "float_value", metric->float_value);
        break;

    case METRIC_TYPE_ACCELEROMETER_ROTATION_X:
        cJSON_AddStringToObject(json, "metric_type", "METRIC_TYPE_ACCELEROMETER_ROTATION_X");
        cJSON_AddNumberToObject(json, "float_value", metric->float_value);
        break;

    case METRIC_TYPE_ACCELEROMETER_ROTATION_Y:
        cJSON_AddStringToObject(json, "metric_type", "METRIC_TYPE_ACCELEROMETER_ROTATION_Y");
        cJSON_AddNumberToObject(json, "float_value", metric->float_value);
        break;

    case METRIC_TYPE_ACCELEROMETER_ROTATION_Z:
        cJSON_AddStringToObject(json, "metric_type", "METRIC_TYPE_ACCELEROMETER_ROTATION_Z");
        cJSON_AddNumberToObject(json, "float_value", metric->float_value);
        break;

    case METRIC_TYPE_ACCELEROMETER_ROTATION_TOTAL:
        cJSON_AddStringToObject(json, "metric_type", "METRIC_TYPE_ACCELEROMETER_ROTATION_TOTAL");
        cJSON_AddNumberToObject(json, "float_value", metric->float_value);
        break;

    case METRIC_TYPE_TIME_OF_FLIGHT_DISTANCE:
        cJSON_AddStringToObject(json, "metric_type", "METRIC_TYPE_TIME_OF_FLIGHT_DISTANCE");
        cJSON_AddNumberToObject(json, "uint16_value", metric->uint16_value);
        break;

    case METRIC_TYPE_CARD_READER_VALID:
        cJSON_AddStringToObject(json, "metric_type", "METRIC_TYPE_CARD_READER_VALID");
        cJSON_AddBoolToObject(json, "bool_value", metric->bool_value);
        break;

    default:
        ESP_LOGE(TAG, "Unknown metric type enum: %d", metric->metric_type);
        break;
    }

    return json;
}

static void metrics_publisher_handler(void *)
{
    for (;;)
    {
        metric_t msg;
        xQueueReceive(queue_metrics_handle, &msg, portMAX_DELAY);

        cJSON *json = metric_to_cjson(&msg);
        // TODO: HTTP publish
        cJSON_Delete(json);
    }
}

esp_err_t metrics_publisher_init(void)
{
    // ✅ Wi-Fi is called inside metrics publisher
    esp_err_t ret = wifi_manager_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi_manager_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = wifi_manager_ensure_connected(20000);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi_manager_ensure_connected failed: %s", esp_err_to_name(ret));
        return ret;
    }

    BaseType_t rtos_ret = xTaskCreate(metrics_publisher_handler, "Metrics Publisher",
                                      configMINIMAL_STACK_SIZE, NULL,
                                      tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task: %d", rtos_ret);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t metrics_publisher_deinit(void)
{
    vTaskDelete(task_handle);
    task_handle = NULL;

    return wifi_manager_deinit();
}
