#pragma once
#include <stdbool.h>
#include "ir/ir.h"

/** @brief How often the automations look at the readings. */
#define AUTOMATION_PERIOD_MS 2000

/** @brief Light level at or above which the status LED of the air conditioner counts as lit. */
#define STATUS_LED_LIT_LIGHT_LEVEL 30

/**
 * @brief Seconds in a row the status LED must contradict the believed power
 *        before the board believes the LED.
 */
#define STATUS_LED_DISAGREEMENT_SECONDS 10

/**
 * @brief Tells in which mode the thermostat switches the air conditioner on.
 *
 * @param believed_power_on true if the air conditioner is believed on.
 * @param room_temperature_celsius Latest room temperature.
 * @param door_open Latest state of the door.
 * @param light_level Latest light level on the status LED.
 * @param heat_at_or_below_celsius Room temperature at or below which it switches on, heating.
 * @param cool_at_or_above_celsius Room temperature at or above which it switches on, cooling.
 * @return AC_MODE_HEAT at or below heat_at_or_below_celsius,
 *         AC_MODE_COOL at or above cool_at_or_above_celsius,
 *         AC_MODE_OFF to leave the air conditioner as it is: believed on,
 *         status LED lit, door open, or room between the two.
 */
ac_mode_t thermostat_switch_on_mode(bool believed_power_on, float room_temperature_celsius, bool door_open,
                                    int light_level, int heat_at_or_below_celsius, int cool_at_or_above_celsius);

/**
 * @brief Tells whether the air conditioner should be switched off because the door has been open for delay_seconds.
 *
 * Counts the readings in a row with the door open and the air conditioner
 * on: closing the door, or switching the air conditioner off, starts the
 * count again.
 *
 * @param open_readings Readings counted so far; updated.
 * @param believed_power_on true if the air conditioner is believed on.
 * @param door_open Latest state of the door.
 * @param delay_seconds Seconds the door must stay open; an odd number counts
 *        one second less.
 * @return true at every reading from the one that completes delay_seconds,
 *         until the count starts again.
 */
bool door_should_switch_off(int *open_readings, bool believed_power_on, bool door_open, int delay_seconds);

/**
 * @brief Tells whether the status LED has contradicted the believed power
 *        for STATUS_LED_DISAGREEMENT_SECONDS.
 *
 * Counts the readings in a row with the LED lit while the air conditioner is
 * believed off, or dark while it is believed on. A reading where LED and
 * belief agree starts the count again.
 *
 * @param disagreeing_readings Readings counted so far; updated.
 * @param believed_power_on true if the air conditioner is believed on.
 * @param light_level Latest light level on the status LED.
 * @return true at every reading from the one that completes
 *         STATUS_LED_DISAGREEMENT_SECONDS, until the count starts again.
 */
bool status_led_contradicts_power(int *disagreeing_readings, bool believed_power_on, int light_level);
