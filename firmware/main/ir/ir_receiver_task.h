#pragma once

/**
 * @brief Checks the IR receiver every 10 ms and logs each message; when it
 *        comes from the air conditioner remote, also logs the state it has
 *        set. Never returns.
 *
 * @param argument Unused.
 */
void ir_receiver_task(void *argument);
