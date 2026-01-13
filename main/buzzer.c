#include "buzzer.h"

#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

#include "queue.h"

#define PIN_BUZZER GPIO_NUM_12

static const char *TAG = "BUZZER";

static TaskHandle_t buzzer_task_handle;
static TaskHandle_t alarm_task_handle;

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
        ESP_LOGE(TAG, "Failed to create task with error code: %d", rtos_ret);
        esp_ret = ESP_FAIL;
        goto cleanup_gpio;
    }

    rtos_ret = alarm_task_init();
    if (rtos_ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to initialize alarm task with error code: %d", rtos_ret);
        goto cleanup_buzzer_task;
    }

    return ESP_OK;

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
        queue_task_message_t msg;
        const BaseType_t ret = xQueueReceive(queue_buzzer_handle, &msg, portMAX_DELAY);
        if (ret != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to receive message from queue with error code: %d, suspending task.", ret);
            vTaskSuspend(NULL); // suspend this task
            continue;
        }

        switch (msg)
        {
        case QUEUE_MESSAGE_BUZZER_ALARM_START:
            vTaskResume(alarm_task_handle);
            break;

        case QUEUE_MESSAGE_BUZZER_ALARM_STOP:
            vTaskSuspend(alarm_task_handle);
            break;

        case QUEUE_MESSAGE_BUZZER_BEEP_CARD_VALID:
            bool alarm_running = eTaskGetState(alarm_task_handle) != eSuspended;
            if (alarm_running)
                vTaskSuspend(alarm_task_handle);

            // BEEP

            if (alarm_running)
                vTaskResume(alarm_task_handle);
            break;

        case QUEUE_MESSAGE_BUZZER_BEEP_CARD_INVALID:
            bool alarm_running = eTaskGetState(alarm_task_handle) != eSuspended;
            if (alarm_running)
                vTaskSuspend(alarm_task_handle);

            // BEEP

            if (alarm_running)
                vTaskResume(alarm_task_handle);
            break;

        default:
            ESP_LOGE(TAG, "Received invalid message from queue, message num: %d", msg);
            break;
        }
    }
}

static BaseType_t alarm_task_init(void)
{
    const BaseType_t ret = xTaskCreate(alarm_task_handler, "Buzzer Alarm", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &alarm_task_handle);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create task with error code: %d", ret);
        return ret;
    }

    vTaskSuspend(alarm_task_handle);

    return pdPASS;
}

static void alarm_task_handler(void *)
{
    // make alarm sounds
}
