#include "wifi_manager.h"

#include <string.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <nvs_flash.h>

static const char *TAG = "WIFI_MANAGER";

#define WIFI_SSID "AAA"
#define WIFI_PASSWORD "AAA"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define WIFI_RECONNECT_MAX_RETRY 5

static EventGroupHandle_t wifi_event_group;
static esp_event_handler_instance_t wifi_event_handler_instance;
static esp_event_handler_instance_t got_ip_event_handler_instance;

static bool s_inited = false;
static bool s_connected = false;

static void wifi_event_handler(void *, esp_event_base_t event_base, int32_t event_id, void *event_data)
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
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        (void)event_data;
        wifi_reconnect_count = 0;
        s_connected = true;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

esp_err_t wifi_manager_init(void)
{
    if (s_inited) return ESP_OK;

    esp_err_t esp_ret = nvs_flash_init();
    if (esp_ret == ESP_ERR_NVS_NO_FREE_PAGES || esp_ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS init failed (%s), erasing...", esp_err_to_name(esp_ret));
        ESP_ERROR_CHECK(nvs_flash_erase());
        esp_ret = nvs_flash_init();
    }
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL)
    {
        ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
        return ESP_ERR_NO_MEM;
    }

    esp_ret = esp_netif_init();
    if (esp_ret != ESP_OK && esp_ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    esp_ret = esp_event_loop_create_default();
    if (esp_ret != ESP_OK && esp_ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_ret = esp_wifi_init(&wifi_init_config);
    if (esp_ret != ESP_OK)  // ✅ fixed
    {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    esp_ret = esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_event_handler_instance);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register WIFI handler: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    esp_ret = esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &got_ip_event_handler_instance);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register IP handler: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    esp_ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set mode: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    esp_ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set config: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    esp_ret = esp_wifi_start();
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    s_inited = true;
    return ESP_OK;
}

esp_err_t wifi_manager_ensure_connected(int timeout_ms)
{
    esp_err_t ret = wifi_manager_init();
    if (ret != ESP_OK) return ret;

    if (s_connected) return ESP_OK;

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to Wi-Fi");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to connect to Wi-Fi");
    return ESP_FAIL;
}

esp_err_t wifi_manager_deinit(void)
{
    if (!s_inited) return ESP_OK;

    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) return ret;

    // ✅ correct unregister for instance handlers
    ret = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_handler_instance);
    if (ret != ESP_OK) return ret;

    ret = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_instance);
    if (ret != ESP_OK) return ret;

    ret = esp_wifi_deinit();
    if (ret != ESP_OK) return ret;

    vEventGroupDelete(wifi_event_group);
    wifi_event_group = NULL;

    s_inited = false;
    s_connected = false;
    return ESP_OK;
}
