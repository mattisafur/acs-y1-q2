#include "time_sync.h"

#include <time.h>
#include <sys/time.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi_manager.h"

static const char *TAG = "TIME_SYNC";
static bool s_sntp_started = false;

static void sntp_cb(struct timeval *tv) { (void)tv; }

bool time_sync_is_valid(void)
{
    time_t now = time(NULL);
    struct tm tm_now;
    gmtime_r(&now, &tm_now);
    return (tm_now.tm_year + 1900) >= 2024; // avoid 1970
}

static void sntp_start_once(void)
{
    if (s_sntp_started) return;

    ESP_LOGI(TAG, "Starting SNTP...");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_time_sync_notification_cb(sntp_cb);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();

    s_sntp_started = true;
}

esp_err_t time_sync_ensure_epoch(int wifi_timeout_ms, int sntp_timeout_seconds)
{
    // ✅ Wi-Fi call here (as you want)
    esp_err_t ret = wifi_manager_ensure_connected(wifi_timeout_ms);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Wi-Fi not connected, cannot SNTP sync");
        return ret;
    }

    sntp_start_once();

    for (int i = 0; i < sntp_timeout_seconds; i++)
    {
        if (time_sync_is_valid())
        {
            ESP_LOGI(TAG, "Time synced. epoch=%ld", (long)time(NULL));
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGW(TAG, "SNTP timeout. epoch=%ld", (long)time(NULL));
    return ESP_ERR_TIMEOUT;
}
