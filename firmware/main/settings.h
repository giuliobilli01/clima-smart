#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "automation/automation.h"
#include "ir/ir.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Prepares the flash memory area that keeps the settings.
 *
 * If the area is full or was written by a different ESP-IDF version, erases
 * it with all the saved settings.
 */
void settings_init(void);

/**
 * @brief Reads the air conditioner protocol learned from its remote.
 *
 * @param protocol Filled with the IR library's decode_type_t value.
 * @param model Filled with the IR library's model number for that protocol.
 * @return true if a protocol was saved earlier, false if none has been
 *         learned yet; in that case protocol and model are left unchanged.
 */
bool settings_load_ac_protocol(int32_t *protocol, int16_t *model);

/**
 * @brief Saves the air conditioner protocol learned from its remote.
 *
 * On failure logs an error.
 *
 * @param protocol The IR library's decode_type_t value.
 * @param model The IR library's model number for that protocol.
 */
void settings_save_ac_protocol(int32_t protocol, int16_t model);

/**
 * @brief Reads the state the air conditioner was believed to be in before the reboot.
 *
 * @param believed_state Filled with the saved believed state.
 * @return true if a state was saved earlier, false otherwise; in that case
 *         believed_state is left unchanged.
 */
bool settings_load_ac_state(ac_state_t *believed_state);

/**
 * @brief Saves the state the air conditioner is believed to be in.
 *
 * On failure logs an error.
 *
 * @param believed_state Believed state, with a whole target temperature.
 */
void settings_save_ac_state(const ac_state_t *believed_state);

/**
 * @brief Reads whether the automations were on before the reboot.
 *
 * @return true if they were on, false if they were off or have never been
 *         switched on.
 */
bool settings_load_automations_enabled(void);

/**
 * @brief Saves whether the automations are on.
 *
 * On failure logs an error.
 *
 * @param enabled true if the automations are on.
 */
void settings_save_automations_enabled(bool enabled);

/**
 * @brief Reads how long the door must stay open before the automations
 *        switch the air conditioner off.
 *
 * @param seconds Filled with the saved delay.
 * @return true if a delay was saved earlier, false otherwise; in that case
 *         seconds is left unchanged.
 */
bool settings_load_door_delay(int *seconds);

/**
 * @brief Saves how long the door must stay open before the automations
 *        switch the air conditioner off.
 *
 * On failure logs an error.
 *
 * @param seconds Delay in seconds.
 */
void settings_save_door_delay(int seconds);

/**
 * @brief Reads whether the status LED check was on before the reboot.
 *
 * @return true if it was on, false if it was off or has never been switched
 *         on.
 */
bool settings_load_status_led_check_enabled(void);

/**
 * @brief Saves whether the status LED check is on.
 *
 * On failure logs an error.
 *
 * @param enabled true if the status LED check is on.
 */
void settings_save_status_led_check_enabled(bool enabled);

/**
 * @brief Reads the thresholds and the target temperature of the thermostat.
 *
 * @param thermostat Filled with the saved settings.
 * @return true if they were saved earlier, false otherwise; in that case
 *         thermostat is left unchanged.
 */
bool settings_load_thermostat(thermostat_settings_t *thermostat);

/**
 * @brief Saves the thresholds and the target temperature of the thermostat.
 *
 * On failure logs an error.
 *
 * @param thermostat Settings to save.
 */
void settings_save_thermostat(const thermostat_settings_t *thermostat);

#ifdef __cplusplus
}
#endif
