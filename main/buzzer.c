#include "buzzer.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "app_config.h"
#include "queue.h"

#define PIN_BUZZER GPIO_NUM_12

#define ALARM_QUEUE_SIZE_ITEMS 16

static const char *TAG = "buzzer";

static TaskHandle_t buzzer_task_handle;
static TaskHandle_t alarm_task_handle;

static QueueHandle_t alarm_queue_handle;

typedef enum
{
    ALARM_QUEUE_MESSAGE_START,
    ALARM_QUEUE_MESSAGE_STOP,
} alarm_queue_message_t;

static void buzzer_task_handler(void *)
{
    for (;;)
    {
        message_t incoming_message;
        xQueueReceive(queue_buzzer_handle, &incoming_message, portMAX_DELAY);
        ESP_LOGD(TAG, "Received message with enum number: %d", incoming_message);

        alarm_queue_message_t outgoing_message;
        switch (incoming_message)
        {
        case MESSAGE_BUZZER_ALARM_START:
            outgoing_message = ALARM_QUEUE_MESSAGE_START;
            xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);
            break;

        case MESSAGE_BUZZER_ALARM_STOP:
            outgoing_message = ALARM_QUEUE_MESSAGE_STOP;
            xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);
            break;

        case MESSAGE_BUZZER_CARD_VALID:
            outgoing_message = ALARM_QUEUE_MESSAGE_STOP;
            xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);

            gpio_set_level(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(150));
            gpio_set_level(PIN_BUZZER, 0);

            outgoing_message = ALARM_QUEUE_MESSAGE_START;
            xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);
            break;

        case MESSAGE_BUZZER_CARD_INVALID:
            outgoing_message = ALARM_QUEUE_MESSAGE_STOP;
            xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);

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

            outgoing_message = ALARM_QUEUE_MESSAGE_START;
            xQueueSendToBack(alarm_queue_handle, &outgoing_message, portMAX_DELAY);
            break;

        default:
            ESP_LOGE(TAG, "Received invalid message from queue, message num: %d", outgoing_message);
            break;
        }
    }
}

static void alarm_task_handler(void *)
{
    alarm_queue_message_t msg;
    for (;;)
    {

        const BaseType_t ret = xQueueReceive(alarm_queue_handle, &msg, 0);

        switch (msg)
        {
        case ALARM_QUEUE_MESSAGE_START:
            gpio_set_level(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(PIN_BUZZER, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        case ALARM_QUEUE_MESSAGE_STOP:
            if (ret == pdTRUE)
            {
                gpio_set_level(PIN_BUZZER, 0);
                vTaskDelay(pdMS_TO_TICKS(50));
                ESP_LOGI(TAG, "Alarm stopped");
            }

            break;

        default:
            if (ret == pdTRUE)
            {
                ESP_LOGE(TAG, "Received invalid message from queue, message num: %d", msg);
            }
            break;
        }
    }
}

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

    BaseType_t rtos_ret = xTaskCreate(buzzer_task_handler, "Buzzer", APP_CONFIG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &buzzer_task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create buzzer task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_gpio;
    }

    rtos_ret = xTaskCreate(alarm_task_handler, "Buzzer Alarm", APP_CONFIG_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &alarm_task_handle);
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create alarm task with error code: %d", rtos_ret);
        goto cleanup_buzzer_task;
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
    alarm_task_handle = NULL;
cleanup_buzzer_task:
    vTaskDelete(buzzer_task_handle);
    buzzer_task_handle = NULL;
cleanup_gpio:
    esp_err_t cleanup_esp_ret = gpio_reset_pin(PIN_BUZZER);
    if (cleanup_esp_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to reset the gpio pin: %s. aborting program", esp_err_to_name(cleanup_esp_ret));
        abort();
    }

    return esp_ret;
}

esp_err_t buzzer_deinit(void)
{
    vTaskDelete(alarm_task_handle);
    alarm_task_handle = NULL;

    vTaskDelete(buzzer_task_handle);
    buzzer_task_handle = NULL;

    esp_err_t ret = gpio_reset_pin(PIN_BUZZER);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to reset gpio pin: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
