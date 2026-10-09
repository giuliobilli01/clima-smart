#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Size in bytes of the name of a mode or fan speed, NUL included. */
#define AC_SETTING_NAME_SIZE 12

/** @brief State of the fake air conditioner, as the display shows it: whole degrees, names of mode and fan speed. */
typedef struct {
    bool power_on;
    int target_temperature_celsius;
    char mode_name[AC_SETTING_NAME_SIZE];
    char fan_name[AC_SETTING_NAME_SIZE];
} received_ac_state_t;

/**
 * @brief Starts the IR receiver, with the fake air conditioner off.
 */
void ir_init(void);

/**
 * @brief Returns at once whether an air conditioner command has arrived, and
 *        applies it to the fake air conditioner.
 *
 * Logs every complete IR message: an air conditioner command with all its
 * settings, any other message with its protocol and number of bits.
 *
 * @param state Filled with the new state of the fake air conditioner when
 *        the function returns true.
 * @return true if an air conditioner command was received and applied.
 */
bool ir_receive_ac_state(received_ac_state_t *state);

#ifdef __cplusplus
}
#endif
