#pragma once

#include <esp_err.h>
#include <stdbool.h>
#include <time.h>

esp_err_t app_wifi_init(void);
esp_err_t app_wifi_deinit(void);
