#pragma once

#include <esp_err.h>

/**
 * @brief Initializes the card reader module.
 *
 * Configures the UART interface, GPIO enable pin and creates the
 * FreeRTOS task responsible for handling incoming RFID data.
 *
 * @return ESP_OK on success, or an error code on failure.
 */

esp_err_t card_reader_init(void);

/**
 * @brief Deinitializes the card reader module.
 *
 * Stops the associated FreeRTOS task, resets configured GPIO pins
 * and releases allocated UART resources.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t card_reader_deinit(void);
