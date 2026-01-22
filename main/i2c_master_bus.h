#pragma once

#include <driver/i2c_master.h>

extern i2c_master_bus_handle_t i2c_master_bus_handle;

esp_err_t i2c_master_bus_init(void);
esp_err_t i2c_master_bus_deinit(void);
