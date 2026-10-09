#include "telegram.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "log.h"

/** @brief Start of every Bot API address: the server, then "bot" and the token. */
#define API_URL "https://api.telegram.org/bot" CONFIG_CLIMA_TELEGRAM_BOT_TOKEN

/** @brief How long Telegram keeps a getUpdates request waiting when no message arrives, in seconds. */
#define POLL_TIMEOUT_SECONDS 30

/** @brief How long a request may last before it is abandoned: the long poll plus 10 s. */
#define REQUEST_TIMEOUT_MS ((POLL_TIMEOUT_SECONDS + 10) * 1000)

/** @brief Pause after a failed request. */
#define RETRY_DELAY_MS 5000

/** @brief The HTTPS client of every request, with its open connection. */
static esp_http_client_handle_t bot_api_client;

/** @brief true once the updates sent while the board was off have been forgotten. */
static bool old_updates_forgotten;

/** @brief ID of the first update not handled yet, 0 until the first update arrives. */
static int next_update_id;

void telegram_init(void)
{
    esp_http_client_config_t config = {
        .url = API_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = REQUEST_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    bot_api_client = esp_http_client_init(&config);
    esp_http_client_set_header(bot_api_client, "Content-Type", "application/json");
}

/**
 * @brief Calls one method of the Bot API and reads Telegram's answer.
 *
 * Uses the connection left open by the previous call, or opens a new one and
 * checks the server against the certificates in the firmware. Reads the whole
 * answer, and closes the connection when the answer is cut or Telegram asks
 * to close it. On failure logs the status and Telegram's description of the
 * error, then waits RETRY_DELAY_MS.
 *
 * @param url Address of the method, such as API_URL "/getUpdates".
 * @param parameters Parameters of the method.
 * @return The answer, to be freed with cJSON_Delete(), or NULL if Telegram
 *         did not answer 200 OK with JSON.
 */
static cJSON *call_bot_api(const char *url, const cJSON *parameters)
{
    esp_http_client_set_url(bot_api_client, url);

    char *body = cJSON_PrintUnformatted(parameters);
    int body_length = strlen(body);
    int64_t answer_length = -1;
    if (esp_http_client_open(bot_api_client, body_length) == ESP_OK &&
        esp_http_client_write(bot_api_client, body, body_length) == body_length)
        answer_length = esp_http_client_fetch_headers(bot_api_client);
    cJSON_free(body);

    cJSON *answer = NULL;
    int read_length = 0;
    char *answer_text = answer_length > 0 ? malloc(answer_length) : NULL;
    if (answer_text != NULL) {
        read_length = esp_http_client_read_response(bot_api_client, answer_text, answer_length);
        answer = cJSON_ParseWithLength(answer_text, read_length);
        free(answer_text);
    }

    int status = esp_http_client_get_status_code(bot_api_client);
    bool answer_read_whole = answer_length > 0 && read_length == answer_length;
    if (!answer_read_whole || !esp_http_client_is_persistent_connection(bot_api_client))
        esp_http_client_close(bot_api_client);

    if (status == 200 && answer != NULL)
        return answer;

    const char *description = cJSON_GetStringValue(cJSON_GetObjectItem(answer, "description"));
    ESP_LOGW(LOG_TAG, "Telegram request failed, status %d: %s", status, description ? description : "no answer");
    cJSON_Delete(answer);
    vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
    return NULL;
}

/**
 * @brief Reads one update: a text message, or the press of a button under a message of the bot.
 *
 * @param update Update from getUpdates.
 * @param message Filled when the update is a text message or a button press.
 * @return true if message was filled, false for any other update.
 */
static bool read_update(const cJSON *update, telegram_message_t *message)
{
    const cJSON *button_press = cJSON_GetObjectItem(update, "callback_query");
    const cJSON *chat_message;
    const char *text;
    if (button_press != NULL) {
        chat_message = cJSON_GetObjectItem(button_press, "message");
        text = cJSON_GetStringValue(cJSON_GetObjectItem(button_press, "data"));
    } else {
        chat_message = cJSON_GetObjectItem(update, "message");
        text = cJSON_GetStringValue(cJSON_GetObjectItem(chat_message, "text"));
    }

    const cJSON *chat_id = cJSON_GetObjectItem(cJSON_GetObjectItem(chat_message, "chat"), "id");
    const cJSON *message_id = cJSON_GetObjectItem(chat_message, "message_id");
    const char *callback_query_id = cJSON_GetStringValue(cJSON_GetObjectItem(button_press, "id"));
    if (!cJSON_IsNumber(chat_id) || !cJSON_IsNumber(message_id) || text == NULL)
        return false;

    message->chat_id = chat_id->valuedouble;
    message->message_id = message_id->valueint;
    strlcpy(message->text, text, sizeof(message->text));
    strlcpy(message->callback_query_id, callback_query_id ? callback_query_id : "", sizeof(message->callback_query_id));
    return true;
}

bool telegram_wait_for_message(telegram_message_t *message)
{
    cJSON *parameters = cJSON_CreateObject();
    cJSON_AddNumberToObject(parameters, "offset", old_updates_forgotten ? next_update_id : -1);
    cJSON_AddNumberToObject(parameters, "limit", 1);
    cJSON_AddNumberToObject(parameters, "timeout", old_updates_forgotten ? POLL_TIMEOUT_SECONDS : 0);

    cJSON *answer = call_bot_api(API_URL "/getUpdates", parameters);
    cJSON_Delete(parameters);
    if (answer == NULL)
        return false;

    cJSON *update = cJSON_GetArrayItem(cJSON_GetObjectItem(answer, "result"), 0);
    cJSON *update_id = cJSON_GetObjectItem(update, "update_id");
    if (cJSON_IsNumber(update_id))
        next_update_id = update_id->valueint + 1;

    bool received = old_updates_forgotten && read_update(update, message);
    old_updates_forgotten = true;

    cJSON_Delete(answer);
    return received;
}

/**
 * @brief Writes a keyboard as the Bot API wants it: an object whose
 *        "inline_keyboard" is an array of rows, each an array of buttons.
 *
 * @param keyboard Buttons to write.
 * @return The object, to be freed with cJSON_Delete() or with the parameters it is added to.
 */
static cJSON *create_reply_markup(const telegram_keyboard_t *keyboard)
{
    cJSON *reply_markup = cJSON_CreateObject();
    cJSON *rows = cJSON_AddArrayToObject(reply_markup, "inline_keyboard");
    cJSON *row = NULL;
    for (int index = 0; index < keyboard->button_count; index++) {
        if (row == NULL || (keyboard->buttons_per_row > 0 && index % keyboard->buttons_per_row == 0)) {
            row = cJSON_CreateArray();
            cJSON_AddItemToArray(rows, row);
        }

        cJSON *button = cJSON_CreateObject();
        cJSON_AddStringToObject(button, "text", keyboard->buttons[index].label);
        cJSON_AddStringToObject(button, "callback_data", keyboard->buttons[index].callback_data);
        cJSON_AddItemToArray(row, button);
    }

    return reply_markup;
}

void telegram_send_message(long long chat_id, const char *text, const telegram_keyboard_t *keyboard)
{
    cJSON *parameters = cJSON_CreateObject();
    cJSON_AddNumberToObject(parameters, "chat_id", chat_id);
    cJSON_AddStringToObject(parameters, "text", text);
    if (keyboard != NULL)
        cJSON_AddItemToObject(parameters, "reply_markup", create_reply_markup(keyboard));

    cJSON_Delete(call_bot_api(API_URL "/sendMessage", parameters));
    cJSON_Delete(parameters);
}

/**
 * @brief Confirms a button press to Telegram, then rewrites the message that
 *        holds the button.
 *
 * @param button_press Button press to answer.
 * @param text New text of the message.
 * @param keyboard New buttons under the message, or NULL for none.
 */
static void rewrite_message_of_button(const telegram_message_t *button_press, const char *text,
                                      const telegram_keyboard_t *keyboard)
{
    cJSON *acknowledgement = cJSON_CreateObject();
    cJSON_AddStringToObject(acknowledgement, "callback_query_id", button_press->callback_query_id);

    cJSON_Delete(call_bot_api(API_URL "/answerCallbackQuery", acknowledgement));
    cJSON_Delete(acknowledgement);

    cJSON *parameters = cJSON_CreateObject();
    cJSON_AddNumberToObject(parameters, "chat_id", button_press->chat_id);
    cJSON_AddNumberToObject(parameters, "message_id", button_press->message_id);
    cJSON_AddStringToObject(parameters, "text", text);
    if (keyboard != NULL)
        cJSON_AddItemToObject(parameters, "reply_markup", create_reply_markup(keyboard));

    cJSON_Delete(call_bot_api(API_URL "/editMessageText", parameters));
    cJSON_Delete(parameters);
}

void telegram_reply(const telegram_message_t *message, const char *text, const telegram_keyboard_t *keyboard)
{
    if (message->callback_query_id[0] != '\0')
        rewrite_message_of_button(message, text, keyboard);
    else
        telegram_send_message(message->chat_id, text, keyboard);
}

void telegram_keyboard_add_button(telegram_keyboard_t *keyboard, const char *label, const char *callback_data)
{
    if (keyboard->button_count == TELEGRAM_MAX_BUTTONS)
        return;

    telegram_button_t *button = &keyboard->buttons[keyboard->button_count++];
    button->label = label;
    strlcpy(button->callback_data, callback_data, sizeof(button->callback_data));
}

void telegram_set_menu(const telegram_command_t *commands, size_t command_count)
{
    cJSON *parameters = cJSON_CreateObject();
    cJSON *menu = cJSON_AddArrayToObject(parameters, "commands");
    for (size_t index = 0; index < command_count; index++) {
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "command", commands[index].name);
        cJSON_AddStringToObject(entry, "description", commands[index].description);
        cJSON_AddItemToArray(menu, entry);
    }

    cJSON_Delete(call_bot_api(API_URL "/setMyCommands", parameters));
    cJSON_Delete(parameters);
}
