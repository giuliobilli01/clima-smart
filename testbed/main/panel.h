#pragma once
#include "ir.h"

/**
 * @brief Prepares the display, the power LED and the buzzer, and writes the
 *        name of the test-bed on the display.
 *
 * The power LED starts off and the buzzer silent.
 */
void panel_init(void);

/**
 * @brief Shows a new state on the panel.
 *
 * Writes power, mode and target temperature on the first line of the
 * display and the fan speed on the second, switches the power LED on or off
 * and beeps once. Waits about a tenth of a second, while the buzzer sounds.
 *
 * @param state State to show.
 */
void panel_show(const received_ac_state_t *state);
