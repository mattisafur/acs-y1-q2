#pragma once

#include <esp_err.h>

/**
 * @brief Initializes the buzzer module.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t buzzer_init(void);

/**
 * @brief Deinitializes the buzzer module.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t buzzer_deinit(void);
