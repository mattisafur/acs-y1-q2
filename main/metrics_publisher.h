#pragma once

#include <esp_err.h>

/**
 * @brief Initializes the metrics publisher module.
 *
 * Sets up the HTTP client and creates the FreeRTOS task responsible
 * for reading metrics from the metrics queue and sending them to the
 * configured remote endpoint using HTTP POST requests.
 *
 * @return ESP_OK on success, or an error code on failure.
 */

esp_err_t metrics_publisher_init(void);

/**
 * @brief Deinitializes the metrics publisher module.
 *
 * Cleans up the HTTP client and releases any allocated resources
 * associated with the metrics publishing functionality.
 *
 * @return ESP_OK on success, or an error code on failure.
 */

esp_err_t metrics_publisher_deinit(void);
