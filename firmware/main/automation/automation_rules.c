#include "automation_rules.h"

ac_mode_t thermostat_switch_on_mode(bool believed_power_on, float room_temperature_celsius, bool door_open,
                                    int light_level, int heat_at_or_below_celsius, int cool_at_or_above_celsius)
{
    if (believed_power_on || door_open || light_level >= STATUS_LED_LIT_LIGHT_LEVEL)
        return AC_MODE_OFF;

    if (room_temperature_celsius <= heat_at_or_below_celsius)
        return AC_MODE_HEAT;

    if (room_temperature_celsius >= cool_at_or_above_celsius)
        return AC_MODE_COOL;

    return AC_MODE_OFF;
}

bool door_should_switch_off(int *open_readings, bool believed_power_on, bool door_open, int delay_seconds)
{
    *open_readings = believed_power_on && door_open ? *open_readings + 1 : 0;
    return *open_readings >= delay_seconds * 1000 / AUTOMATION_PERIOD_MS;
}

bool status_led_contradicts_power(int *disagreeing_readings, bool believed_power_on, int light_level)
{
    bool status_led_says_on = light_level >= STATUS_LED_LIT_LIGHT_LEVEL;
    *disagreeing_readings = status_led_says_on != believed_power_on ? *disagreeing_readings + 1 : 0;
    return *disagreeing_readings >= STATUS_LED_DISAGREEMENT_SECONDS * 1000 / AUTOMATION_PERIOD_MS;
}
