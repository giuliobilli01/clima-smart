#include <cstring>
#include <string>
#include "ir.h"
#include "IRac.h"
#include "IRrecv.h"
#include "IRutils.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ir_Whirlpool.h"

/** @brief Tag that marks this program's messages on the serial monitor. */
static const char *LOG_TAG = "testbed";

/** @brief GPIO of the IR receiver. */
#define IR_RECEIVER_GPIO GPIO_NUM_5

/** @brief How many flashes and pauses one received message can hold. */
#define IR_CAPTURE_BUFFER_SIZE 1024

/** @brief Milliseconds of darkness that end a received message. */
#define IR_MESSAGE_END_SILENCE_MS 50

/**
 * @brief Records the flashes seen by the IR receiver and recognises their protocol.
 *
 * Copies each complete message before decoding it, and listens again at once.
 */
static IRrecv ir_receiver(IR_RECEIVER_GPIO, IR_CAPTURE_BUFFER_SIZE, IR_MESSAGE_END_SILENCE_MS, true);

/** @brief State the fake air conditioner is in; each command is applied on top of it. */
static stdAc::state_t current_state;

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

void ir_init(void)
{
    IRac::initState(&current_state);
    ir_receiver.enableIRIn();
}

bool ir_receive_ac_state(received_ac_state_t *state)
{
    decode_results message;
    if (!ir_receiver.decode(&message))
        return false;

    relabel_misread_whirlpool_message(&message);
    std::string protocol = typeToString(message.decode_type);

    stdAc::state_t new_state;
    if (!IRAcUtils::decodeToState(&message, &new_state, &current_state)) {
        ESP_LOGI(LOG_TAG, "ignored %s, %d bits", protocol.c_str(), message.bits);
        return false;
    }
    current_state = new_state;

    state->power_on = current_state.power;
    state->target_temperature_celsius = current_state.degrees;
    strlcpy(state->mode_name, IRac::opmodeToString(current_state.mode).c_str(), sizeof(state->mode_name));
    strlcpy(state->fan_name, IRac::fanspeedToString(current_state.fanspeed).c_str(), sizeof(state->fan_name));

    ESP_LOGI(LOG_TAG, "received %s: %s, %s, %d C, fan %s", protocol.c_str(), state->power_on ? "on" : "off",
             state->mode_name, state->target_temperature_celsius, state->fan_name);
    ESP_LOGI(LOG_TAG, "settings: %s", IRAcUtils::resultAcToString(&message).c_str());

    return true;
}
