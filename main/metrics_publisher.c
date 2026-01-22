#include "metrics_publisher.h"

#include <cJSON.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <nvs_flash.h>

#include "queue.h"

static const char *TAG = "METRICS PUBLISHER";

#define WIFI_SSID "AAA"
#define WIFI_PASSWORD "AAA"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define WIFI_RECONNECT_MAX_RETRY 5

static TaskHandle_t task_handle;

static EventGroupHandle_t wifi_event_group;
static esp_event_handler_instance_t wifi_event_handler_instance;
static esp_event_handler_instance_t got_ip_event_handler_instance;

static cJSON *metric_to_cjson(metric_t *metric)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "timestamp", metric->timestamp);

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
        ESP_LOGE(TAG, "Received unknown metric type with enum number %d", metric->metric_type);
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

        (void)metric_to_cjson(&msg);
    }
}

static void wifi_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static unsigned int wifi_reconnect_count = 0;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_err_t ret = esp_wifi_connect();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to connect to wifi: %s. aborting program", esp_err_to_name(ret));
            abort();
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (wifi_reconnect_count < WIFI_RECONNECT_MAX_RETRY)
        {
            ESP_LOGW(TAG, "Disconnected from wifi, trying to reconnect...");

            esp_err_t ret = esp_wifi_connect();
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to connect to wifi: %s. aborting program", esp_err_to_name(ret));
                abort();
            }
            wifi_reconnect_count++;
        }
        else
        {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        (void)event_data;
        wifi_reconnect_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t metrics_publisher_init(void)
{
    esp_err_t esp_ret = nvs_flash_init();
    if (esp_ret == ESP_ERR_NVS_NO_FREE_PAGES || esp_ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Failed to initialize NVS flash: %s. Erasing flash.", esp_err_to_name(esp_ret));

        esp_ret = nvs_flash_erase();
        if (esp_ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to erase NVS flash: %s", esp_err_to_name(esp_ret));
            goto cleanup_none;
        }

        esp_ret = nvs_flash_init();
    }
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS Flash: %s", esp_err_to_name(esp_ret));
        goto cleanup_none;
    }

    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL)
    {
        ESP_LOGE(TAG, "Failed to create wifi event group.");
        goto cleanup_nvs_flash;
    }

    esp_ret = esp_netif_init();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize network interface: %s", esp_err_to_name(esp_ret));
        goto cleanup_wifi_event_group;
    }

    esp_ret = esp_event_loop_create_default();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(esp_ret));
        goto cleanup_netif;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_ret = esp_wifi_init(&wifi_init_config);
    if (esp_ret == ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize wifi: %s", esp_err_to_name(esp_ret));
        goto cleanup_event_loop;
    }

    esp_ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_event_handler_instance);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register wifi event handler: %s", esp_err_to_name(esp_ret));
        goto cleanup_wifi;
    }

    esp_ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &got_ip_event_handler_instance);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register ip event handler: %s", esp_err_to_name(esp_ret));
        goto cleanup_wifi_event_handler;
    }

    esp_ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi mode: %s", esp_err_to_name(esp_ret));
        goto cleanup_got_ip_event_handler;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi config: %s", esp_err_to_name(esp_ret));
        goto cleanup_got_ip_event_handler;
    }

    esp_ret = esp_wifi_start();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start wifi: %s", esp_err_to_name(esp_ret));
        goto cleanup_got_ip_event_handler;
    }

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGD(TAG, "Received connection bit, connected to wifi");
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGE(TAG, "Failed to connect to wifi");
        goto cleanup_wifi_start;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to connect to wifi due to unexpected event");
        goto cleanup_wifi_start;
    }

    BaseType_t rtos_ret = xTaskCreate(metrics_publisher_handler, "Metrics Publisher", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_wifi_start;
    }

    return ESP_OK;

cleanup_wifi_start:
    esp_err_t cleanup_ret = esp_wifi_stop();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop wifi: %s, aborting program", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_got_ip_event_handler:
    cleanup_ret = esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler_instance);
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister ip event handler: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_wifi_event_handler:
    cleanup_ret = esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance);
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister wifi event handler: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_wifi:
    cleanup_ret = esp_wifi_deinit();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize wifi: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_event_loop:
    cleanup_ret = esp_event_loop_delete_default();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete default event loop: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_netif:
    cleanup_ret = esp_netif_deinit();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize network interface: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_wifi_event_group:
    vEventGroupDelete(wifi_event_group);
cleanup_nvs_flash:
    cleanup_ret = nvs_flash_deinit();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize event group: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_none:
    return esp_ret;
}

esp_err_t metrics_publisher_deinit(void)
{
    vTaskDelete(task_handle);
    task_handle = NULL;

    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop wifi: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler_instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister ip event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister wifi event handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize wifi: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_delete_default();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete default event loop: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_netif_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize network interface: %s", esp_err_to_name(ret));
        return ret;
    }

    vEventGroupDelete(wifi_event_group);

    ret = nvs_flash_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize event group: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
