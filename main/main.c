// #include <driver/gpio.h>
// #include <mpu6050.h>
// #include <stdlib.h>

// static const char *TAG = "mpu6050_test";

// void app_main(void)
// {

//     ESP_ERROR_CHECK(i2cdev_init());

//     mpu6050_dev_t dev = {0}; // set all values in struct to 0

//     ESP_ERROR_CHECK(mpu6050_init_desc(&dev, MPU6050_I2C_ADDRESS_LOW, 0, GPIO_NUM_21, GPIO_NUM_22));

//     while (1)
//     {
//         esp_err_t res = i2c_dev_probe(&dev.i2c_dev, I2C_DEV_WRITE);
//         if (res == ESP_OK)
//         {
//             ESP_LOGI(TAG, "Found MPU60x0 device");
//             break;
//         }
//         ESP_LOGE(TAG, "MPU60x0 not found");
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }
