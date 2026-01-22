#include "i2c_master_bus.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>

static const char *TAG = "I2C MASTER BUS";

#define I2C_MASTER_BUS_PORT_NUM 0
#define I2C_MASTER_BUS_GPIO_SDA GPIO_NUM_21
#define I2C_MASTER_BUS_GPIO_SCL GPIO_NUM_22

i2c_master_bus_handle_t i2c_master_bus_handle;

esp_err_t i2c_master_bus_init(void)
{
    i2c_master_bus_config_t master_bus_config = {
        .i2c_port = 0,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&master_bus_config, i2c_master_bus_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create new I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t i2c_master_bus_deinit(void)
{
    esp_err_t ret = i2c_del_master_bus(i2c_master_bus_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
