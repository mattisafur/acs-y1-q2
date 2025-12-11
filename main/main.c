#include <driver/gpio.h>
#include <esp_log.h>
#include <math.h>
#include <mpu6050.h>
#include <stdbool.h>
#include <stdlib.h>

static const char *TAG = "mpu6050_test";

#define SDA GPIO_NUM_21
#define SCL GPIO_NUM_22
#define I2C_PORT I2C_NUM_0
#define ACCEL_ADDR MPU6050_I2C_ADDRESS_LOW
#define ACCEL_THR 150
#define ROTAT_THR 150

bool alarm_activated = false;

float calculate_vec_total(float x, float y, float z)
{
    return sqrtf(pow(x, 2) + pow(y, 2) + pow(z, 2));
}

void app_main(void)
{

    ESP_ERROR_CHECK(i2cdev_init());

    mpu6050_dev_t dev = {0}; // TODO check if setting to 0 is needed
    ESP_ERROR_CHECK(mpu6050_init_desc(&dev, ACCEL_ADDR, 0, SDA, SCL));

    // probe for sensor until it's found
    for (;;)
    {
        esp_err_t res = i2c_dev_probe(&dev.i2c_dev, I2C_DEV_WRITE);
        if (res == ESP_OK)
        {
            ESP_LOGI(TAG, "Found MPU60x0 device");
            break;
        }
        ESP_LOGE(TAG, "MPU60x0 not found");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(mpu6050_init(&dev));

    for (;;)
    {
        mpu6050_acceleration_t acceleration = {0};
        mpu6050_rotation_t rotation = {0};
        ESP_ERROR_CHECK(mpu6050_get_motion(&dev, &acceleration, &rotation));

        ESP_LOGI(TAG, "ax: %.4f ay: %.4f az: %.4f rx: %.4f ry: %.4f rz: %.4f ", acceleration.x, acceleration.y, acceleration.z, rotation.x, rotation.y, rotation.z);

        float total_acceleration = calculate_vec_total(acceleration.x, acceleration.y, acceleration.z);
        float total_rotation = calculate_vec_total(rotation.x, rotation.y, rotation.z);

        if (total_acceleration >= ACCEL_THR || total_rotation >= ROTAT_THR)
        {
            alarm_activated = true;
            for (;;)
            {
                ESP_LOGI(TAG, "alarm has been activated!!!");
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    }
}
