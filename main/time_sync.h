#pragma once

#include <esp_err.h>
#include <stdbool.h>

/**
 * @brief Synchronizes system time using SNTP.
 *
 * Initializes the SNTP client, configures time servers
 * and waits until the system time has been successfully
 * synchronized.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t time_sync_init(void);
