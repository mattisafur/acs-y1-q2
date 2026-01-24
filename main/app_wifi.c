#include "app_wifi.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <nvs_flash.h>
#include <string.h>

static const char *TAG = "app wifi";

#define WIFI_SSID "AndrejHotspot"
#define WIFI_PASSWORD "azbm3134"

#define APP_WIFI_BIT_CONNECTED BIT0
#define APP_WIFI_BIT_FAIL BIT1

#define APP_WIFI_RECONNECT_MAX_RETRY 5

static EventGroupHandle_t wifi_event_group_handle = NULL;
static esp_event_handler_instance_t wifi_event_handler_instance;
static esp_event_handler_instance_t ip_event_handler_instance;

static void wifi_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *)
{
    static unsigned int reconnect_count = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (reconnect_count > APP_WIFI_RECONNECT_MAX_RETRY)
        {
            xEventGroupSetBits(wifi_event_group_handle, APP_WIFI_BIT_FAIL);
            return;
        }

        ESP_LOGW(TAG, "Disconnected from wifi, reconnecting...");
        esp_wifi_connect();
        reconnect_count++;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        reconnect_count = 0;
        xEventGroupSetBits(wifi_event_group_handle, APP_WIFI_BIT_CONNECTED);
    }
}

esp_err_t app_wifi_init(void)
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
            ESP_LOGE(TAG, "Failed to erase nvs flash: %s", esp_err_to_name(ret));
            goto cleanup_nothing;
        }
        ret = nvs_flash_init();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize nvs flash again: %s", esp_err_to_name(ret));
            goto cleanup_nothing;
        }
    }

    ESP_LOGI(TAG, "Creating wifi event group...");
    wifi_event_group_handle = xEventGroupCreate();
    if (wifi_event_group_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create wifi event group");
        goto cleanup_nvs_flash;
    }

    ESP_LOGI(TAG, "Initializing netif...");
    ret = esp_netif_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize network netif: %s", esp_err_to_name(ret));
        goto cleanup_wifi_event_group;
    }

    ESP_LOGI(TAG, "Creating default event loop...");
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        goto cleanup_netif;
    }

    ESP_LOGI(TAG, "Creating default wifi station...");
    esp_netif_create_default_wifi_sta();

    ESP_LOGI(TAG, "Initializing wifi...");
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_init_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize wifi: %s", esp_err_to_name(ret));
        goto cleanup_event_loop;
    }

    ESP_LOGI(TAG, "Registering wifi event handler...");
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_event_handler_instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register wifi event handler: %s", esp_err_to_name(ret));
        goto cleanup_wifi;
    }

    ESP_LOGI(TAG, "Registering ip event handler...");
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &ip_event_handler_instance);
    if (ret != ESP_OK)
    {
        ESP_LOGI(TAG, "Failed to register ip event handler: %s", esp_err_to_name(ret));
        goto cleanup_wifi_event_handler;
    }

    ESP_LOGI(TAG, "Setting wifi mode...");
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi mode: %s", esp_err_to_name(ret));
        goto cleanup_ip_event_handler;
    }

    ESP_LOGI(TAG, "Setting wifi config...");
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = APP_WIFI_SSID,
            .password = APP_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi config: %s", esp_err_to_name(ret));
        goto cleanup_ip_event_handler;
    }

    ESP_LOGI(TAG, "Starting wifi...");
    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start wifi: %s", esp_err_to_name(ret));
        goto cleanup_ip_event_handler;
    }

    ESP_LOGI(TAG, "Waiting for event bits...");
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group_handle, APP_WIFI_BIT_CONNECTED | APP_WIFI_BIT_FAIL, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & APP_WIFI_BIT_CONNECTED)
    {
        ESP_LOGI(TAG, "Received connection bit, connected to wifi");
    }
    else if (bits & APP_WIFI_BIT_FAIL)
    {
        ESP_LOGE(TAG, "Failed to connect to wifi.");
        goto cleanup_wifi_start;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to connect to wifi due to unexpected event.");
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
cleanup_ip_event_handler:
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
    vEventGroupDelete(wifi_event_group_handle);
cleanup_nvs_flash:
    cleanup_ret = nvs_flash_deinit();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize event group: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_nothing:
    return ret;
}

esp_err_t app_wifi_deinit(void)
{
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

    vEventGroupDelete(wifi_event_group_handle);

    ret = nvs_flash_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize event group: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
