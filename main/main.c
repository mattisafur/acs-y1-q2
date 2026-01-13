#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <mpu6050.h>

#include "app_config.h"

#define QUEUE_SIZE 16

#define RFID_RX_BUF_SIZE 256

static const char *TAG = "MAIN";

TaskHandle_t task_orchastrator_task_handle;
TaskHandle_t card_reader_task_handle;
TaskHandle_t speaker_task_handle;
TaskHandle_t accelerometer_task_handle;
TaskHandle_t time_of_flight_task_handle;
TaskHandle_t metric_publisher_task_handle;

mpu6050_dev_t accelerometer_dev;

void task_orchastrator_task_handler(void *args) {}
void card_reader_task_handler(void *args) {}
void speaker_task_handler(void *args) {}
void accelerometer_task_handler(void *args) {}
void time_of_flight_task_handler(void *args) {}
void metric_publisher_task_handler(void *args) {}

void init_card_reader()
{

    uart_config_t uart_config = {
        .baud_rate = RFID_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // Set UART parameters
    ESP_ERROR_CHECK(uart_param_config(RFID_UART_NUM, &uart_config));

    // Set UART pins
    ESP_ERROR_CHECK(uart_set_pin(RFID_UART_NUM,
                                 PIN_RFID_UART_TX,
                                 PIN_RFID_UART_RX,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    // Install UART driver ONCE
    ESP_ERROR_CHECK(uart_driver_install(RFID_UART_NUM,
                                        RFID_RX_BUF_SIZE,
                                        0,
                                        0,
                                        NULL,
                                        0));

    ESP_LOGI(TAG, "RFID UART initialized on RX=%d TX=%d", PIN_RFID_UART_RX, PIN_RFID_UART_TX);
}

void init_accelerometer()
{
    ESP_ERROR_CHECK(mpu6050_init_desc(&accelerometer_dev, I2C_ADDR_MPU6050, I2C_NUM, PIN_I2C_SDA, PIN_I2C_SCL));

    for (;;)
    {
        esp_err_t res = i2c_dev_probe(&accelerometer_dev.i2c_dev, I2C_DEV_WRITE);

        if (res == ESP_OK)
        {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }

    ESP_ERROR_CHECK(mpu6050_init(&accelerometer_dev));
}
void init_time_of_flight() {}

void init_buzzer()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1 << PIN_BUZZER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

void app_main(void) {}
