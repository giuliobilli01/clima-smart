#pragma once

/**
 * @brief Reads the allowed chat IDs set in menuconfig, and creates the queue
 *        of the notifications and the Telegram client.
 *
 * If the list of chat IDs is not numbers separated by commas, logs an error
 * and no chat is allowed.
 */
void bot_init(void);

/**
 * @brief Queues a notification for all the allowed chats, and logs it.
 *
 * Returns at once; the notification is sent within 30 s while the board is
 * connected to the WiFi, otherwise as soon as it connects. If 8
 * notifications are already waiting, drops this one and logs a warning.
 *
 * @param format Text of the notification, with printf placeholders: at most
 *        127 characters once filled, the rest is cut.
 */
void bot_notify(const char *format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief Sets the bot's menu, then, over and over: sends the queued
 *        notifications, waits for a message or a button press, and runs and
 *        answers the command it holds. Never returns.
 *
 * Waits for the WiFi connection before the menu and before every round.
 * Answers only the chats listed in menuconfig, and logs the chat ID of the
 * others.
 *
 * @param argument Unused.
 */
void bot_task(void *argument);
