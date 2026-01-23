#pragma once
#include <stdbool.h>
#include <esp_err.h>

bool time_sync_is_valid(void);

// Calls Wi-Fi inside + SNTP sync.
// wifi_timeout_ms in milliseconds, sntp_timeout_seconds in seconds.
esp_err_t time_sync_ensure_epoch(int wifi_timeout_ms, int sntp_timeout_seconds);
