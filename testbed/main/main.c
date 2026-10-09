#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ir.h"
#include "panel.h"

/** @brief Priority of the receiver task. */
#define TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/** @brief Stack of the receiver task, in bytes. */
#define TASK_STACK_SIZE 4096

/** @brief How often the receiver is checked for a new message. */
#define RECEIVER_POLL_PERIOD_MS 10

/**
 * @brief Checks the IR receiver every 10 ms and shows every air conditioner
 *        command on the panel. Never returns.
 *
 * @param argument Unused.
 */
static void receiver_task(void *argument)
{
    while (1) {
        received_ac_state_t state;
        if (ir_receive_ac_state(&state))
            panel_show(&state);

        vTaskDelay(pdMS_TO_TICKS(RECEIVER_POLL_PERIOD_MS));
    }
}

/**
 * @brief Program entry point, called by ESP-IDF at boot.
 *
 * Prepares the receiver and the panel, then creates the receiver task.
 */
void app_main(void)
{
    ir_init();
    panel_init();

    xTaskCreate(receiver_task, "receiver", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
}
