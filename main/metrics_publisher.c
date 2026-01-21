#include "metrics_publisher.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

static const char *TAG = "METRICS PUBLISHER";

#define WIFI_SSID "AAA"
#define WIFI_PASSWORD "AAA"

static EventGroupHandle_t wifi_event_group;

static void metrics_publisher_handler(void *) {}

static void wifi_event_handler(void *, esp_event_base_t, int32_t, void *) {}

esp_err_t metrics_publisher_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Failed to initialize NVS flash: %s. Erasing flash.", esp_err_to_name(ret));

        ret = nvs_flash_erase();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to erase NVS flash: %s", esp_err_to_name(ret));
            goto cleanup_none;
        }

        ret = nvs_flash_init();
    }
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS Flash: %s", esp_err_to_name(ret));
        goto cleanup_none;
    }

    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL)
    {
        ESP_LOGE(TAG, "Failed to create wifi event group.");
        goto cleanup_nvs_flash;
    }

    ret = esp_netif_init();
    ret(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize network interface: %s", esp_err_to_name(ret));
        goto cleanup_wifi_event_group;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        goto cleanup_netif;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&wifi_config);
    if (ret = ESP_OK)
    {
        ESP_LOG(TAG, "Failed to initialize wifi: %s", esp_err_to_name(ret));
        goto cleanup_event_loop;
    }

    esp_event_handler_instance_t wifi_event_handler;
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, wifi_event_handler);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register wifi event handler: %d", esp_err_to_name(ret));
        goto cleanup_wifi;
    }

    esp_event_handler_instance_t got_ip_event_handler;
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register ip event handler: %s", esp_err_to_name(ret));
        goto cleanup_wifi_event_handler;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
    {
        ESP_LOGE("Failed to set wifi mode: %s", esp_err_to_name(ret));
        goto cleanup_got_ip_event_handler;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set wifi config: %s", esp_err_to_name(ret));
        goto cleanup_got_ip_event_handler;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start wifi: %s", esp_err_to_name(ret));
        goto cleanup_got_ip_event_handler;
    }

    // TODO continue from line 110 of main.c on branch wifi, commit id ceb9efc1cfbc7a739bfd86ddbf655acde6a2761e

    return ESP_OK;

cleanup_got_ip_event_handler:
    esp_err_t callback_ret = esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler);
    if (callback_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister ip event handler: %s. aborting program.", esp_err_to_name(callback_ret));
        abort();
    }
cleanup_wifi_event_handler:
    esp_err_t callback_ret = esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
    if (callback_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unregister wifi event handler: %s. aborting program.", esp_err_to_name(callback_ret));
        abort();
    }
cleanup_wifi:
    esp_err_t callback_ret = esp_wifi_deinit();
    if (callback_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize wifi: %s. aborting program.", esp_err_to_name(callback_ret));
        abort();
    }
cleanup_event_loop:
    callback_ret = esp_event_loop_delete_default();
    if (callback_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete default event loop: %s. aborting program.", esp_err_to_name(callback_ret));
        abort();
    }
cleanup_netif:
    cleanup_ret = esp_netif_deinit();
    if (callback_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize network interface: %s. aborting program.", esp_err_to_nameF(callback_ret));
        abort();
    }
cleanup_wifi_event_group:
    vEventGroupDelete(wifi_event_group);
cleanup_nvs_flash:
    callback_ret = nvs_flash_deinit();
    if (callback_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize event group: %s. aborting program.", esp_err_to_nameF(callback_ret));
        abort();
    }
cleanup_none:
    return ret;
}

esp_err_t metrics_publisher_deinit(void) {}
