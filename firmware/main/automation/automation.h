#pragma once
#include <stdbool.h>

/**
 * @brief Room temperature at or below which the thermostat switches the air
 *        conditioner on, heating, until another one is set.
 */
#define THERMOSTAT_HEAT_AT_OR_BELOW_DEFAULT_CELSIUS 17

/**
 * @brief Room temperature at or above which the thermostat switches the air
 *        conditioner on, cooling, until another one is set.
 */
#define THERMOSTAT_COOL_AT_OR_ABOVE_DEFAULT_CELSIUS 27

/**
 * @brief Target temperature set by the thermostat when it switches the air
 *        conditioner on, until another one is set.
 */
#define THERMOSTAT_TARGET_DEFAULT_CELSIUS 23

/** @brief Lowest thermostat threshold accepted, in degrees Celsius. */
#define THERMOSTAT_THRESHOLD_MIN_CELSIUS 10

/** @brief Highest thermostat threshold accepted, in degrees Celsius. */
#define THERMOSTAT_THRESHOLD_MAX_CELSIUS 35

/**
 * @brief Seconds in a row with the door open and the air conditioner on
 *        before it is switched off, until another delay is set.
 */
#define DOOR_DELAY_DEFAULT_SECONDS 60

/** @brief Shortest door delay accepted, in seconds. */
#define DOOR_DELAY_MIN_SECONDS 10

/** @brief Longest door delay accepted, in seconds: ten minutes. */
#define DOOR_DELAY_MAX_SECONDS 600

/** @brief Settings of the thermostat, in whole degrees Celsius. */
typedef struct {
    int heat_at_or_below_celsius; /**< Room temperature at or below which it switches the air conditioner on to heat. */
    int cool_at_or_above_celsius; /**< Room temperature at or above which it switches the air conditioner on to cool. */
    int target_celsius;           /**< Target temperature it sets when it switches the air conditioner on. */
} thermostat_settings_t;

/**
 * @brief Creates the mutex of the settings of the automations, and loads
 *        them from the settings.
 *
 * Without saved values, the automations and the status LED check are off,
 * and the door delay and the thermostat take their default values.
 */
void automation_init(void);

/**
 * @brief Every 2 s applies to the latest readings the first rule that holds
 *        among those that are on, and notifies its action to the allowed
 *        chats. Never returns.
 *
 * While the automations or the status LED check are off, their count of
 * readings starts again from zero.
 *
 * @param argument Unused.
 */
void automation_task(void *argument);

/**
 * @brief Switches the automations on or off, and saves the choice in the settings.
 *
 * @param enabled true to switch them on.
 */
void automation_set_enabled(bool enabled);

/**
 * @brief Tells whether the automations are on.
 *
 * @return true if they are on.
 */
bool automation_is_enabled(void);

/**
 * @brief Switches the status LED check on or off, and saves the choice in the settings.
 *
 * @param enabled true to switch it on.
 */
void automation_set_status_led_check_enabled(bool enabled);

/**
 * @brief Tells whether the status LED check is on.
 *
 * @return true if it is on.
 */
bool automation_is_status_led_check_enabled(void);

/**
 * @brief Sets how long the door must stay open, with the air conditioner on,
 *        before the automations switch it off, and saves it in the settings.
 *
 * @param seconds From DOOR_DELAY_MIN_SECONDS to DOOR_DELAY_MAX_SECONDS.
 * @return true if seconds is in range and has been set.
 */
bool automation_set_door_delay(int seconds);

/**
 * @brief Tells how long the door must stay open, with the air conditioner on,
 *        before the automations switch it off.
 *
 * @return The door delay in seconds: DOOR_DELAY_DEFAULT_SECONDS until it is
 *         changed.
 */
int automation_get_door_delay(void);

/**
 * @brief Sets the thresholds and the target temperature of the thermostat, and saves them in the settings.
 *
 * @param thermostat New settings: the thresholds from
 *        THERMOSTAT_THRESHOLD_MIN_CELSIUS to THERMOSTAT_THRESHOLD_MAX_CELSIUS,
 *        the heating one below the cooling one; the target from
 *        AC_MIN_TARGET_TEMPERATURE_CELSIUS to AC_MAX_TARGET_TEMPERATURE_CELSIUS.
 * @return true if the settings are in range and have been set.
 */
bool automation_set_thermostat(const thermostat_settings_t *thermostat);

/**
 * @brief Reads the thresholds and the target temperature of the thermostat.
 *
 * @param thermostat Filled with the settings: the default ones until they
 *        are changed.
 */
void automation_get_thermostat(thermostat_settings_t *thermostat);
