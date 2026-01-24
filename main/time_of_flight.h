#pragma once

#include <esp_err.h>

/**
 * @brief Initializes the time of flight sensor.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t time_of_flight_init(void);

/**
 * @brief Deinitializes the time of flight sensor.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t time_of_flight_deinit(void);
