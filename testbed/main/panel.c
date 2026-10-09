#include "panel.h"
#include <stdio.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hd44780.h"

/** @brief GPIO of the power LED, on while the air conditioner is on. */
#define POWER_LED_GPIO GPIO_NUM_4

/** @brief GPIO of the buzzer. */
#define BUZZER_GPIO GPIO_NUM_6

/** @brief Pitch of the beep, in hertz. */
#define BEEP_FREQUENCY_HZ 4000

/** @brief Length of the beep, in milliseconds. */
#define BEEP_DURATION_MS 100

/** @brief How long the buzzer pin stays high in each period, out of 256: half. */
#define BEEP_DUTY 128

/** @brief Characters in one line of the display, without the terminator. */
#define DISPLAY_COLUMNS 16

/** @brief Size in bytes of the text of one line, NUL included. */
#define LINE_TEXT_SIZE 32

/** @brief The display: GPIOs of its pins, font and number of lines. */
static const hd44780_t display = {
    .pins = {
        .rs = GPIO_NUM_7,
        .e = GPIO_NUM_15,
        .d4 = GPIO_NUM_16,
        .d5 = GPIO_NUM_17,
        .d6 = GPIO_NUM_18,
        .d7 = GPIO_NUM_8,
        .bl = HD44780_NOT_USED,
    },
    .font = HD44780_FONT_5X8,
    .lines = 2,
};

/**
 * @brief Prepares the BEEP_FREQUENCY_HZ square wave of the buzzer, silent.
 */
static void buzzer_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = BEEP_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {
        .gpio_num = BUZZER_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

/**
 * @brief Sounds the buzzer once, for BEEP_DURATION_MS.
 */
static void beep(void)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, BEEP_DUTY);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    vTaskDelay(pdMS_TO_TICKS(BEEP_DURATION_MS));

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/**
 * @brief Replaces one line of the display.
 *
 * @param line Line to write: 0 for the first, 1 for the second.
 * @param text Text of the line, cut to DISPLAY_COLUMNS characters.
 */
static void write_line(uint8_t line, const char *text)
{
    char padded[DISPLAY_COLUMNS + 1];
    snprintf(padded, sizeof(padded), "%-*.*s", DISPLAY_COLUMNS, DISPLAY_COLUMNS, text);

    hd44780_gotoxy(&display, 0, line);
    hd44780_puts(&display, padded);
}

void panel_init(void)
{
    ESP_ERROR_CHECK(hd44780_init(&display));
    write_line(0, "Clima test-bed");

    gpio_config_t power_led_config = {
        .pin_bit_mask = BIT64(POWER_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&power_led_config));

    buzzer_init();
}

void panel_show(const received_ac_state_t *state)
{
    char text[LINE_TEXT_SIZE];
    snprintf(text, sizeof(text), "%s %s %dC", state->power_on ? "On" : "Off", state->mode_name,
             state->target_temperature_celsius);
    write_line(0, text);

    snprintf(text, sizeof(text), "Fan %s", state->fan_name);
    write_line(1, text);

    gpio_set_level(POWER_LED_GPIO, state->power_on);
    beep();
}
