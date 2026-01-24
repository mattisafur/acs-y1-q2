#include <esp_err.h>
#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "main";

#define SSID ""
#define PASSWORD ""
#define ENDPOINT_URL "http://4.233.137.69/ingest/metrics"
#define WIFI_RECONNECT_MAX_RETRY 5
#define JSON_DATA "{\"timestamp\": 0, \"metric_type\": \"METRIC_TYPE_ACCELEROMETER_ACCELERATION_X\", \"float_value\": 1.5}"

#define WIFI_BIT_CONNECTED BIT0
#define WIFI_BIT_FAIL BIT1

static EventGroupHandle_t wifi_event_group_handle = NULL;
static esp_event_handler_instance_t wifi_event_handler_instance;
static esp_event_handler_instance_t ip_event_handler_instance;
static esp_http_client_handle_t http_client_handle = NULL;

static void wifi_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *)
{
    static unsigned int reconnect_count = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (reconnect_count > WIFI_RECONNECT_MAX_RETRY)
        {
            xEventGroupSetBits(wifi_event_group_handle, WIFI_BIT_FAIL);
            return;
        }

        ESP_LOGW(TAG, "Disconnected from wifi, reconnecting...");
        esp_wifi_connect();
        reconnect_count++;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        reconnect_count = 0;
        xEventGroupSetBits(wifi_event_group_handle, WIFI_BIT_CONNECTED);
    }
}

static esp_err_t http_event_handler(esp_http_client_event_t *event)
{

    const char *event_id_name;
    switch (event->event_id)
    {
    case HTTP_EVENT_ERROR:
        event_id_name = "HTTP_EVENT_ERROR";
        break;
    case HTTP_EVENT_ON_CONNECTED:
        event_id_name = "HTTP_EVENT_ON_CONNECTED";
        break;
    case HTTP_EVENT_HEADERS_SENT:
        event_id_name = "HTTP_EVENT_HEADERS_SENT";
        break;
    case HTTP_EVENT_ON_HEADER:
        event_id_name = "HTTP_EVENT_ON_HEADER";
        break;
    case HTTP_EVENT_ON_DATA:
        event_id_name = "HTTP_EVENT_ON_DATA";
        break;
    case HTTP_EVENT_ON_FINISH:
        event_id_name = "HTTP_EVENT_ON_FINISH";
        break;
    case HTTP_EVENT_DISCONNECTED:
        event_id_name = "HTTP_EVENT_DISCONNECTED";
        break;
    case HTTP_EVENT_REDIRECT:
        event_id_name = "HTTP_EVENT_REDIRECT";
        break;
    default:
        ESP_LOGE(TAG, "Received an invalid event id: %d", event->event_id);
        event_id_name = "INVALID_EVENT_ID";
        break;
    }

    ESP_LOGI(TAG, "Received Event");
    ESP_LOGI(TAG, "==============");
    ESP_LOGI(TAG, "event id: %s", event_id_name);
    ESP_LOGI(TAG, "client pointer: %p", event->client);
    ESP_LOGI(TAG, "data: %.*s", event->data_len, event->data);
    ESP_LOGI(TAG, "user data pointer: %p", event->user_data);
    ESP_LOGI(TAG, "header key: %s", event->header_key ? event->header_key : "<No header key>");
    ESP_LOGI(TAG, "header value: %s", event->header_value ? event->header_value : "<No header value>");

    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing NVS flash...");
    ret = nvs_flash_init();
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to initialize nvs flash: %s. erasing flash.", esp_err_to_name(ret));
        ret = nvs_flash_erase();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to erase nvs flash: %s. aborting.", esp_err_to_name(ret));
            abort();
        }
        ret = nvs_flash_init();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize nvs flash again: %s. aborting.", esp_err_to_name(ret));
            abort();
        }
    }

    ESP_LOGI(TAG, "Creating wifi event group...");
    wifi_event_group_handle = xEventGroupCreate();
    if (wifi_event_group_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create wifi event group. aborting.");
        abort();
    }

    ESP_LOGI(TAG, "Initializing netif...");
    ret = esp_netif_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize network netif: %s. aborting.", esp_err_to_name(ret));
        abort();
    }

    ESP_LOGI(TAG, "Creating default event loop...");
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create default event loop: %s. aborting.", esp_err_to_name(ret));
        abort();
    }

    ESP_LOGI(TAG, "Creating default wifi station...");
    esp_netif_create_default_wifi_sta();

    ESP_LOGI(TAG, "Initializing wifi...");
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_init_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize wifi: %s. aborting.", esp_err_to_name(ret));
        abort();
    }

    ESP_LOGI(TAG, "Registering wifi event handler...");
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_event_handler_instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register wifi event handler: %s. aborting.", esp_err_to_name(ret));
        abort();
    }

    ESP_LOGI(TAG, "Registering ip event handler...");
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &ip_event_handler_instance);
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "Failed to register ip event handler: %s. aborting.", esp_err_to_name(ret));
        abort();
    }

    ESP_LOGI(TAG, "Setting wifi mode...");
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi mode: %s. aborting.", esp_err_to_name(ret));
        abort();
    }

    ESP_LOGI(TAG, "Setting wifi config...");
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi config: %s. aborting.", esp_err_to_name(ret));
        abort();
    }

    ESP_LOGI(TAG, "Starting wifi...");
    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start wifi: %s. aborting.", esp_err_to_name(ret));
        abort();
    }

    ESP_LOGI(TAG, "Waiting for event bits...");
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group_handle, WIFI_BIT_CONNECTED | WIFI_BIT_FAIL, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_BIT_CONNECTED)
    {
        ESP_LOGI(TAG, "Received connection bit, connected to wifi");
    }
    else if (bits & WIFI_BIT_FAIL)
    {
        ESP_LOGE(TAG, "Failed to connect to wifi. aborting");
        abort();
    }
    else
    {
        ESP_LOGE(TAG, "Failed to connect to wifi due to unexpected event. aborting");
        abort();
    }

    ESP_LOGI(TAG, "Initializing HTTP client...");
    esp_http_client_config_t http_client_config = {
        .url = ENDPOINT_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
    };
    http_client_handle = esp_http_client_init(&http_client_config);
    if (http_client_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client config. aborting.");
        abort();
    }

    ESP_LOGI(TAG, "Setting Content-Type header...");
    ret = esp_http_client_set_header(http_client_handle, "Content-Type", "application/json");
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set Content-Type header: %s. aborting.", esp_err_to_name(ret));
        abort();
    }

    ESP_LOGI(TAG, "Settings post field...");
    ret = esp_http_client_set_post_field(http_client_handle, JSON_DATA, strlen(JSON_DATA));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set post field: %s. aborting.", esp_err_to_name(ret));
        abort();
    }

    ESP_LOGI(TAG, "Perofrming HTTP client...");
    ret = esp_http_client_perform(http_client_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to perform HTTP client: %s. aborting", esp_err_to_name(ret));
        abort();
    }
}
