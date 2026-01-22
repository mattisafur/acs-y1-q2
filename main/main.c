#include <esp_log.h>

#include "queue.h"
#include "task_orchastrator.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret = queue_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize queues: %s", esp_err_to_name(ret));
    }

    ret = task_orchastrator_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize Task Orchestrator: %s", esp_err_to_name(ret));
        goto cleanup_queue;
    }

    return;

cleanup_queue:
    queue_deinit();
cleanup_none:
    abort();
}
