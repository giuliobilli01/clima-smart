#include "automation.h"
#include "automation_rules.h"
#include "bot/bot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ir/ir.h"
#include "log.h"
#include "sensors/sensors.h"
#include "settings.h"

/** @brief Settings of the automations. */
typedef struct {
    bool enabled;                     /**< true if the automations are on. */
    int door_delay_seconds;           /**< Seconds of open door before they switch the air conditioner off. */
    bool status_led_check_enabled;    /**< true if the status LED check is on. */
    thermostat_settings_t thermostat; /**< Thresholds and target temperature of the thermostat. */
} automation_settings_t;

/** @brief Settings in use by the automations. */
static automation_settings_t automation_settings;

/** @brief Lets one task at a time use automation_settings. */
static SemaphoreHandle_t automation_settings_lock;

/** @brief Readings in a row with the door open and the air conditioner on. */
static int door_open_readings;

/** @brief Readings in a row with the status LED contradicting the believed power. */
static int status_led_disagreeing_readings;

void automation_init(void)
{
    automation_settings_lock = xSemaphoreCreateMutex();

    automation_settings.enabled = settings_load_automations_enabled();
    automation_settings.status_led_check_enabled = settings_load_status_led_check_enabled();

    automation_settings.door_delay_seconds = DOOR_DELAY_DEFAULT_SECONDS;
    settings_load_door_delay(&automation_settings.door_delay_seconds);

    automation_settings.thermostat = (thermostat_settings_t){
        .heat_at_or_below_celsius = THERMOSTAT_HEAT_AT_OR_BELOW_DEFAULT_CELSIUS,
        .cool_at_or_above_celsius = THERMOSTAT_COOL_AT_OR_ABOVE_DEFAULT_CELSIUS,
        .target_celsius = THERMOSTAT_TARGET_DEFAULT_CELSIUS,
    };
    settings_load_thermostat(&automation_settings.thermostat);
}

void automation_set_enabled(bool enabled)
{
    xSemaphoreTake(automation_settings_lock, portMAX_DELAY);
    automation_settings.enabled = enabled;
    settings_save_automations_enabled(enabled);
    xSemaphoreGive(automation_settings_lock);

    ESP_LOGI(LOG_TAG, "automations %s", enabled ? "on" : "off");
}

bool automation_is_enabled(void)
{
    xSemaphoreTake(automation_settings_lock, portMAX_DELAY);
    bool enabled = automation_settings.enabled;
    xSemaphoreGive(automation_settings_lock);
    return enabled;
}

void automation_set_status_led_check_enabled(bool enabled)
{
    xSemaphoreTake(automation_settings_lock, portMAX_DELAY);
    automation_settings.status_led_check_enabled = enabled;
    settings_save_status_led_check_enabled(enabled);
    xSemaphoreGive(automation_settings_lock);

    ESP_LOGI(LOG_TAG, "status LED check %s", enabled ? "on" : "off");
}

bool automation_is_status_led_check_enabled(void)
{
    xSemaphoreTake(automation_settings_lock, portMAX_DELAY);
    bool enabled = automation_settings.status_led_check_enabled;
    xSemaphoreGive(automation_settings_lock);
    return enabled;
}

bool automation_set_door_delay(int seconds)
{
    if (seconds < DOOR_DELAY_MIN_SECONDS || seconds > DOOR_DELAY_MAX_SECONDS)
        return false;

    xSemaphoreTake(automation_settings_lock, portMAX_DELAY);
    automation_settings.door_delay_seconds = seconds;
    settings_save_door_delay(seconds);
    xSemaphoreGive(automation_settings_lock);

    ESP_LOGI(LOG_TAG, "door delay %d s", seconds);
    return true;
}

int automation_get_door_delay(void)
{
    xSemaphoreTake(automation_settings_lock, portMAX_DELAY);
    int seconds = automation_settings.door_delay_seconds;
    xSemaphoreGive(automation_settings_lock);
    return seconds;
}

bool automation_set_thermostat(const thermostat_settings_t *thermostat)
{
    if (thermostat->heat_at_or_below_celsius < THERMOSTAT_THRESHOLD_MIN_CELSIUS ||
        thermostat->cool_at_or_above_celsius > THERMOSTAT_THRESHOLD_MAX_CELSIUS ||
        thermostat->heat_at_or_below_celsius >= thermostat->cool_at_or_above_celsius ||
        thermostat->target_celsius < AC_MIN_TARGET_TEMPERATURE_CELSIUS ||
        thermostat->target_celsius > AC_MAX_TARGET_TEMPERATURE_CELSIUS)
        return false;

    xSemaphoreTake(automation_settings_lock, portMAX_DELAY);
    automation_settings.thermostat = *thermostat;
    settings_save_thermostat(thermostat);
    xSemaphoreGive(automation_settings_lock);

    ESP_LOGI(LOG_TAG, "thermostat: heat at %d C or below, cool at %d C or above, to %d C",
             thermostat->heat_at_or_below_celsius, thermostat->cool_at_or_above_celsius, thermostat->target_celsius);
    return true;
}

void automation_get_thermostat(thermostat_settings_t *thermostat)
{
    xSemaphoreTake(automation_settings_lock, portMAX_DELAY);
    *thermostat = automation_settings.thermostat;
    xSemaphoreGive(automation_settings_lock);
}

/**
 * @brief Copies the settings in use by the automations.
 *
 * @param settings Filled with the two switches, the door delay and the thermostat settings.
 */
static void read_automation_settings(automation_settings_t *settings)
{
    xSemaphoreTake(automation_settings_lock, portMAX_DELAY);
    *settings = automation_settings;
    xSemaphoreGive(automation_settings_lock);
}

/**
 * @brief Applies to one reading the first rule that holds among those that
 *        are on, if any: corrects the believed power, or switches the air
 *        conditioner off or on, and notifies the allowed chats.
 *
 * @param believed_state Believed state of the air conditioner; changed by the action.
 * @param readings Latest readings.
 * @param settings Switches of the rules, door delay and thermostat settings.
 */
static void apply_rules(ac_state_t *believed_state, const sensor_readings_t *readings,
                        const automation_settings_t *settings)
{
    int door_delay_seconds = settings->door_delay_seconds;
    const thermostat_settings_t *thermostat = &settings->thermostat;

    bool status_led_contradicts =
        settings->status_led_check_enabled &&
        status_led_contradicts_power(&status_led_disagreeing_readings, believed_state->power_on, readings->light_level);

    bool door_switch_off =
        settings->enabled &&
        door_should_switch_off(&door_open_readings, believed_state->power_on, readings->door_open, door_delay_seconds);

    ac_mode_t switch_on_mode =
        settings->enabled ? thermostat_switch_on_mode(believed_state->power_on, readings->room_temperature_celsius,
                                                      readings->door_open, readings->light_level,
                                                      thermostat->heat_at_or_below_celsius,
                                                      thermostat->cool_at_or_above_celsius)
                          : AC_MODE_OFF;

    if (status_led_contradicts) {

        believed_state->power_on = !believed_state->power_on;
        ir_ac_set_believed_power(believed_state->power_on);
        bot_notify("Air conditioner %s, as its status LED shows", believed_state->power_on ? "on" : "off");
    } else if (door_switch_off) {

        believed_state->power_on = false;
        if (ir_ac_send(believed_state))
            bot_notify("Air conditioner switched off: the door has been open for %d s", door_delay_seconds);
    } else if (switch_on_mode != AC_MODE_OFF) {

        believed_state->power_on = true;
        believed_state->mode = switch_on_mode;
        believed_state->target_temperature_celsius = thermostat->target_celsius;

        if (ir_ac_send(believed_state))
            bot_notify("Air conditioner switched on, %s at %d C: the room is at %.1f C",
                       log_ac_mode_name(switch_on_mode), thermostat->target_celsius,
                       readings->room_temperature_celsius);
    }
}

void automation_task(void *argument)
{
    TickType_t last_wake_ticks = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&last_wake_ticks, pdMS_TO_TICKS(AUTOMATION_PERIOD_MS));

        automation_settings_t settings;
        read_automation_settings(&settings);

        if (!settings.enabled)
            door_open_readings = 0;

        if (!settings.status_led_check_enabled)
            status_led_disagreeing_readings = 0;

        ac_state_t believed_state;
        ir_ac_get_believed_state(&believed_state);
        sensor_readings_t readings;
        sensors_get_latest_readings(&readings);

        apply_rules(&believed_state, &readings, &settings);
    }
}
