#pragma once
#include <stdbool.h>
#include <esp_err.h>

// Safe to call multiple times
esp_err_t wifi_manager_init(void);

// Safe to call multiple times. Returns ESP_OK if already connected.
// timeout_ms is milliseconds.
esp_err_t wifi_manager_ensure_connected(int timeout_ms);

bool wifi_manager_is_connected(void);

// Optional cleanup
esp_err_t wifi_manager_deinit(void);
