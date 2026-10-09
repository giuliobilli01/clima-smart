#include <cstring>
#include <string>
#include "ir.h"
#include "IRac.h"
#include "IRrecv.h"
#include "IRsend.h"
#include "IRutils.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ir_Whirlpool.h"
#include "settings.h"

/** @brief GPIO of the IR transmitter. */
#define IR_LED_GPIO GPIO_NUM_17

/** @brief GPIO of the IR receiver. */
#define IR_RECEIVER_GPIO GPIO_NUM_5

/** @brief How many flashes and pauses one received message can hold. */
#define IR_CAPTURE_BUFFER_SIZE 1024

/** @brief Milliseconds of darkness that end a received message. */
#define IR_MESSAGE_END_SILENCE_MS 50

/** @brief IR LED, set up to blink at 38 kHz for the commands of air_conditioner. */
static IRsend ir_led(IR_LED_GPIO);

/**
 * @brief Builds and sends the air conditioner commands, and keeps the believed state.
 *
 * Its field next holds the state the next command sends.
 */
static IRac air_conditioner(IR_LED_GPIO);

/**
 * @brief Records the flashes seen by the IR receiver and recognises their protocol.
 *
 * Copies each complete message before decoding it, and listens again at once.
 */
static IRrecv ir_receiver(IR_RECEIVER_GPIO, IR_CAPTURE_BUFFER_SIZE, IR_MESSAGE_END_SILENCE_MS, true);

/** @brief Lets one task at a time use air_conditioner and ir_receiver. */
static SemaphoreHandle_t ir_lock;

/** @brief Binary semaphore that starts an IR transmission. */
static SemaphoreHandle_t transmit_requested;

/** @brief Binary semaphore that signals the end of an IR transmission. */
static SemaphoreHandle_t transmit_finished;

/** @brief true if the last command was sent. */
static bool transmit_succeeded;

/**
 * @brief Copies a state into air_conditioner.next, the state the next command sends.
 *
 * @param state State to copy.
 */
static void set_next_state(const ac_state_t *state)
{
    air_conditioner.next.power = state->power_on;
    air_conditioner.next.mode = static_cast<stdAc::opmode_t>(state->mode);
    air_conditioner.next.degrees = state->target_temperature_celsius;
    air_conditioner.next.fanspeed = static_cast<stdAc::fanspeed_t>(state->fan);
}

/**
 * @brief Reads the state the air conditioner is believed to be in.
 *
 * @param believed_state Filled with the believed state.
 */
static void read_believed_state(ac_state_t *believed_state)
{
    stdAc::state_t library_believed_state = air_conditioner.getStatePrev();

    believed_state->power_on = library_believed_state.power;
    believed_state->mode = static_cast<ac_mode_t>(library_believed_state.mode);
    believed_state->target_temperature_celsius = library_believed_state.degrees;
    believed_state->fan = static_cast<ac_fan_t>(library_believed_state.fanspeed);
}

/** @brief Saves the believed state in the settings. */
static void save_believed_state(void)
{
    ac_state_t believed_state;
    read_believed_state(&believed_state);
    settings_save_ac_state(&believed_state);
}

void ir_init(void)
{
    ir_lock = xSemaphoreCreateMutex();
    transmit_requested = xSemaphoreCreateBinary();
    transmit_finished = xSemaphoreCreateBinary();

    ir_led.begin();
    ir_receiver.enableIRIn();

    int32_t learned_protocol;
    int16_t learned_model;
    if (settings_load_ac_protocol(&learned_protocol, &learned_model)) {
        air_conditioner.next.protocol = static_cast<decode_type_t>(learned_protocol);
        air_conditioner.next.model = learned_model;
    }

    ac_state_t saved_state;
    if (settings_load_ac_state(&saved_state))
        set_next_state(&saved_state);
    else
        air_conditioner.next.mode = stdAc::opmode_t::kCool;

    air_conditioner.next.celsius = true;
    air_conditioner.next.light = true;
    air_conditioner.markAsSent();
}

void ir_transmit_task(void *argument)
{
    while (1) {
        xSemaphoreTake(transmit_requested, portMAX_DELAY);

        ir_receiver.pause();
        transmit_succeeded = air_conditioner.sendAc();
        ir_receiver.resume();

        xSemaphoreGive(transmit_finished);
    }
}

bool ir_ac_send(const ac_state_t *desired)
{
    xSemaphoreTake(ir_lock, portMAX_DELAY);

    set_next_state(desired);
    xSemaphoreGive(transmit_requested);
    xSemaphoreTake(transmit_finished, portMAX_DELAY);

    bool sent = transmit_succeeded;
    if (sent)
        save_believed_state();

    xSemaphoreGive(ir_lock);
    return sent;
}

void ir_ac_get_believed_state(ac_state_t *believed_state)
{
    xSemaphoreTake(ir_lock, portMAX_DELAY);
    read_believed_state(believed_state);
    xSemaphoreGive(ir_lock);
}

void ir_ac_set_believed_power(bool power_on)
{
    xSemaphoreTake(ir_lock, portMAX_DELAY);

    air_conditioner.next = air_conditioner.getStatePrev();
    air_conditioner.next.power = power_on;
    air_conditioner.markAsSent();
    save_believed_state();

    xSemaphoreGive(ir_lock);
}

/**
 * @brief Relabels as WHIRLPOOL_AC a KELON168 message whose Whirlpool checksums are valid.
 *
 * @param message Message decoded by ir_receiver.
 */
static void relabel_misread_whirlpool_message(decode_results *message)
{
    if (message->decode_type == decode_type_t::KELON168 && IRWhirlpoolAc::validChecksum(message->state))
        message->decode_type = decode_type_t::WHIRLPOOL_AC;
}

/**
 * @brief Updates the believed state with the settings, protocol and model of a received message.
 *
 * Keeps the believed mode when the message gives the off mode. Saves the
 * believed state in the settings, and the protocol and model when they change.
 *
 * @param message Message decoded by ir_receiver.
 * @return true if the message came from an air conditioner remote.
 */
static bool update_state_from_remote(const decode_results *message)
{
    stdAc::state_t believed_state = air_conditioner.getStatePrev();
    if (!IRAcUtils::decodeToState(message, &air_conditioner.next, &believed_state))
        return false;

    if (air_conditioner.next.mode == stdAc::opmode_t::kOff)
        air_conditioner.next.mode = believed_state.mode;
    air_conditioner.next.protocol = message->decode_type;
    air_conditioner.markAsSent();
    save_believed_state();

    if (air_conditioner.next.protocol != believed_state.protocol || air_conditioner.next.model != believed_state.model)
        settings_save_ac_protocol(air_conditioner.next.protocol, air_conditioner.next.model);

    return true;
}

/**
 * @brief Writes the log text of a received message.
 *
 * The text holds the protocol name, the number of bits, the raw code in
 * hexadecimal and, for an air conditioner remote, its settings.
 *
 * @param message Message decoded by ir_receiver.
 * @param description Filled with the text.
 * @param description_size Size of description in bytes; longer text is cut.
 */
static void write_message_for_log(const decode_results *message, char *description, size_t description_size)
{
    std::string text = typeToString(message->decode_type, message->repeat) + ", " +
                       std::to_string(message->bits) + " bits, " + resultToHexidecimal(message);

    std::string air_conditioner_settings = IRAcUtils::resultAcToString(message);
    if (!air_conditioner_settings.empty())
        text += ", " + air_conditioner_settings;

    strlcpy(description, text.c_str(), description_size);
}

bool ir_receive_message(char *description, size_t description_size, bool *ac_state_changed)
{
    xSemaphoreTake(ir_lock, portMAX_DELAY);

    *ac_state_changed = false;
    decode_results message;
    bool received = ir_receiver.decode(&message);
    if (received) {
        relabel_misread_whirlpool_message(&message);
        *ac_state_changed = update_state_from_remote(&message);
        write_message_for_log(&message, description, description_size);
    }

    xSemaphoreGive(ir_lock);
    return received;
}
