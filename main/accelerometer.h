#pragma once

#include <esp_err.h>

/**
 * @brief Initializes the accelerometer sensor and starts the monitoring task.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t accelerometer_init(void);

/**
 * @brief Deinitializes the accelerometer sensor and stops the monitoring task.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t accelerometer_deinit(void);
