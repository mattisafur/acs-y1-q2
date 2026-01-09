#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <mpu6050.h>

#include "include/i2c_addrs.h"
#include "include/pin_defs.h"

#define QUEUE_SIZE 16

#define RFID_RX_BUF_SIZE 256
#define RFID_BAUD_RATE 2400

static const char *TAG = "MAIN";

TaskHandle_t task_orchastrator_task_handle;
TaskHandle_t card_reader_task_handle;
TaskHandle_t speaker_task_handle;
TaskHandle_t accelerometer_task_handle;
TaskHandle_t time_of_flight_task_handle;
TaskHandle_t metric_publisher_task_handle;

QueueHandle_t task_return_queue_handle;
QueueHandle_t card_reader_queue_handle;
QueueHandle_t speaker_queue_handle;
QueueHandle_t accelerometer_queue_handle;
QueueHandle_t time_of_flight_queue_handle;
QueueHandle_t metric_queue_handle;

mpu6050_dev_t accelerometer_dev;

enum TaskQueueMessage
{
    CARD_VALID,
    CARD_INVALID,
};

typedef struct
{
    char *task_name;
    float value;
} MetricQueueMessage;

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

void app_main(void)
{
    init_card_reader();
    init_accelerometers();
    init_time_of_flight();
    init_buzzer();

    BaseType_t task_orchastrator_result = xTaskCreate(task_orchastrator_task_handler, "Task Orchastrator", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_orchastrator_task_handle);
    if (task_orchastrator_result == pdPASS)
    {
        ESP_LOGE(TAG, "Failed creating task \"Task Orchastrator\"");
    }

    BaseType_t card_reader_result = xTaskCreate(card_reader_task_handler, "Card Reader", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &card_reader_task_handle);
    if (card_reader_result == pdPASS)
    {
        ESP_LOGE(TAG, "Failed creating task \"Card Reader\"");
    }

    BaseType_t speaker_result = xTaskCreate(speaker_task_handler, "Speaker", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &speaker_task_handle);
    if (speaker_result == pdPASS)
    {
        ESP_LOGE(TAG, "Failed creating task \"Speaker\"");
    }

    BaseType_t accelerometer_result = xTaskCreate(accelerometer_task_handler, "Accelerometer", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &accelerometer_task_handle);
    if (accelerometer_result == pdPASS)
    {
        ESP_LOGE(TAG, "Failed creating task \"Accelerometer\"");
    }

    BaseType_t time_of_flight_result = xTaskCreate(time_of_flight_task_handler, "Time Of Flight", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &time_of_flight_task_handle);
    if (time_of_flight_result == pdPASS)
    {
        ESP_LOGE(TAG, "Failed creating task \"Time Of Flight\"");
    }

    BaseType_t metric_publisher_result = xTaskCreate(metric_publisher_task_handler, "Metric Publisher", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &metric_publisher_task_handle);
    if (metric_publisher_result == pdPASS)
    {
        ESP_LOGE(TAG, "Failed creating task \"Metric Publisher\"");
    }

    task_return_queue_handle = xQueueCreate(QUEUE_SIZE, sizeof(enum TaskQueueMessage));
    card_reader_queue_handle = xQueueCreate(QUEUE_SIZE, sizeof(enum TaskQueueMessage));
    speaker_queue_handle = xQueueCreate(QUEUE_SIZE, sizeof(enum TaskQueueMessage));
    accelerometer_queue_handle = xQueueCreate(QUEUE_SIZE, sizeof(enum TaskQueueMessage));
    time_of_flight_queue_handle = xQueueCreate(QUEUE_SIZE, sizeof(enum TaskQueueMessage));

    metric_queue_handle = xQueueCreate(QUEUE_SIZE, sizeof(MetricQueueMessage));
}
