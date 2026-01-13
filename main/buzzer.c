#include "buzzer.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "queue.h"

#define PIN_BUZZER GPIO_NUM_12

#define ALARM_QUEUE_SIZE_ITEMS 16

static const char *TAG = "BUZZER";

static TaskHandle_t buzzer_task_handle;
static TaskHandle_t alarm_task_handle;

static QueueHandle_t alarm_queue_handle;

typedef enum alarm_queue_message_t
{
    ALARM_QUEUE_MESSAGE_START,
    ALARM_QUEUE_MESSAGE_STOP,
} alarm_queue_message_t;

esp_err_t buzzer_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1 << PIN_BUZZER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t esp_ret = gpio_config(&config);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure gpio: %s", esp_err_to_name(esp_ret));
        goto cleanup_gpio;
    }

    esp_ret = gpio_set_level(PIN_BUZZER, 0);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set gpio initial state: %s", esp_err_to_name(esp_ret));
        goto cleanup_gpio;
    }

    BaseType_t rtos_ret = xTaskCreate(buzzer_task_handler, "Buzzer", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &buzzer_task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create buzzer task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_gpio;
    }

    const BaseType_t rtos_ret = xTaskCreate(alarm_task_handler, "Buzzer Alarm", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &alarm_task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create alarm task with error code: %d", rtos_ret);
        return rtos_ret;
    }

    alarm_queue_handle = xQueueCreate(ALARM_QUEUE_SIZE_ITEMS, sizeof(alarm_queue_message_t));
    if (alarm_queue_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize alarm queue");
        goto cleanup_alarm_task;
    }

    return ESP_OK;

cleanup_alarm_task:
    vTaskDelete(alarm_task_handle);
cleanup_buzzer_task:
    vTaskDelete(buzzer_task_handle);
cleanup_gpio:
    gpio_reset_pin(PIN_BUZZER);
    return esp_ret;
}

static void buzzer_task_handler(void *)
{
    for (;;)
    {
        message_t msg;
        const BaseType_t ret = xQueueReceive(queue_buzzer_handle, &msg, portMAX_DELAY);

        switch (msg)
        {
        case MESSAGE_BUZZER_ALARM_START:
            xQueueSendToFront(alarm_queue_handle, ALARM_QUEUE_MESSAGE_START, portMAX_DELAY);
            break;

        case MESSAGE_BUZZER_ALARM_STOP:
            xQueueSendToFront(alarm_queue_handle, ALARM_QUEUE_MESSAGE_STOP, portMAX_DELAY);
            break;

        case MESSAGE_BUZZER_CARD_VALID:
            xQueueSendToFront(alarm_queue_handle, ALARM_QUEUE_MESSAGE_STOP, portMAX_DELAY);

            gpio_set_level(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(150));
            gpio_set_level(PIN_BUZZER, 0);

            xQueueSendToFront(alarm_queue_handle, ALARM_QUEUE_MESSAGE_START, portMAX_DELAY);
            break;

        case MESSAGE_BUZZER_CARD_INVALID:
            xQueueSendToFront(alarm_queue_handle, ALARM_QUEUE_MESSAGE_STOP, portMAX_DELAY);

            // BEEP

            xQueueSendToFront(alarm_queue_handle, ALARM_QUEUE_MESSAGE_START, portMAX_DELAY);
            break;

        default:
            ESP_LOGE(TAG, "Received invalid message from queue, message num: %d", msg);
            break;
        }
    }
}

static void alarm_task_handler(void *)
{
    for (;;)
    {
        alarm_queue_message_t msg;
        const BaseType_t ret = xQueueReceive(queue_buzzer_handle, &msg, 0);

        if (ret != errQUEUE_EMPTY)
        {
            switch (msg)
            {
            case ALARM_QUEUE_MESSAGE_START:
                gpio_set_level(PIN_BUZZER, 1);
                vTaskDelay(pdMS_TO_TICKS(50));
                gpio_set_level(PIN_BUZZER, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case ALARM_QUEUE_MESSAGE_STOP:
                vTaskDelay(pdMS_TO_TICKS(10));
                break;

            default:
                ESP_LOGE(TAG, "Received invalid message from queue, message num: %d", msg);
                break;
            }
        }
    }
}
