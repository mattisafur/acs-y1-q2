#include <esp_log.h>

#include "app_wifi.h"
#include "queue.h"
#include "task_orchastrator.h"
#include "time_sync.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_err_t ret = queue_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize queues: %s", esp_err_to_name(ret));
        goto cleanup_none;
    }

    ret = app_wifi_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize wifi: %s", esp_err_to_name(ret));
        goto cleanup_queue;
    }

    ret = time_sync_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to syncronize time: %s", esp_err_to_name(ret));
        goto cleanup_wifi;
    }

    ret = task_orchastrator_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize Task Orchestrator: %s", esp_err_to_name(ret));
        goto cleanup_wifi;
    }

    return;

cleanup_wifi:
    esp_err_t cleanup_ret = app_wifi_deinit();
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to deinitialize wifi: %s. aborting program.", esp_err_to_name(cleanup_ret));
        abort();
    }
cleanup_queue:
    queue_deinit();
cleanup_none:
    abort();
}
