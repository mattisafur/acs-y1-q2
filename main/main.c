#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <stdlib.h>

#define LOG_TAG "MAIN"

void app_main(void)
{
    esp_err_t err;

    i2c_master_bus_handle_t i2c_master_handle;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        // .intr_priority
        // .trans_queue_depth
        .flags = {
            .enable_internal_pullup = true,
            // .allow_pd
        },
    };
    err = i2c_new_master_bus(&bus_config, &i2c_master_handle);
    ESP_ERROR_CHECK(err);
    ESP_LOGI(LOG_TAG, "initialized i2c master");

    i2c_master_dev_handle_t i2c_device_1_handle;
    i2c_device_config_t i2c_device_1_config = {
        .dev_addr_length = 7, // TODO find value
        .device_address = 0x68,
        .scl_speed_hz = 100000
        // .scl_wait_us
        // .flags = {
        //     .disable_ack_check
        // }
    };
    err = i2c_master_bus_add_device(i2c_master_handle, &i2c_device_1_config, &i2c_device_1_handle);
    ESP_ERROR_CHECK(err);
    ESP_LOGI(LOG_TAG, "initialized i2c device 1");

    i2c_master_dev_handle_t i2c_device_2_handle;
    i2c_device_config_t i2c_device_2_config = {
        .dev_addr_length = 7, // TODO find value
        .device_address = 0x77,
        .scl_speed_hz = 100000
        // .scl_wait_us
        // .flags = {
        //     .disable_ack_check
        // }
    };
    err = i2c_master_bus_add_device(i2c_master_handle, &i2c_device_2_config, &i2c_device_2_handle);
    ESP_ERROR_CHECK(err);
    ESP_LOGI(LOG_TAG, "initialized i2c device 2");
}
