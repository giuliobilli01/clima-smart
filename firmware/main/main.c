#include "automation/automation.h"
#include "bot/bot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ir/ir.h"
#include "ir/ir_receiver_task.h"
#include "log.h"
#include "network/web_server.h"
#include "network/wifi.h"
#include "sensors/sensors.h"
#include "settings.h"

/** @brief Priority of the receiver, sensor, automation and bot tasks. */
#define TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/** @brief Stack of the IR transmitting, receiver, sensor and automation tasks, in bytes. */
#define TASK_STACK_SIZE 4096

/** @brief Stack of the Telegram bot task, in bytes. */
#define BOT_TASK_STACK_SIZE 8192

/** @brief Priority of the IR transmitting task: the highest. */
#define IR_TRANSMIT_PRIORITY (configMAX_PRIORITIES - 1)

/** @brief Core of the IR transmitting task. */
#define IR_TRANSMIT_CORE 1

/**
 * @brief Program entry point, called by ESP-IDF at boot.
 *
 * Prepares every module (settings, IR, sensors, automations, bot), then
 * starts their tasks: the IR transmitter on core 1, the receiver of the
 * original remote, the sensors and the automations. Last, connects to the
 * WiFi, starts the web server and the Telegram bot task.
 */
void app_main(void)
{
    settings_init();
    ir_init();
    sensors_init();
    automation_init();
    bot_init();
    log_ac_believed_state();

    xTaskCreatePinnedToCore(ir_transmit_task, "ir_transmit", TASK_STACK_SIZE, NULL, IR_TRANSMIT_PRIORITY, NULL,
                            IR_TRANSMIT_CORE);
    xTaskCreate(ir_receiver_task, "ir_receiver", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(sensors_task, "sensors", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
    xTaskCreate(automation_task, "automation", TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);

    wifi_start();
    web_server_start();
    xTaskCreate(bot_task, "bot", BOT_TASK_STACK_SIZE, NULL, TASK_PRIORITY, NULL);
}
