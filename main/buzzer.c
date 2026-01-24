#include "buzzer.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "app_config.h"
#include "queue.h"

static const char *TAG = "buzzer";

#define PIN_BUZZER GPIO_NUM_12

static TaskHandle_t buzzer_task_handle;
static TaskHandle_t alarm_task_handle;

static QueueHandle_t alarm_queue_handle;

typedef enum alarm_queue_message_t
{
    ALARM_QUEUE_MESSAGE_START,
    ALARM_QUEUE_MESSAGE_STOP,
} alarm_queue_message_t;

static const char *alarm_queue_message_to_name(alarm_queue_message_t message)
{
    switch (message)
    {
    case ALARM_QUEUE_MESSAGE_START:
        return "ALARM_QUEUE_MESSAGE_START";
    case ALARM_QUEUE_MESSAGE_STOP:
        return "ALARM_QUEUE_MESSAGE_STOP";
    default:
        ESP_LOGE(TAG, "Received invalid alarm message, enum code %d.", message);
        return "INVALID_MESSAGE_TYPE";
    }
}

/**
 * @brief Task handler for buzzer control.
 * Processes messages from the buzzer queue and controls the alarm.
 *
 * @param pvParameters Unused.
 */
static void buzzer_task_handler(void *)
{
    bool alarm_running = false;
    for (;;)
    {
        message_t incoming_message;
        xQueueReceive(queue_handle_buzzer, &incoming_message, portMAX_DELAY);

        alarm_queue_message_t outgoing_message;
        switch (incoming_message.type)
        {
        case MESSAGE_TYPE_BUZZER_ALARM_START:
            outgoing_message = ALARM_QUEUE_MESSAGE_START;
            xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);
            alarm_running = true;
            break;

        case MESSAGE_TYPE_BUZZER_ALARM_STOP:
            outgoing_message = ALARM_QUEUE_MESSAGE_STOP;
            xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);
            alarm_running = false;
            break;

        case MESSAGE_TYPE_BUZZER_CARD_VALID:
            if (alarm_running)
            {
                outgoing_message = ALARM_QUEUE_MESSAGE_STOP;
                xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);
            }

            gpio_set_level(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(150));
            gpio_set_level(PIN_BUZZER, 0);

            if (alarm_running)
            {
                outgoing_message = ALARM_QUEUE_MESSAGE_START;
                xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);
            }
            break;

        case MESSAGE_TYPE_BUZZER_CARD_INVALID:
            if (alarm_running)
            {
                outgoing_message = ALARM_QUEUE_MESSAGE_STOP;
                xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);
            }

            gpio_set_level(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(PIN_BUZZER, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(PIN_BUZZER, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(PIN_BUZZER, 0);
            vTaskDelay(pdMS_TO_TICKS(50));

            if (alarm_running)
            {
                outgoing_message = ALARM_QUEUE_MESSAGE_START;
                xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);
            }
            break;

        default:
            ESP_LOGE(TAG, "Received invalid message from buzzer queue, message num: %d", outgoing_message);
            break;
        }
    }
}

/**
 * @brief Task handler for alarm sound generation.
 * Generates the alarm beep pattern when enabled.
 *
 * @param pvParameters Unused.
 */
static void alarm_task_handler(void *)
{
    bool enabled = false;
    for (;;)
    {
        alarm_queue_message_t message;
        const BaseType_t ret = xQueueReceive(alarm_queue_handle, &message, 0);

        if (ret == pdTRUE)
        {
            switch (message)
            {
            case ALARM_QUEUE_MESSAGE_START:
                enabled = true;
                break;

            case ALARM_QUEUE_MESSAGE_STOP:
                enabled = false;
                if (ret == pdTRUE)
                {
                    gpio_set_level(PIN_BUZZER, 0);
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                break;

            default:
                if (ret == pdTRUE)
                {
                    ESP_LOGE(TAG, "Received invalid message from alarm queue, message num: %d", message);
                }
                break;
            }
        }

        if (enabled)
        {
            gpio_set_level(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(PIN_BUZZER, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

esp_err_t buzzer_init(void)
{
    esp_err_t esp_ret;
    BaseType_t rtos_ret;
    esp_err_t cleanup_ret;

    ESP_LOGI(TAG, "Configuring GPIO...");
    const gpio_config_t config = {
        .pin_bit_mask = 1 << PIN_BUZZER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_ret = gpio_config(&config);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure gpio: %s", esp_err_to_name(esp_ret));
        goto cleanup_gpio;
    }

    ESP_LOGI(TAG, "Setting initial GPIO level to low...");
    esp_ret = gpio_set_level(PIN_BUZZER, 0);
    if (esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set gpio initial state: %s", esp_err_to_name(esp_ret));
        goto cleanup_gpio;
    }

    ESP_LOGI(TAG, "Creating alarm queue...");
    alarm_queue_handle = xQueueCreate(APP_CONFIG_QUEUE_SIZE_ITEMS, sizeof(alarm_queue_message_t));
    if (alarm_queue_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize alarm queue");
        goto cleanup_gpio;
    }

    ESP_LOGI(TAG, "Creating buzzer task...");
    rtos_ret = xTaskCreate(buzzer_task_handler, "Buzzer", APP_CONFIG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &buzzer_task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create buzzer task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_alarm_queue;
    }

    ESP_LOGI(TAG, "Creating alarm task...");
    rtos_ret = xTaskCreate(alarm_task_handler, "Buzzer Alarm", APP_CONFIG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &alarm_task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create alarm task with error code: %d", rtos_ret);
        goto cleanup_buzzer_task;
    }

    return ESP_OK;

cleanup_buzzer_task:
    ESP_LOGI(TAG, "Deleting buzzer task...");
    vTaskDelete(buzzer_task_handle);
    buzzer_task_handle = NULL;
cleanup_alarm_queue:
    ESP_LOGI(TAG, "Deleting alarm queue...");
    vQueueDelete(alarm_queue_handle);
    alarm_queue_handle = NULL;
cleanup_gpio:
    ESP_LOGI(TAG, "Resetting GPIO pin...");
    cleanup_ret = gpio_reset_pin(PIN_BUZZER);
    if (cleanup_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to reset the gpio pin: %s. aborting program", esp_err_to_name(cleanup_ret));
        abort();
    }

    return esp_ret;
}

esp_err_t buzzer_deinit(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Deleting alarm task...");
    vTaskDelete(alarm_task_handle);
    alarm_task_handle = NULL;

    ESP_LOGI(TAG, "Deleting buzzer task...");
    vTaskDelete(buzzer_task_handle);
    buzzer_task_handle = NULL;

    ESP_LOGI(TAG, "Deleting alarm queue...");
    vQueueDelete(alarm_queue_handle);
    alarm_queue_handle = NULL;

    ESP_LOGI(TAG, "Resetting GPIO pin...");
    ret = gpio_reset_pin(PIN_BUZZER);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to reset gpio pin: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
