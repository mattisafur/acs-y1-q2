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

SemaphoreHandle_t time_sync_status = NULL;

static void sntp_callback(struct timeval *)
{
    ESP_LOGD(TAG, "Received time synchronnization event, giving semaphore...");
    xSemaphoreGive(time_sync_status);
}

esp_err_t time_sync_init(void)
{
    ESP_LOGI(TAG, "Creating time sync semaphore...");
    time_sync_status = xSemaphoreCreateBinary();
    if (time_sync_status == NULL)
    {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Setting operation mode...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);

    ESP_LOGI(TAG, "Setting time sync notification callback...");
    esp_sntp_set_time_sync_notification_cb(sntp_callback);

    ESP_LOGI(TAG, "Setting time servers...");
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");

    ESP_LOGI(TAG, "Initializing SNTP...");
    esp_sntp_init();

    ESP_LOGI(TAG, "Waiting for time to sync...");
    xSemaphoreTake(time_sync_status, portMAX_DELAY);

    ESP_LOGI(TAG, "Deleting time sync semaphore...");
    vSemaphoreDelete(time_sync_status);
    time_sync_status = NULL;

    return ESP_OK;
}
