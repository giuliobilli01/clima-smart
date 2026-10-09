#include "bot.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include "bot_commands.h"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "log.h"
#include "network/wifi.h"
#include "telegram.h"

/** @brief The list of allowed chats from menuconfig, between brackets: a JSON array. */
#define ALLOWED_CHAT_IDS_JSON "[" CONFIG_CLIMA_TELEGRAM_CHAT_IDS "]"

/** @brief Size of one notification in bytes, NUL included. */
#define NOTIFICATION_SIZE 128

/** @brief Most notifications that can wait to be sent. */
#define NOTIFICATION_QUEUE_LENGTH 8

/** @brief JSON array of the allowed chat IDs, NULL if the list in menuconfig is not valid. */
static cJSON *allowed_chat_ids;

/** @brief Notifications waiting to be sent, each copied into the queue. */
static QueueHandle_t queued_notifications;

void bot_init(void)
{
    allowed_chat_ids = cJSON_Parse(ALLOWED_CHAT_IDS_JSON);
    if (allowed_chat_ids == NULL)
        ESP_LOGE(LOG_TAG, "Telegram chat IDs are not numbers separated by commas: no chat is allowed");

    queued_notifications = xQueueCreate(NOTIFICATION_QUEUE_LENGTH, NOTIFICATION_SIZE);
    telegram_init();
}

void bot_notify(const char *format, ...)
{
    char text[NOTIFICATION_SIZE];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);

    ESP_LOGI(LOG_TAG, "notification: %s", text);
    if (xQueueSend(queued_notifications, text, 0) != pdTRUE)
        ESP_LOGW(LOG_TAG, "too many notifications waiting: this one is dropped");
}

/**
 * @brief Sends every queued notification to every allowed chat, until the queue is empty.
 */
static void send_queued_notifications(void)
{
    char text[NOTIFICATION_SIZE];
    while (xQueueReceive(queued_notifications, text, 0) == pdTRUE) {
        const cJSON *chat_id;
        cJSON_ArrayForEach(chat_id, allowed_chat_ids) {
            telegram_send_message(chat_id->valuedouble, text, NULL);
        }
    }
}

/**
 * @brief Tells whether a chat may command the bot.
 *
 * @param chat_id Chat that sent a message or pressed a button.
 * @return true if chat_id is in the list set in menuconfig.
 */
static bool is_allowed_chat(long long chat_id)
{
    const cJSON *allowed_chat_id;
    cJSON_ArrayForEach(allowed_chat_id, allowed_chat_ids) {
        if (allowed_chat_id->valuedouble == chat_id)
            return true;
    }

    return false;
}

void bot_task(void *argument)
{
    wifi_wait_for_connection();
    bot_commands_set_menu();

    while (1) {
        wifi_wait_for_connection();
        send_queued_notifications();

        telegram_message_t message;
        if (!telegram_wait_for_message(&message))
            continue;

        if (is_allowed_chat(message.chat_id))
            bot_commands_run_and_reply(&message);
        else
            ESP_LOGW(LOG_TAG, "message from chat %lld ignored", message.chat_id);
    }
}
