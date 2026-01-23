#include "card_reader.h"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "app_config.h"
#include "queue.h"

#define CARD_READER_UART_RX_BUFFER_SIZE 256
#define CARD_READER_DEBOUNCE_MS 300
#define CARD_READER_TIMEOUT_MS 500
#define CARD_READER_UID_MAX_LEN 10
#define CARD_READER_UART_NUM UART_NUM_1
#define CARD_READER_UART_BAUD_RATE 2400
#define CARD_READER_UART_GPIO_RX GPIO_NUM_26
#define CARD_READER_UART_GPIO_TX GPIO_NUM_25
#define CARD_READER_GPIO_ENABLE GPIO_NUM_27
#define CARD_READER_TAG_ID "01004B1DA2"

static const char *TAG = "card reader";

static TaskHandle_t task_handle;

static void card_reader_task_handler(void *)
{

    bool valid = false;
    for (;;)
    {
        
        uint8_t uid[12] = {0};
        size_t uid_len = sizeof(uid);

        int len = uart_read_bytes(CARD_READER_UART_NUM, uid, uid_len, pdMS_TO_TICKS(100));
        if (len > 0)
        {
            uid[len] = '\0'; // Null-terminate the received data
            ESP_LOGI(TAG, "Received RFID tag ID: %s", (char *)uid);
            if (len >= 12 && uid[0] == 0x0A && uid[11] == 0x0D)
            {
                char id[11];
                memcpy(id, &uid[1], 10);
                id[10] = '\0';
                if (strcmp(id, CARD_READER_TAG_ID) == 0)
                {
                    ESP_LOGI(TAG, "Valid RFID tag detected: %s", id);
                    valid = true;

                    orchastrator_return_message_t tx_msg = {
                        .component = COMPONENT_CARD_READER,
                        .message = valid ? MESSAGE_CARD_READER_CARD_VALID : MESSAGE_CARD_READER_CARD_INVALID,
                    };
                    BaseType_t q_ret = xQueueSendToBack(queue_task_return_handle, &tx_msg, 0);
                    if (q_ret != pdPASS)
                    {
                        ESP_LOGE(TAG, "Failed to send card read result to queue with error code: %d", q_ret);
                    }

                    metric_t metric_card_reader_valid = {
                        .metric_type = METRIC_TYPE_CARD_READER_VALID,
                        .timestamp = time(NULL),
                        .bool_value = valid ? true : false,
                    };
                    xQueueSendToBack(queue_metrics_handle, &metric_card_reader_valid, 0);
                }
                else
                {
                    ESP_LOGW(TAG, "Invalid RFID tag detected: %s", id);
                    valid = false;

                    orchastrator_return_message_t tx_msg = {
                        .component = COMPONENT_CARD_READER,
                        .message = valid ? MESSAGE_CARD_READER_CARD_VALID : MESSAGE_CARD_READER_CARD_INVALID,
                    };
                    BaseType_t q_ret = xQueueSendToBack(queue_card_reader_handle, &tx_msg, 0);
                    if (q_ret != pdPASS)
                    {
                        ESP_LOGE(TAG, "Failed to send card read result to queue with error code: %d", q_ret);
                    }

                    metric_t metric_card_reader_valid = {
                        .metric_type = METRIC_TYPE_CARD_READER_VALID,
                        .timestamp = time(NULL),
                        .bool_value = valid ? true : false,
                    };
                    xQueueSendToBack(queue_metrics_handle, &metric_card_reader_valid, portMAX_DELAY);
                }
            }
            while (len > 0)
            {
                uint8_t discard;
                len = uart_read_bytes(CARD_READER_UART_NUM, &discard, 1, pdMS_TO_TICKS(1000));
                
            }
        }
    }
}

esp_err_t card_reader_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = CARD_READER_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // Set UART parameters
    esp_err_t esp_ret = uart_param_config(CARD_READER_UART_NUM, &uart_config);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure UART parameters: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    // Set UART pins
    esp_ret = uart_set_pin(CARD_READER_UART_NUM,
                           CARD_READER_UART_GPIO_TX,
                           CARD_READER_UART_GPIO_RX,
                           UART_PIN_NO_CHANGE,
                           UART_PIN_NO_CHANGE);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(esp_ret));
        return esp_ret;
    }

    // Install UART driver ONCE
    esp_ret = uart_driver_install(CARD_READER_UART_NUM,
                                  CARD_READER_UART_RX_BUFFER_SIZE,
                                  0,
                                  0,
                                  NULL,
                                  0);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(esp_ret));
        goto cleanup_uart;
    }
    gpio_config_t config_enable = {
        .pin_bit_mask = 1ULL << CARD_READER_GPIO_ENABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_ret = gpio_config(&config_enable);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure enable gpio: %s", esp_err_to_name(esp_ret));
        goto cleanup_gpio;
    }

    ESP_LOGD(TAG, "Setting enable pin level to low...");
    esp_ret = gpio_set_level(CARD_READER_GPIO_ENABLE, 0);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set enable pin level to low: %s", esp_err_to_name(esp_ret));
        goto cleanup_uart;
    }

    BaseType_t rtos_ret = xTaskCreate(card_reader_task_handler, "Card Reader", APP_CONFIG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create card reader task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_uart;
    }

    return ESP_OK;

cleanup_uart:
    esp_err_t cleanup_esp_ret = uart_driver_delete(CARD_READER_UART_NUM);
    if (cleanup_esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete UART driver: %s. aborting program", esp_err_to_name(cleanup_esp_ret));
        abort();
    }
cleanup_gpio:
    cleanup_esp_ret = gpio_reset_pin(CARD_READER_GPIO_ENABLE);
    if (cleanup_esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to reset the gpio pin: %s. aborting program", esp_err_to_name(cleanup_esp_ret));
        abort();
    }
    return esp_ret;
}

esp_err_t card_reader_deinit(void)
{
    vTaskDelete(task_handle);
    task_handle = NULL;

    esp_err_t ret = uart_driver_delete(CARD_READER_UART_NUM);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to delete UART driver: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_reset_pin(CARD_READER_GPIO_ENABLE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to reset the gpio pin: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
