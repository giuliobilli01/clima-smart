#include "settings.h"
#include "esp_log.h"
#include "log.h"
#include "nvs.h"
#include "nvs_flash.h"

/** @brief NVS namespace that groups all the settings of this program. */
static const char *SETTINGS_NAMESPACE = "clima";

/** @brief NVS key of the air conditioner protocol. */
static const char *AC_PROTOCOL_KEY = "ac_protocol";

/** @brief NVS key of the air conditioner model within its protocol. */
static const char *AC_MODEL_KEY = "ac_model";

/** @brief NVS key of the believed power of the air conditioner, 1 for on. */
static const char *AC_POWER_KEY = "ac_power";

/** @brief NVS key of the believed operating mode. */
static const char *AC_MODE_KEY = "ac_mode";

/** @brief NVS key of the believed target temperature, in whole degrees Celsius. */
static const char *AC_TEMPERATURE_KEY = "ac_temperature";

/** @brief NVS key of the believed fan speed. */
static const char *AC_FAN_KEY = "ac_fan";

/** @brief NVS key of the switch of the automations, 1 for on. */
static const char *AUTOMATIONS_ENABLED_KEY = "automations";

/** @brief NVS key of the door delay, in seconds. */
static const char *DOOR_DELAY_KEY = "door_delay";

/** @brief NVS key of the switch of the status LED check, 1 for on. */
static const char *STATUS_LED_CHECK_ENABLED_KEY = "led_check";

/** @brief NVS key of the room temperature at or below which the thermostat heats, in whole degrees Celsius. */
static const char *THERMOSTAT_HEAT_KEY = "heat_at";

/** @brief NVS key of the room temperature at or above which the thermostat cools, in whole degrees Celsius. */
static const char *THERMOSTAT_COOL_KEY = "cool_at";

/** @brief NVS key of the target temperature set by the thermostat, in whole degrees Celsius. */
static const char *THERMOSTAT_TARGET_KEY = "auto_temp";

void settings_init(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
}

bool settings_load_ac_protocol(int32_t *protocol, int16_t *model)
{
    nvs_handle_t settings;
    if (nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &settings) != ESP_OK)
        return false;

    int32_t saved_protocol;
    int16_t saved_model;
    bool found = nvs_get_i32(settings, AC_PROTOCOL_KEY, &saved_protocol) == ESP_OK &&
                 nvs_get_i16(settings, AC_MODEL_KEY, &saved_model) == ESP_OK;
    nvs_close(settings);

    if (found) {
        *protocol = saved_protocol;
        *model = saved_model;
    }
    return found;
}

void settings_save_ac_protocol(int32_t protocol, int16_t model)
{
    nvs_handle_t settings;
    esp_err_t result = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &settings);
    if (result == ESP_OK) {
        result = nvs_set_i32(settings, AC_PROTOCOL_KEY, protocol);
        if (result == ESP_OK)
            result = nvs_set_i16(settings, AC_MODEL_KEY, model);
        if (result == ESP_OK)
            result = nvs_commit(settings);

        nvs_close(settings);
    }

    if (result != ESP_OK)
        ESP_LOGE(LOG_TAG, "cannot save the air conditioner protocol: %s", esp_err_to_name(result));
}

bool settings_load_ac_state(ac_state_t *believed_state)
{
    nvs_handle_t settings;
    if (nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &settings) != ESP_OK)
        return false;

    int8_t saved_power_on, saved_mode, saved_temperature, saved_fan;
    bool found = nvs_get_i8(settings, AC_POWER_KEY, &saved_power_on) == ESP_OK &&
                 nvs_get_i8(settings, AC_MODE_KEY, &saved_mode) == ESP_OK &&
                 nvs_get_i8(settings, AC_TEMPERATURE_KEY, &saved_temperature) == ESP_OK &&
                 nvs_get_i8(settings, AC_FAN_KEY, &saved_fan) == ESP_OK;
    nvs_close(settings);

    if (found) {
        believed_state->power_on = saved_power_on;
        believed_state->mode = saved_mode;
        believed_state->target_temperature_celsius = saved_temperature;
        believed_state->fan = saved_fan;
    }
    return found;
}

void settings_save_ac_state(const ac_state_t *believed_state)
{
    nvs_handle_t settings;
    esp_err_t result = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &settings);
    if (result == ESP_OK) {
        result = nvs_set_i8(settings, AC_POWER_KEY, believed_state->power_on);
        if (result == ESP_OK)
            result = nvs_set_i8(settings, AC_MODE_KEY, believed_state->mode);
        if (result == ESP_OK)
            result = nvs_set_i8(settings, AC_TEMPERATURE_KEY, believed_state->target_temperature_celsius);
        if (result == ESP_OK)
            result = nvs_set_i8(settings, AC_FAN_KEY, believed_state->fan);
        if (result == ESP_OK)
            result = nvs_commit(settings);

        nvs_close(settings);
    }

    if (result != ESP_OK)
        ESP_LOGE(LOG_TAG, "cannot save the air conditioner state: %s", esp_err_to_name(result));
}

bool settings_load_automations_enabled(void)
{
    nvs_handle_t settings;
    if (nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &settings) != ESP_OK)
        return false;

    int8_t saved_enabled = 0;
    nvs_get_i8(settings, AUTOMATIONS_ENABLED_KEY, &saved_enabled);
    nvs_close(settings);

    return saved_enabled;
}

void settings_save_automations_enabled(bool enabled)
{
    nvs_handle_t settings;
    esp_err_t result = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &settings);
    if (result == ESP_OK) {
        result = nvs_set_i8(settings, AUTOMATIONS_ENABLED_KEY, enabled);
        if (result == ESP_OK)
            result = nvs_commit(settings);

        nvs_close(settings);
    }

    if (result != ESP_OK)
        ESP_LOGE(LOG_TAG, "cannot save the automations switch: %s", esp_err_to_name(result));
}

bool settings_load_door_delay(int *seconds)
{
    nvs_handle_t settings;
    if (nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &settings) != ESP_OK)
        return false;

    int16_t saved_seconds;
    bool found = nvs_get_i16(settings, DOOR_DELAY_KEY, &saved_seconds) == ESP_OK;
    nvs_close(settings);

    if (found)
        *seconds = saved_seconds;
    return found;
}

void settings_save_door_delay(int seconds)
{
    nvs_handle_t settings;
    esp_err_t result = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &settings);
    if (result == ESP_OK) {
        result = nvs_set_i16(settings, DOOR_DELAY_KEY, seconds);
        if (result == ESP_OK)
            result = nvs_commit(settings);

        nvs_close(settings);
    }

    if (result != ESP_OK)
        ESP_LOGE(LOG_TAG, "cannot save the door delay: %s", esp_err_to_name(result));
}

bool settings_load_status_led_check_enabled(void)
{
    nvs_handle_t settings;
    if (nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &settings) != ESP_OK)
        return false;

    int8_t saved_enabled = 0;
    nvs_get_i8(settings, STATUS_LED_CHECK_ENABLED_KEY, &saved_enabled);
    nvs_close(settings);

    return saved_enabled;
}

void settings_save_status_led_check_enabled(bool enabled)
{
    nvs_handle_t settings;
    esp_err_t result = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &settings);
    if (result == ESP_OK) {
        result = nvs_set_i8(settings, STATUS_LED_CHECK_ENABLED_KEY, enabled);
        if (result == ESP_OK)
            result = nvs_commit(settings);

        nvs_close(settings);
    }

    if (result != ESP_OK)
        ESP_LOGE(LOG_TAG, "cannot save the status LED check switch: %s", esp_err_to_name(result));
}

bool settings_load_thermostat(thermostat_settings_t *thermostat)
{
    nvs_handle_t settings;
    if (nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &settings) != ESP_OK)
        return false;

    int8_t saved_heat, saved_cool, saved_target;
    bool found = nvs_get_i8(settings, THERMOSTAT_HEAT_KEY, &saved_heat) == ESP_OK &&
                 nvs_get_i8(settings, THERMOSTAT_COOL_KEY, &saved_cool) == ESP_OK &&
                 nvs_get_i8(settings, THERMOSTAT_TARGET_KEY, &saved_target) == ESP_OK;
    nvs_close(settings);

    if (found) {
        thermostat->heat_at_or_below_celsius = saved_heat;
        thermostat->cool_at_or_above_celsius = saved_cool;
        thermostat->target_celsius = saved_target;
    }
    return found;
}

void settings_save_thermostat(const thermostat_settings_t *thermostat)
{
    nvs_handle_t settings;
    esp_err_t result = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &settings);
    if (result == ESP_OK) {
        result = nvs_set_i8(settings, THERMOSTAT_HEAT_KEY, thermostat->heat_at_or_below_celsius);
        if (result == ESP_OK)
            result = nvs_set_i8(settings, THERMOSTAT_COOL_KEY, thermostat->cool_at_or_above_celsius);
        if (result == ESP_OK)
            result = nvs_set_i8(settings, THERMOSTAT_TARGET_KEY, thermostat->target_celsius);
        if (result == ESP_OK)
            result = nvs_commit(settings);

        nvs_close(settings);
    }

    if (result != ESP_OK)
        ESP_LOGE(LOG_TAG, "cannot save the thermostat: %s", esp_err_to_name(result));
}
