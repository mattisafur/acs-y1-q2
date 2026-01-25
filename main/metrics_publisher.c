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

#define METRICS_PUBLISHER_ENDPOINT_URL "http://4.233.137.69/ingest/metrics"

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
        ESP_LOGE(TAG, "Unknown metric type: %s", queue_metric_type_to_name(metric->metric_type));
        break;
    }

    return json;
}

static void metrics_publisher_handler(void *)
{
    for (;;)
    {
        metric_t msg;
        xQueueReceive(queue_handle_metrics, &msg, portMAX_DELAY);

        cJSON *metric_json = metric_to_cjson(&msg);
        char *metric_json_string = cJSON_Print(metric_json);

        esp_err_t ret = esp_http_client_set_header(http_client_handle, "Content-Type", "application/json");
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set http client header: %s", esp_err_to_name(ret));
        }

        ret = esp_http_client_set_post_field(http_client_handle, metric_json_string, strlen(metric_json_string));
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to set http client post field: %s", esp_err_to_name(ret));
        }

        ret = esp_http_client_perform(http_client_handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to perform POST request: %s.", esp_err_to_name(ret));
        }

        free(metric_json_string);
        cJSON_free(metric_json);
    }
}

static esp_err_t http_event_handler(esp_http_client_event_t *event) { return ESP_OK; }

esp_err_t metrics_publisher_init(void)
{
    esp_err_t esp_ret;
    BaseType_t rtos_ret;
    esp_err_t cleanup_ret;

    ESP_LOGI(TAG, "Initializing HTTP client...");
    esp_http_client_config_t http_client_config = {
        .url = METRICS_PUBLISHER_ENDPOINT_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
    };
    http_client_handle = esp_http_client_init(&http_client_config);
    if (http_client_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize http client.");
        goto cleanup_none;
    }

    ESP_LOGI(TAG, "Creating task...");
    rtos_ret = xTaskCreate(metrics_publisher_handler, "Metrics publisher", APP_CONFIG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to craete task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_http_client;
    }

    return ESP_OK;

cleanup_http_client:
    ESP_LOGI(TAG, "Cleaning up HTTP client...");
    cleanup_ret = esp_http_client_cleanup(http_client_handle);
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
    esp_err_t ret;

    ESP_LOGI(TAG, "Cleaning up HTTP client...");
    ret = esp_http_client_cleanup(http_client_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to clean up HTTP client: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
