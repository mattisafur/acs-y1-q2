#pragma once

#include <esp_err.h>

/**
 * @brief Initializes the task orchestrator module.
 *
 * This function initializes all required system modules
 * (sensors, buzzer, card reader, metrics publisher) and
 * creates the FreeRTOS task that handles incoming messages
 * and controls the system logic.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t task_orchastrator_init(void);
