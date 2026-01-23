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

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define WIFI_RECONNECT_MAX_RETRY 5

static EventGroupHandle_t wifi_event_group_handle;
static esp_event_handler_instance_t wifi_event_handler_instance;
static esp_event_handler_instance_t got_ip_event_handler_instance;

static bool s_connected = false;

static void app_wifi_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static unsigned int wifi_reconnect_count = 0;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        s_connected = false;

        if (wifi_reconnect_count < WIFI_RECONNECT_MAX_RETRY)
        {
            ESP_LOGW(TAG, "Disconnected from Wi-Fi, reconnecting...");
            esp_wifi_connect();
            wifi_reconnect_count++;
        }
        else
        {
            xEventGroupSetBits(wifi_event_group_handle, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        (void)event_data;
        wifi_reconnect_count = 0;
        s_connected = true;
        xEventGroupSetBits(wifi_event_group_handle, WIFI_CONNECTED_BIT);
    }
}

esp_err_t app_wifi_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Failed to initialize NVS flash: %s. erasing flash...", esp_err_to_name(ret));
        ret = nvs_flash_erase();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to erase NVS flash: %s.", esp_err_to_name(ret));
            goto cleanup_none;
        }
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        goto cleanup_none;
    }

    wifi_event_group_handle = xEventGroupCreate();
    if (wifi_event_group_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
        goto cleanup_nvs_flash;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Failed to initialize network interface: %s", esp_err_to_name(ret));
        goto cleanup_wifi_event_group;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        goto cleanup_netif;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_init_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize wifi: %s", esp_err_to_name(ret));
        goto cleanup_event_loop;
    }

    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &app_wifi_event_handler, NULL, &wifi_event_handler_instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register wifi event handler: %s", esp_err_to_name(ret));
        goto cleanup_wifi;
    }

    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &app_wifi_event_handler, NULL, &got_ip_event_handler_instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register ip event handler: %s", esp_err_to_name(ret));
        goto cleanup_wifi_event_handler;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi mode: %s", esp_err_to_name(ret));
        goto cleanup_ip_event_handler;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi config: %s", esp_err_to_name(ret));
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set config: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(ret));
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start wifi: %s", esp_err_to_name(ret));
    }

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group_handle, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
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
cleanup_none:
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
        ESP_LOGE(TAG, "Failed to unregister ip event handler: %s.", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister wifi event handler: %s.", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize wifi: %s.", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_delete_default();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete default event loop: %s.", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_netif_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize network interface: %s.", esp_err_to_name(ret));
        return ret;
    }

    vEventGroupDelete(wifi_event_group_handle);

    ret = nvs_flash_deinit();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize NVS flash: %s. aborting program.", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
