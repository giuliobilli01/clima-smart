#include "ir_receiver_task.h"
#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ir.h"
#include "log.h"

/** @brief How often the receiver is checked for a new message. */
#define RECEIVER_POLL_PERIOD_MS 10

/** @brief Size in bytes of the text that describes a received message. */
#define MESSAGE_DESCRIPTION_SIZE 512

/** @brief Description of the last received message. */
static char received_message_description[MESSAGE_DESCRIPTION_SIZE];

void ir_receiver_task(void *argument)
{
    while (1) {
        bool ac_state_changed;
        if (ir_receive_message(received_message_description, sizeof(received_message_description), &ac_state_changed)) {
            ESP_LOGI(LOG_TAG, "received %s", received_message_description);
            if (ac_state_changed)
                log_ac_believed_state();
        }

        vTaskDelay(pdMS_TO_TICKS(RECEIVER_POLL_PERIOD_MS));
    }
}
