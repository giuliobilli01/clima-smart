#pragma once
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Operating mode of the air conditioner, with the same values as the IR library's stdAc::opmode_t. */
typedef enum {
    AC_MODE_OFF = -1,
    AC_MODE_AUTO = 0,
    AC_MODE_COOL = 1,
    AC_MODE_HEAT = 2,
    AC_MODE_DRY = 3,
    AC_MODE_FAN = 4,
} ac_mode_t;

/** @brief Fan speed of the air conditioner, with the same values as the IR library's stdAc::fanspeed_t. */
typedef enum {
    AC_FAN_AUTO = 0,
    AC_FAN_MIN = 1,
    AC_FAN_LOW = 2,
    AC_FAN_MEDIUM = 3,
    AC_FAN_HIGH = 4,
    AC_FAN_MAX = 5,
    AC_FAN_MEDIUM_HIGH = 6,
} ac_fan_t;

/** @brief Lowest target temperature accepted in a command. */
#define AC_MIN_TARGET_TEMPERATURE_CELSIUS 16

/** @brief Highest target temperature accepted in a command. */
#define AC_MAX_TARGET_TEMPERATURE_CELSIUS 30

/** @brief Everything we control on the air conditioner. */
typedef struct {
    bool power_on;
    ac_mode_t mode;
    float target_temperature_celsius;
    ac_fan_t fan;
} ac_state_t;

/**
 * @brief Prepares the IR LED, the receiver and the believed state.
 *
 * Creates the mutex and the two signals, sets up the 38 kHz blinking of the
 * IR LED and starts the receiver. Loads from the settings the learned
 * protocol and the believed state; without a saved state, the air conditioner
 * is believed off, in cooling mode. Every command keeps the display light on.
 */
void ir_init(void);

/**
 * @brief Sends the state set by ir_ac_send(), each time it asks. Never returns.
 *
 * Pauses the receiver while sending.
 *
 * @param argument Unused.
 */
void ir_transmit_task(void *argument);

/**
 * @brief Sends the whole desired state to the air conditioner, with the learned protocol.
 *
 * Waits for ir_transmit_task() to send it, then saves the new believed state
 * in the settings.
 *
 * @param desired State the air conditioner should be in after the command.
 * @return true if sent; false if no protocol has been learned yet, or the
 *         learned one cannot be sent this way.
 */
bool ir_ac_send(const ac_state_t *desired);

/**
 * @brief Reads the state the air conditioner is believed to be in.
 *
 * It is the last state the board sent, or heard from the original remote.
 *
 * @param believed_state Filled with the believed state.
 */
void ir_ac_get_believed_state(ac_state_t *believed_state);

/**
 * @brief Sets the believed power, on the board only.
 *
 * Mode, temperature and fan keep their believed values. Saves the new
 * believed state in the settings.
 *
 * @param power_on Power the air conditioner is really in.
 */
void ir_ac_set_believed_power(bool power_on);

/**
 * @brief Returns at once the IR message received last, if any, described in one line.
 *
 * When the message comes from an air conditioner remote, updates the believed
 * state with its settings, protocol and model, and saves them in the settings.
 *
 * @param description Filled with protocol name, number of bits, raw code in
 *        hexadecimal and, for air conditioners, the decoded settings.
 * @param description_size Size of description in bytes; longer text is cut.
 * @param ac_state_changed Set to true if the message updated the believed state.
 * @return true if a message was ready, false otherwise.
 */
bool ir_receive_message(char *description, size_t description_size, bool *ac_state_changed);

#ifdef __cplusplus
}
#endif
