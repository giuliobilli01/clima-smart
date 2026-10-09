#include "bot_commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "automation/automation.h"
#include "ir/ir.h"
#include "log.h"
#include "sensors/sensors.h"

/** @brief Size in bytes of the buffer for an answer. */
#define ANSWER_SIZE 768

/** @brief Size in bytes of the buffers for the two words of a command, such as "/fan" and "medium". */
#define WORD_SIZE 16

/** @brief Format that reads the two words of a command, each cut to WORD_SIZE - 1 characters. */
#define TWO_WORDS_FORMAT "%15s %15s"

/** @brief Fan buttons in a full row. */
#define FAN_BUTTONS_PER_ROW 3

/**
 * @brief Reads the value of the commands that take "on" or "off".
 *
 * @param value Word after the command, such as "off" in "/auto off".
 * @param on Filled with true for "on", false for "off".
 * @return true if value is "on" or "off".
 */
static bool read_on_off(const char *value, bool *on)
{
    *on = strcmp(value, "on") == 0;
    return *on || strcmp(value, "off") == 0;
}

/**
 * @brief Runs /on and /off: sets the power in the state to send.
 *
 * @param value "on" for /on, "off" for /off: their fixed value in COMMANDS.
 * @param desired Copy of the believed state.
 * @return true if value is "on" or "off".
 */
static bool set_desired_power(const char *value, ac_state_t *desired)
{
    bool power_on;
    if (!read_on_off(value, &power_on))
        return false;

    desired->power_on = power_on;
    return true;
}

/**
 * @brief Reads the value of the commands that take a temperature: a whole
 *        number such as "24", or a change from the current value such as "+1"
 *        or "-2".
 *
 * @param value Word after the command, such as "+1" in "/temp +1".
 * @param current_celsius Current value of what the command changes; a change starts from it.
 * @param celsius Filled with the temperature read.
 * @return true if value is a whole number, with or without a sign.
 */
static bool read_temperature(const char *value, int current_celsius, int *celsius)
{
    char *end;
    long temperature = strtol(value, &end, 10);
    if (value[0] == '+' || value[0] == '-')
        temperature += current_celsius;
    if (end == value || *end != '\0')
        return false;

    *celsius = temperature;
    return true;
}

/**
 * @brief Runs /temp: sets the target temperature in the state to send.
 *
 * @param value Word after /temp: typed, such as "24", or "-1" and "+1" from the
 *        buttons of add_temperature_buttons().
 * @param desired Copy of the believed state; its target temperature is where a change starts from.
 * @return true if the new temperature is from AC_MIN_TARGET_TEMPERATURE_CELSIUS
 *         to AC_MAX_TARGET_TEMPERATURE_CELSIUS.
 */
static bool set_desired_temperature(const char *value, ac_state_t *desired)
{
    int temperature;
    if (!read_temperature(value, desired->target_temperature_celsius, &temperature) ||
        temperature < AC_MIN_TARGET_TEMPERATURE_CELSIUS || temperature > AC_MAX_TARGET_TEMPERATURE_CELSIUS)
        return false;

    desired->target_temperature_celsius = temperature;
    return true;
}

/**
 * @brief Runs /mode: sets the operating mode in the state to send.
 *
 * @param value Word after /mode, such as "cool": typed, or sent by a button of
 *        add_mode_buttons().
 * @param desired Copy of the believed state.
 * @return true if value is one of auto, cool, heat, dry, fan.
 */
static bool set_desired_mode(const char *value, ac_state_t *desired)
{
    for (ac_mode_t mode = AC_MODE_AUTO; mode <= AC_MODE_FAN; mode++) {
        if (strcmp(value, log_ac_mode_name(mode)) == 0) {
            desired->mode = mode;
            return true;
        }
    }

    return false;
}

/**
 * @brief Runs /fan: sets the fan speed in the state to send.
 *
 * @param value Word after /fan, such as "low": typed, or sent by a button of
 *        add_fan_buttons().
 * @param desired Copy of the believed state.
 * @return true if value is one of auto, min, low, medium, high, max.
 */
static bool set_desired_fan(const char *value, ac_state_t *desired)
{
    for (ac_fan_t fan = AC_FAN_AUTO; fan <= AC_FAN_MAX; fan++) {
        if (strcmp(value, log_ac_fan_name(fan)) == 0) {
            desired->fan = fan;
            return true;
        }
    }

    return false;
}

/**
 * @brief Runs /auto: switches the automations on or off, and saves the choice.
 *
 * @param value Word after /auto: "on" or "off".
 * @return true if value is "on" or "off".
 */
static bool set_automations(const char *value)
{
    bool enabled;
    if (!read_on_off(value, &enabled))
        return false;

    automation_set_enabled(enabled);
    return true;
}

/**
 * @brief Runs /autoheat: sets the room temperature at or below which the
 *        thermostat switches the air conditioner on to heat, and saves it.
 *
 * @param value Word after /autoheat: typed, such as "17", or "-1" and "+1" from the
 *        buttons of add_thermostat_heat_buttons().
 * @return true if the new threshold is from THERMOSTAT_THRESHOLD_MIN_CELSIUS to
 *         THERMOSTAT_THRESHOLD_MAX_CELSIUS and below the cooling one.
 */
static bool set_thermostat_heat(const char *value)
{
    thermostat_settings_t thermostat;
    automation_get_thermostat(&thermostat);

    return read_temperature(value, thermostat.heat_at_or_below_celsius, &thermostat.heat_at_or_below_celsius) &&
           automation_set_thermostat(&thermostat);
}

/**
 * @brief Runs /autocool: sets the room temperature at or above which the
 *        thermostat switches the air conditioner on to cool, and saves it.
 *
 * @param value Word after /autocool: typed, such as "27", or "-1" and "+1" from the
 *        buttons of add_thermostat_cool_buttons().
 * @return true if the new threshold is from THERMOSTAT_THRESHOLD_MIN_CELSIUS to
 *         THERMOSTAT_THRESHOLD_MAX_CELSIUS and above the heating one.
 */
static bool set_thermostat_cool(const char *value)
{
    thermostat_settings_t thermostat;
    automation_get_thermostat(&thermostat);

    return read_temperature(value, thermostat.cool_at_or_above_celsius, &thermostat.cool_at_or_above_celsius) &&
           automation_set_thermostat(&thermostat);
}

/**
 * @brief Runs /autotemp: sets the target temperature that the thermostat
 *        sets when it switches the air conditioner on, and saves it.
 *
 * @param value Word after /autotemp: typed, such as "23", or "-1" and "+1" from the
 *        buttons of add_thermostat_target_buttons().
 * @return true if the new target is from AC_MIN_TARGET_TEMPERATURE_CELSIUS to
 *         AC_MAX_TARGET_TEMPERATURE_CELSIUS.
 */
static bool set_thermostat_target(const char *value)
{
    thermostat_settings_t thermostat;
    automation_get_thermostat(&thermostat);

    return read_temperature(value, thermostat.target_celsius, &thermostat.target_celsius) &&
           automation_set_thermostat(&thermostat);
}

/**
 * @brief Runs /door: sets how long the door must stay open before the
 *        automations switch the air conditioner off, and saves it.
 *
 * @param value Word after /door, in seconds: typed, such as "60", or sent by a
 *        button of add_door_delay_buttons().
 * @return true if value is a whole number from DOOR_DELAY_MIN_SECONDS to
 *         DOOR_DELAY_MAX_SECONDS.
 */
static bool set_door_delay(const char *value)
{
    char *end;
    long seconds = strtol(value, &end, 10);
    return end != value && *end == '\0' && automation_set_door_delay(seconds);
}

/**
 * @brief Runs /sync: sets the believed power on the board only, and saves it.
 *
 * @param value Word after /sync, "on" or "off": the power the air conditioner is really in.
 * @return true if value is "on" or "off".
 */
static bool set_believed_power(const char *value)
{
    bool power_on;
    if (!read_on_off(value, &power_on))
        return false;

    ir_ac_set_believed_power(power_on);
    return true;
}

/**
 * @brief Runs /led: switches the status LED check on or off, and saves the choice.
 *
 * @param value Word after /led: "on" or "off".
 * @return true if value is "on" or "off".
 */
static bool set_status_led_check(const char *value)
{
    bool enabled;
    if (!read_on_off(value, &enabled))
        return false;

    automation_set_status_led_check_enabled(enabled);
    return true;
}

/**
 * @brief Button of /status, /on and /off: "Turn off" when the air conditioner
 *        is on, "Turn on" when it is off.
 *
 * The button sends "/off" or "/on".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Believed state after the command.
 */
static void add_power_button(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    if (believed_state->power_on)
        telegram_keyboard_add_button(keyboard, "Turn off", "/off");
    else
        telegram_keyboard_add_button(keyboard, "Turn on", "/on");
}

/**
 * @brief Buttons of /temp: "-1 C" above the lowest target temperature, and
 *        "+1 C" below the highest.
 *
 * The buttons send "/temp -1" and "/temp +1".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Believed state after the command.
 */
static void add_temperature_buttons(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    if (believed_state->target_temperature_celsius > AC_MIN_TARGET_TEMPERATURE_CELSIUS)
        telegram_keyboard_add_button(keyboard, "-1 C", "/temp -1");
    if (believed_state->target_temperature_celsius < AC_MAX_TARGET_TEMPERATURE_CELSIUS)
        telegram_keyboard_add_button(keyboard, "+1 C", "/temp +1");
}

/**
 * @brief Buttons of /mode: one per operating mode, such as "cool".
 *
 * Each button sends its mode, such as "/mode cool".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Unused: these buttons do not depend on the air conditioner.
 */
static void add_mode_buttons(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    char callback_data[TELEGRAM_TEXT_SIZE];
    for (ac_mode_t mode = AC_MODE_AUTO; mode <= AC_MODE_FAN; mode++) {
        snprintf(callback_data, sizeof(callback_data), "/mode %s", log_ac_mode_name(mode));
        telegram_keyboard_add_button(keyboard, log_ac_mode_name(mode), callback_data);
    }
}

/**
 * @brief Buttons of /fan: one per fan speed, such as "low", FAN_BUTTONS_PER_ROW
 *        per row.
 *
 * Each button sends its speed, such as "/fan low".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Unused: these buttons do not depend on the air conditioner.
 */
static void add_fan_buttons(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    char callback_data[TELEGRAM_TEXT_SIZE];
    keyboard->buttons_per_row = FAN_BUTTONS_PER_ROW;

    for (ac_fan_t fan = AC_FAN_AUTO; fan <= AC_FAN_MAX; fan++) {
        snprintf(callback_data, sizeof(callback_data), "/fan %s", log_ac_fan_name(fan));
        telegram_keyboard_add_button(keyboard, log_ac_fan_name(fan), callback_data);
    }
}

/**
 * @brief Button of /auto: "Automations off" when they are on, "Automations on"
 *        when they are off.
 *
 * The button sends "/auto off" or "/auto on".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Unused: this button does not depend on the air conditioner.
 */
static void add_automations_button(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    if (automation_is_enabled())
        telegram_keyboard_add_button(keyboard, "Automations off", "/auto off");
    else
        telegram_keyboard_add_button(keyboard, "Automations on", "/auto on");
}

/**
 * @brief Buttons of /autoheat: "-1 C" and "+1 C", which lower and raise the
 *        heating threshold of the thermostat.
 *
 * The buttons send "/autoheat -1" and "/autoheat +1".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Unused: these buttons do not depend on the air conditioner.
 */
static void add_thermostat_heat_buttons(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    telegram_keyboard_add_button(keyboard, "-1 C", "/autoheat -1");
    telegram_keyboard_add_button(keyboard, "+1 C", "/autoheat +1");
}

/**
 * @brief Buttons of /autocool: "-1 C" and "+1 C", which lower and raise the
 *        cooling threshold of the thermostat.
 *
 * The buttons send "/autocool -1" and "/autocool +1".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Unused: these buttons do not depend on the air conditioner.
 */
static void add_thermostat_cool_buttons(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    telegram_keyboard_add_button(keyboard, "-1 C", "/autocool -1");
    telegram_keyboard_add_button(keyboard, "+1 C", "/autocool +1");
}

/**
 * @brief Buttons of /autotemp: "-1 C" and "+1 C", which lower and raise the
 *        target temperature of the thermostat.
 *
 * The buttons send "/autotemp -1" and "/autotemp +1".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Unused: these buttons do not depend on the air conditioner.
 */
static void add_thermostat_target_buttons(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    telegram_keyboard_add_button(keyboard, "-1 C", "/autotemp -1");
    telegram_keyboard_add_button(keyboard, "+1 C", "/autotemp +1");
}

/**
 * @brief Buttons of /door: three door delays, 10 s, 30 s and 60 s.
 *
 * The buttons send "/door 10", "/door 30" and "/door 60".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Unused: these buttons do not depend on the air conditioner.
 */
static void add_door_delay_buttons(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    telegram_keyboard_add_button(keyboard, "10 s", "/door 10");
    telegram_keyboard_add_button(keyboard, "30 s", "/door 30");
    telegram_keyboard_add_button(keyboard, "60 s", "/door 60");
}

/**
 * @brief Button of /sync: the opposite of what the board believes, "It is off"
 *        when it believes on, "It is on" when it believes off.
 *
 * The button sends "/sync off" or "/sync on".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Believed state after the command.
 */
static void add_sync_button(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    if (believed_state->power_on)
        telegram_keyboard_add_button(keyboard, "It is off", "/sync off");
    else
        telegram_keyboard_add_button(keyboard, "It is on", "/sync on");
}

/**
 * @brief Button of /led: "LED check off" when the status LED check is on,
 *        "LED check on" when it is off.
 *
 * The button sends "/led off" or "/led on".
 *
 * @param keyboard Keyboard of the answer.
 * @param believed_state Unused: this button does not depend on the air conditioner.
 */
static void add_status_led_check_button(telegram_keyboard_t *keyboard, const ac_state_t *believed_state)
{
    if (automation_is_status_led_check_enabled())
        telegram_keyboard_add_button(keyboard, "LED check off", "/led off");
    else
        telegram_keyboard_add_button(keyboard, "LED check on", "/led on");
}

/**
 * @brief A command of the bot: its entry in the menu, what it changes, and the buttons under its answer.
 */
typedef struct {
    const char *name;        /**< Name without the slash, as in the menu. */
    const char *description; /**< Text next to the name, in the menu and in the list of commands. */
    const char *fixed_value; /**< Value the command always takes, such as "on" for /on; NULL if typed after it. */
    bool (*change_desired_state)(const char *value, ac_state_t *desired); /**< Changes the state sent by IR, or NULL. */
    bool (*change_board)(const char *value); /**< Changes a setting of the board, or what it believes, or NULL. */
    void (*add_buttons)(telegram_keyboard_t *keyboard, const ac_state_t *believed_state); /**< Adds its buttons. */
} bot_command_t;

/** @brief The commands of the bot, in the order of the menu. */
static const bot_command_t COMMANDS[] = {
    {.name = "status", .description = "Air conditioner state and room readings",
     .add_buttons = add_power_button},
    {.name = "on", .description = "Switch the air conditioner on",
     .fixed_value = "on", .change_desired_state = set_desired_power, .add_buttons = add_power_button},
    {.name = "off", .description = "Switch the air conditioner off",
     .fixed_value = "off", .change_desired_state = set_desired_power, .add_buttons = add_power_button},
    {.name = "temp", .description = "Target temperature: buttons, or /temp 24, /temp +1",
     .change_desired_state = set_desired_temperature, .add_buttons = add_temperature_buttons},
    {.name = "mode", .description = "Mode: buttons, or /mode cool",
     .change_desired_state = set_desired_mode, .add_buttons = add_mode_buttons},
    {.name = "fan", .description = "Fan speed: buttons, or /fan low",
     .change_desired_state = set_desired_fan, .add_buttons = add_fan_buttons},
    {.name = "auto", .description = "Automations: buttons, or /auto on, /auto off",
     .change_board = set_automations, .add_buttons = add_automations_button},
    {.name = "autoheat", .description = "Thermostat heats at or below: buttons, or /autoheat 17",
     .change_board = set_thermostat_heat, .add_buttons = add_thermostat_heat_buttons},
    {.name = "autocool", .description = "Thermostat cools at or above: buttons, or /autocool 27",
     .change_board = set_thermostat_cool, .add_buttons = add_thermostat_cool_buttons},
    {.name = "autotemp", .description = "Thermostat target: buttons, or /autotemp 23",
     .change_board = set_thermostat_target, .add_buttons = add_thermostat_target_buttons},
    {.name = "door", .description = "Seconds of open door before switching off: buttons, or /door 60",
     .change_board = set_door_delay, .add_buttons = add_door_delay_buttons},
    {.name = "sync", .description = "The board is wrong about on or off: buttons, or /sync on, /sync off",
     .change_board = set_believed_power, .add_buttons = add_sync_button},
    {.name = "led", .description = "Status LED check: buttons, or /led on, /led off",
     .change_board = set_status_led_check, .add_buttons = add_status_led_check_button},
};

/** @brief Number of commands in COMMANDS. */
#define COMMAND_COUNT (sizeof(COMMANDS) / sizeof(COMMANDS[0]))

/**
 * @brief Finds the command named by the first word of a message.
 *
 * @param word First word of the message, such as "/temp".
 * @return The command, or NULL if word is not a slash followed by the name of a command.
 */
static const bot_command_t *find_command(const char *word)
{
    if (word[0] != '/')
        return NULL;

    for (size_t index = 0; index < COMMAND_COUNT; index++) {
        if (strcmp(word + 1, COMMANDS[index].name) == 0)
            return &COMMANDS[index];
    }
    return NULL;
}

/** @brief How running a command went, which decides the text of the answer. */
typedef enum {
    COMMAND_DONE,          /**< Run, or nothing to run: the answer is the status. */
    COMMAND_INVALID_VALUE, /**< The value is not valid: the answer is the list of commands. */
    COMMAND_NO_PROTOCOL,   /**< No protocol learned from the remote yet: the answer asks for a key press on it. */
} command_outcome_t;

/**
 * @brief Changes the believed state as a command says, and sends it by IR.
 *
 * @param command Command to run; its change_desired_state is not NULL.
 * @param value Value of the command.
 * @return COMMAND_DONE if the new state was sent, COMMAND_INVALID_VALUE if
 *         value is not valid, COMMAND_NO_PROTOCOL if it could not be sent.
 */
static command_outcome_t send_desired_state_by_ir(const bot_command_t *command, const char *value)
{
    ac_state_t desired;
    ir_ac_get_believed_state(&desired);
    if (!command->change_desired_state(value, &desired))
        return COMMAND_INVALID_VALUE;

    if (!ir_ac_send(&desired))
        return COMMAND_NO_PROTOCOL;

    log_ac_believed_state();
    return COMMAND_DONE;
}

/**
 * @brief Runs a command with its value, when it has one.
 *
 * A command that changes the air conditioner sends the new state by IR; a
 * command that changes the board changes it.
 *
 * @param command Command to run.
 * @param typed_value Text after the command, empty if there is none; ignored
 *        by a command with a fixed value.
 * @return How running the command went.
 */
static command_outcome_t run_command(const bot_command_t *command, const char *typed_value)
{
    const char *value = command->fixed_value != NULL ? command->fixed_value : typed_value;
    if (value[0] == '\0')
        return COMMAND_DONE;

    if (command->change_desired_state != NULL)
        return send_desired_state_by_ir(command, value);
    if (command->change_board != NULL && !command->change_board(value))
        return COMMAND_INVALID_VALUE;

    return COMMAND_DONE;
}

/**
 * @brief Writes the status on five lines: the believed air conditioner state,
 *        the latest room readings, whether the automations are on with the
 *        door delay, the thermostat settings, and whether the status LED
 *        check is on.
 *
 * @param answer Buffer for the text.
 * @param answer_size Size of the buffer in bytes.
 */
static void write_status(char *answer, size_t answer_size)
{
    ac_state_t believed_state;
    ir_ac_get_believed_state(&believed_state);
    sensor_readings_t readings;
    sensors_get_latest_readings(&readings);
    thermostat_settings_t thermostat;
    automation_get_thermostat(&thermostat);

    snprintf(answer, answer_size,
             "Air conditioner: %s, %s, %.0f C, fan %s\nRoom: %.1f C, light %d/4095, door %s\n"
             "Automations: %s, off after %d s of open door\n"
             "Thermostat: heat at %d C or below, cool at %d C or above, to %d C\nStatus LED check: %s",
             believed_state.power_on ? "on" : "off", log_ac_mode_name(believed_state.mode),
             believed_state.target_temperature_celsius, log_ac_fan_name(believed_state.fan),
             readings.room_temperature_celsius, readings.light_level, readings.door_open ? "open" : "closed",
             automation_is_enabled() ? "on" : "off", automation_get_door_delay(), thermostat.heat_at_or_below_celsius,
             thermostat.cool_at_or_above_celsius, thermostat.target_celsius,
             automation_is_status_led_check_enabled() ? "on" : "off");
}

/**
 * @brief Writes the list of commands, one per line with its description.
 *
 * @param answer Buffer for the text.
 * @param answer_size Size of the buffer in bytes.
 */
static void write_command_list(char *answer, size_t answer_size)
{
    strlcpy(answer, "Commands:", answer_size);
    for (size_t index = 0; index < COMMAND_COUNT; index++) {
        strlcat(answer, "\n/", answer_size);
        strlcat(answer, COMMANDS[index].name, answer_size);
        strlcat(answer, " - ", answer_size);
        strlcat(answer, COMMANDS[index].description, answer_size);
    }
}

/**
 * @brief Answers a command that has been run.
 *
 * The text depends on how running it went: the status, the list of commands,
 * or the missing protocol. The buttons are those of the command, for the
 * believed state after it.
 *
 * @param message Message or button press to answer.
 * @param command Command that has been run.
 * @param outcome How running it went.
 */
static void reply_to_command(const telegram_message_t *message, const bot_command_t *command, command_outcome_t outcome)
{
    char answer[ANSWER_SIZE];
    switch (outcome) {
    case COMMAND_DONE:
        write_status(answer, sizeof(answer));
        break;
    case COMMAND_INVALID_VALUE:
        write_command_list(answer, sizeof(answer));
        break;
    case COMMAND_NO_PROTOCOL:
        strlcpy(answer, "No air conditioner protocol learned yet: press a key on its remote", sizeof(answer));
        break;
    }

    ac_state_t believed_state;
    ir_ac_get_believed_state(&believed_state);
    telegram_keyboard_t keyboard = {0};
    command->add_buttons(&keyboard, &believed_state);

    telegram_reply(message, answer, &keyboard);
}

void bot_commands_set_menu(void)
{
    telegram_command_t menu[COMMAND_COUNT];
    for (size_t index = 0; index < COMMAND_COUNT; index++)
        menu[index] = (telegram_command_t){.name = COMMANDS[index].name, .description = COMMANDS[index].description};

    telegram_set_menu(menu, COMMAND_COUNT);
}

void bot_commands_run_and_reply(const telegram_message_t *message)
{
    char command_word[WORD_SIZE] = "";
    char typed_value[WORD_SIZE] = "";
    sscanf(message->text, TWO_WORDS_FORMAT, command_word, typed_value);

    const bot_command_t *command = find_command(command_word);
    if (command == NULL) {
        char command_list[ANSWER_SIZE];
        write_command_list(command_list, sizeof(command_list));
        telegram_reply(message, command_list, NULL);
    } else {
        command_outcome_t outcome = run_command(command, typed_value);
        reply_to_command(message, command, outcome);
    }
}
