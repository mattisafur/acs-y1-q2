#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <time.h>

/**
 * @brief Initializes the WiFi subsystem in station mode.
 *
 * Sets up NVS, network interfaces, event loop and connects to the configured
 * access point. Blocks until connection is established or fails.
 *
 * @return ESP_OK on successful connection, otherwise an error code.
 */
esp_err_t app_wifi_init(void);

/**
 * @brief Deinitializes the WiFi subsystem.
 *
 * Stops WiFi, unregisters event handlers and cleans up
 * allocated resources.
 *
 * @return ESP_OK on success, otherwise an error code.
 */
esp_err_t app_wifi_deinit(void);
