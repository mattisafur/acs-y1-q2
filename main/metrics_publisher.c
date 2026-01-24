#include "metrics_publisher.h"

#include <cJSON.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>

#include "app_config.h"
#include "app_wifi.h"
#include "queue.h"

static const char *TAG = "metrics publisher";

#define METRICS_PUBLISH ER_ENDPOINT_URL "http://4.233.137.69/metrics"

static TaskHandle_t task_handle;

static esp_http_client_handle_t http_client_handle;

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

        cJSON *metric_json = metric_to_cjson(&msg);

        char *metric_json_string = cJSON_Print(metric_json);
        esp_http_client_set_header(http_client_handle, "Content-Type", "application/json");
        esp_http_client_set_post_field(http_client_handle, metric_json_string, strlen(metric_json_string));
        esp_err_t ret = esp_http_client_perform(http_client_handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to perform POST request: %s. conetnt: %s", esp_err_to_name(ret), metric_json);
        }

        cJSON_free(metric_json);
    }
}

static esp_err_t http_event_handler(esp_http_client_event_t *event) { return ESP_OK; }

esp_err_t metrics_publisher_init(void)
{
    esp_err_t esp_ret;
    esp_http_client_config_t http_client_config = {
        .url = METRICS_PUBLISHER_ENDPOINT_URL,
        .event_handler = http_event_handler,
        .method = HTTP_METHOD_POST,
    };
    http_client_handle = esp_http_client_init(&http_client_config);
    if (http_client_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize http client.");
        goto cleanup_none;
    }

    BaseType_t rtos_ret = xTaskCreate(metrics_publisher_handler, "Metrics publisher", 8192, NULL, tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to craete task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_http_client;
    }

    return ESP_OK;

cleanup_http_client:
    esp_err_t cleanup_ret = esp_http_client_cleanup(http_client_handle);
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to clean up HTTP client: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_none:
    return esp_ret;
}

esp_err_t metrics_publisher_deinit(void)
{
    esp_err_t ret = esp_http_client_cleanup(http_client_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to clean up HTTP client: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
