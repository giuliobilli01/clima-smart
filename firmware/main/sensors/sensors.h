#pragma once
#include <stdbool.h>

/** @brief One reading of all the room sensors. */
typedef struct {
    float room_temperature_celsius;
    int light_level; /**< From 0 (dark) to 4095 (bright). */
    bool door_open;
} sensor_readings_t;

/**
 * @brief Prepares the analog inputs of the temperature and light sensors, the
 *        digital input of the door switch, and the mutex of the latest
 *        readings.
 *
 * Also takes the first three temperature measurements.
 */
void sensors_init(void);

/**
 * @brief Reads the room sensors every 2 s, keeps the readings for
 *        sensors_get_latest_readings() and logs them. Never returns.
 *
 * @param argument Unused.
 */
void sensors_task(void *argument);

/**
 * @brief Copies the readings taken last by sensors_task(), at most 2 s old.
 *
 * @param readings Filled with the latest readings.
 */
void sensors_get_latest_readings(sensor_readings_t *readings);
