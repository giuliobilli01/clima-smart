#pragma once
#include "ir/ir.h"
#include "sensors/sensors.h"

/** @brief Tag that marks this program's messages on the serial monitor. */
#define LOG_TAG "clima"

/**
 * @brief Returns the short name of an operating mode, as it appears in the log.
 *
 * @param mode Operating mode.
 * @return Constant string such as "cool".
 */
const char *log_ac_mode_name(ac_mode_t mode);

/**
 * @brief Returns the short name of a fan speed, as it appears in the log.
 *
 * @param fan Fan speed.
 * @return Constant string such as "low".
 */
const char *log_ac_fan_name(ac_fan_t fan);

/**
 * @brief Logs the state the air conditioner is believed to be in.
 *
 * Prints one line with power, mode, target temperature and fan speed,
 * e.g. "state: on, cool, 24 C, fan auto".
 */
void log_ac_believed_state(void);

/**
 * @brief Logs one reading of the room sensors.
 *
 * Prints one line with temperature, light and door,
 * e.g. "room: 23.4 C, light 812/4095, door closed".
 *
 * @param readings Readings to print.
 */
void log_sensor_readings(const sensor_readings_t *readings);
