#include "time_sync.h"

#include <esp_err.h>
#include <esp_log.h>
#include <esp_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>
#include <time.h>

#include "app_wifi.h"

static const char *TAG = "time sync";

static void sntp_cb(struct timeval *) {}

esp_err_t time_sync_init(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_time_sync_notification_cb(sntp_cb);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(2, "time.google.com");
    esp_sntp_init();

    time_t now = time(NULL);
    struct tm tm_now;
    unsigned int tries = 5;
    do
    {
        gmtime_r(&now, &tm_now);
        if (tries == 0)
        {
            ESP_LOGE(TAG, "Failed to synchronize time due to timeout.");
            return ESP_ERR_TIMEOUT;
        }
        tries--;
        vTaskDelay(pdMS_TO_TICKS(1000));
    } while (tm_now.tm_year + 1900 >= 2024);

    return ESP_OK;
}
