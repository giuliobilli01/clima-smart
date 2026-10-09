#include "log.h"
#include "esp_log.h"

const char *log_ac_mode_name(ac_mode_t mode)
{
    switch (mode) {
    case AC_MODE_OFF: return "off";
    case AC_MODE_COOL: return "cool";
    case AC_MODE_HEAT: return "heat";
    case AC_MODE_DRY: return "dry";
    case AC_MODE_FAN: return "fan";
    default: return "auto";
    }
}

const char *log_ac_fan_name(ac_fan_t fan)
{
    switch (fan) {
    case AC_FAN_MIN: return "min";
    case AC_FAN_LOW: return "low";
    case AC_FAN_MEDIUM: return "medium";
    case AC_FAN_HIGH: return "high";
    case AC_FAN_MAX: return "max";
    case AC_FAN_MEDIUM_HIGH: return "medium-high";
    default: return "auto";
    }
}

void log_ac_believed_state(void)
{
    ac_state_t believed_state;
    ir_ac_get_believed_state(&believed_state);

    ESP_LOGI(LOG_TAG, "state: %s, %s, %.0f C, fan %s", believed_state.power_on ? "on" : "off",
             log_ac_mode_name(believed_state.mode), believed_state.target_temperature_celsius,
             log_ac_fan_name(believed_state.fan));
}

void log_sensor_readings(const sensor_readings_t *readings)
{
    ESP_LOGI(LOG_TAG, "room: %.1f C, light %d/4095, door %s", readings->room_temperature_celsius,
             readings->light_level, readings->door_open ? "open" : "closed");
}
