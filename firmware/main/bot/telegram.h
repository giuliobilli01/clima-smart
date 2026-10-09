#pragma once
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Size in bytes of the text of a received message and of the data of
 *        a button, NUL included; longer text is cut.
 */
#define TELEGRAM_TEXT_SIZE 64

/** @brief Size of the identifier of a button press, NUL included. */
#define TELEGRAM_CALLBACK_QUERY_ID_SIZE 64

/** @brief Most buttons under one message. */
#define TELEGRAM_MAX_BUTTONS 8

/** @brief Something a user sent to the bot: a typed message, or the press of a button under a message of the bot. */
typedef struct {
    long long chat_id;                                       /**< Chat to answer. */
    char text[TELEGRAM_TEXT_SIZE];                           /**< Typed text, or the data of the pressed button. */
    int message_id;                                          /**< Message that holds the pressed button. */
    char callback_query_id[TELEGRAM_CALLBACK_QUERY_ID_SIZE]; /**< ID of the button press, empty for a typed message. */
} telegram_message_t;

/** @brief A button under a message. */
typedef struct {
    const char *label;                      /**< Text on the button: a string constant. */
    char callback_data[TELEGRAM_TEXT_SIZE]; /**< Text that comes back to the bot when the button is pressed. */
} telegram_button_t;

/** @brief The buttons under a message, laid out in rows; {0} is an empty keyboard. */
typedef struct {
    telegram_button_t buttons[TELEGRAM_MAX_BUTTONS]; /**< Buttons, row after row. */
    int button_count;                                /**< Buttons added so far. */
    int buttons_per_row;                             /**< Buttons in a full row; 0 puts all of them on one row. */
} telegram_keyboard_t;

/** @brief An entry of the bot's menu, the list shown when typing "/" in the chat. */
typedef struct {
    const char *name;        /**< Name without the slash: 1 to 32 lowercase letters, digits and underscores. */
    const char *description; /**< Text next to the name: 1 to 256 characters. */
} telegram_command_t;

/**
 * @brief Creates the HTTPS client that every request to Telegram goes through.
 */
void telegram_init(void);

/**
 * @brief Waits for the next text message or button press, from any chat.
 *
 * Uses long polling: Telegram answers as soon as something arrives, or after
 * 30 s. The first call forgets what was sent while the board was off. On
 * errors, logs Telegram's answer and waits 5 s before returning.
 *
 * @param message Filled when a message or a button press arrives.
 * @return true if a text message or a button press arrived; false after
 *         30 s, at the first call, on errors and for any other update.
 */
bool telegram_wait_for_message(telegram_message_t *message);

/**
 * @brief Sends a new message to a chat.
 *
 * On errors, logs Telegram's answer and waits 5 s.
 *
 * @param chat_id Chat to write to.
 * @param text Text of the message, at most 4096 characters.
 * @param keyboard Buttons under the message, or NULL for none.
 */
void telegram_send_message(long long chat_id, const char *text, const telegram_keyboard_t *keyboard);

/**
 * @brief Answers a message with text and buttons.
 *
 * Answers a typed message with a new message. For a button press, confirms
 * the press to Telegram and rewrites the message that holds the button with
 * the new text and buttons. On errors, logs Telegram's answer and waits 5 s.
 *
 * @param message Message or button press to answer.
 * @param text Text of the answer, at most 4096 characters.
 * @param keyboard Buttons under the answer, or NULL for none.
 */
void telegram_reply(const telegram_message_t *message, const char *text, const telegram_keyboard_t *keyboard);

/**
 * @brief Adds a button after the last one of a keyboard, up to TELEGRAM_MAX_BUTTONS.
 *
 * @param keyboard Keyboard to extend.
 * @param label Text on the button: a string constant.
 * @param callback_data Text that comes back to the bot when the button is pressed, at most 63 characters.
 */
void telegram_keyboard_add_button(telegram_keyboard_t *keyboard, const char *label, const char *callback_data);

/**
 * @brief Replaces the bot's menu, the list of commands shown when typing "/".
 *
 * On errors, logs Telegram's answer, waits 5 s, and the previous menu stays.
 *
 * @param commands Entries of the menu, in order.
 * @param command_count Number of entries.
 */
void telegram_set_menu(const telegram_command_t *commands, size_t command_count);
