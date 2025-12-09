#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <stdlib.h>

#define LOG_TAG "MAIN"

#define MPU_6050_I2C_ADDR 0x68
#define MPU_6050_I2C_SCL_SPEED_HZ 400000

#define HP_5883_I2C_ADDR 0x2C // NOTE needs validation, might be 0x77
#define HP_5883_I2C_SCL_SPEED_HZ 400000
#define HP_5883_I2C_ADDR_LEN 7

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

    i2c_master_dev_handle_t mpu_6050_i2c_handle;
    i2c_device_config_t mpu_6050_i2c_config = {
        .dev_addr_length = 7,
        .device_address = MPU_6050_I2C_ADDR,
        .scl_speed_hz = MPU_6050_I2C_SCL_SPEED_HZ,
        // .scl_wait_us
        // .flags = {
        //     .disable_ack_check
        // }
    };
    err = i2c_master_bus_add_device(i2c_master_handle, &mpu_6050_i2c_config, &mpu_6050_i2c_handle);
    ESP_ERROR_CHECK(err);
    ESP_LOGI(LOG_TAG, "initialized i2c for MPU6050");

    i2c_master_dev_handle_t hp_5883_i2c_handle;
    i2c_device_config_t hp_5883_i2c_config = {
        .dev_addr_length = HP_5883_I2C_ADDR_LEN,
        .device_address = HP_5883_I2C_ADDR,
        .scl_speed_hz = HP_5883_I2C_SCL_SPEED_HZ,
        // .scl_wait_us
        // .flags = {
        //     .disable_ack_check
        // }
    };
    err = i2c_master_bus_add_device(i2c_master_handle, &hp_5883_i2c_config, &hp_5883_i2c_handle);
    ESP_ERROR_CHECK(err);
    ESP_LOGI(LOG_TAG, "initialized i2c for HP5833");
}
