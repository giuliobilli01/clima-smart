#pragma once
#include "telegram.h"

/**
 * @brief Sets the bot's menu, the list shown when typing "/", from the commands of the bot.
 */
void bot_commands_set_menu(void);

/**
 * @brief Runs the command in a message or button press, and answers it.
 *
 * Finds the command named by the first word of the text, and answers any
 * other text with the list of commands. Runs the command when it has a value:
 * the commands of the air conditioner send the new state by IR, the others
 * change the board only. Answers with the status, or with what went wrong,
 * and the buttons of the command. The answer to a button press rewrites the
 * message that holds the button.
 *
 * @param message Message or button press from an allowed chat.
 */
void bot_commands_run_and_reply(const telegram_message_t *message);
