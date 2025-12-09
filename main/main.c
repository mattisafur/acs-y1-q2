#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "CardReader"

#define BLINK_GPIO GPIO_NUM_23
// UART configuration
#define RFID_UART_NUM UART_NUM_2
#define RFID_TX_PIN (GPIO_NUM_17) // not connected to reader, just required by driver
#define RFID_RX_PIN (GPIO_NUM_16) // connect SOUT of 28140 here

#define RFID_BAUD_RATE 2400

#define APP_BUF_SIZE 64
#define RX_BUF_SIZE 256

#define RFID_TAG_ID "01004B1DA2"


static void rfid_task(void *arg)
{

    uint8_t data[APP_BUF_SIZE];

    ESP_LOGI(TAG, "RFID task started, waiting for tags...");

    while (1)
    {
        // Wait up to 1000 ms for data
        int len = uart_read_bytes(RFID_UART_NUM, data, APP_BUF_SIZE - 1, pdMS_TO_TICKS(200));

        if (len > 0)
        {
            gpio_set_level(BLINK_GPIO, 1);
            data[12] = '\0';
            // Print HEX for debugging
            printf("HEX: ");
            for (int i = 0; i < len && i < 12; i++)
            {
                printf("%02X ", data[i]);
            }
            printf("\n");
            

            // Try to parse classic 0A + 10 ASCII + 0D format
            if (len >= 12 && data[0] == 0x0A && data[11] == 0x0D)
            {
                char id[11];
                memcpy(id, &data[1], 10);
                id[10] = '\0';

                if (strcmp(id, RFID_TAG_ID) == 0)
                {
                    ESP_LOGI(TAG, "RFID TAG CORRECT!");
                }
                else
                {
                    ESP_LOGI(TAG, "RFID TAG INCORRECT!");
                }
            }
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(BLINK_GPIO, 0);

            while (len > 0)
            {
                len = uart_read_bytes(RFID_UART_NUM, data, APP_BUF_SIZE - 1, pdMS_TO_TICKS(1000));
            }
        }
    }
}

void app_main(void)
{
    gpio_config_t led_conf = {
        .pin_bit_mask = 1ULL << BLINK_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);
    gpio_set_level(BLINK_GPIO, 0); 
    // UART configuration
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
                                 RFID_TX_PIN,
                                 RFID_RX_PIN,
                                 UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    // Install UART driver ONCE
    ESP_ERROR_CHECK(uart_driver_install(RFID_UART_NUM,
                                        RX_BUF_SIZE,
                                        0,
                                        0,
                                        NULL,
                                        0));

    ESP_LOGI(TAG, "RFID UART initialized on RX=%d TX=%d", RFID_RX_PIN, RFID_TX_PIN);

    // Start task
    xTaskCreate(rfid_task, "rfid_task", 4096, NULL, 10, NULL);
}