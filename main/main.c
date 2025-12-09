//TOF sensor 

#include <stdlib.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>

#define LOG_TAG "MAIN"

#define TOF400C_VL53L0X_ADDR 0x52
#define TOF400C_VL53L0X_SCL_FREQ_HZ 400000

void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(LOG_TAG, "Starting I2C Master Bus Initialization");
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
    ESP_LOGI(LOG_TAG, "I2C Master Bus Initialized Successfully");

    ESP_LOGI(LOG_TAG, "Adding I2C Device to Master Bus");
    i2c_master_dev_handle_t i2c_device_handle;
    i2c_device_config_t dev_config = {
        .dev_addr_length = 7, // TODO find value
        .device_address = TOF400C_VL53L0X_ADDR,
        .scl_speed_hz = TOF400C_VL53L0X_SCL_FREQ_HZ
        // .scl_wait_us
        // .flags = {
        //     .disable_ack_check
        // }
    };
    i2c_master_bus_add_device(i2c_master_handle, &dev_config, &i2c_device_handle);
    ESP_LOGI(LOG_TAG, "I2C Device Added Successfully");
}