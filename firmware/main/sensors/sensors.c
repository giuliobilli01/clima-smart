#include "sensors.h"
#include <math.h>
#include "driver/gpio.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "log.h"
#include "soc/adc_channel.h"

/** @brief ADC1 channel of the temperature sensor, on GPIO6. */
#define TEMPERATURE_SENSOR_CHANNEL ADC1_GPIO6_CHANNEL

/** @brief ADC1 channel of the light sensor, on GPIO4. */
#define LIGHT_SENSOR_CHANNEL ADC1_GPIO4_CHANNEL

/** @brief GPIO of the door switch, low while the door is closed. */
#define DOOR_SWITCH_GPIO GPIO_NUM_7

/** @brief Attenuation for the temperature sensor: measures up to 1100 mV, that is 60 degrees Celsius. */
#define TEMPERATURE_SENSOR_ATTENUATION ADC_ATTEN_DB_2_5

/** @brief Attenuation for the light sensor: the widest range, up to about 2900 mV. */
#define LIGHT_SENSOR_ATTENUATION ADC_ATTEN_DB_12

/** @brief Temperature samples averaged in each measurement. */
#define TEMPERATURE_SAMPLES 16

/** @brief Output of the TMP36 at 0 degrees Celsius. */
#define TMP36_MILLIVOLTS_AT_ZERO_CELSIUS 500

/** @brief Change of the TMP36 output for each degree. */
#define TMP36_MILLIVOLTS_PER_CELSIUS 10

/** @brief How often the room sensors are read and logged. */
#define SENSOR_READ_PERIOD_MS 2000

/** @brief Light samples in each measurement. */
#define LIGHT_SAMPLES 200

/** @brief Pause between two light samples. */
#define LIGHT_SAMPLE_GAP_US 100

/** @brief ADC1, shared by the temperature and light sensors. */
static adc_oneshot_unit_handle_t adc_unit_handle;

/**
 * @brief Converts the raw values of the temperature channel to millivolts,
 *        using the calibration stored in the chip.
 */
static adc_cali_handle_t temperature_calibration;

/** @brief Readings taken last by sensors_task(). */
static sensor_readings_t latest_readings;

/** @brief Lets one task at a time use latest_readings. */
static SemaphoreHandle_t latest_readings_lock;

/** @brief Last three temperature measurements, oldest first. */
static float recent_temperatures[3];

/**
 * @brief Measures the TMP36 temperature sensor once.
 *
 * Averages TEMPERATURE_SAMPLES calibrated readings, then turns the voltage
 * into degrees with the TMP36 formula.
 *
 * @return Room temperature in degrees Celsius, from this measurement alone.
 */
static float measure_room_temperature(void)
{
    float total_millivolts = 0;
    for (int sample = 0; sample < TEMPERATURE_SAMPLES; sample++) {
        int millivolts;
        ESP_ERROR_CHECK(adc_oneshot_get_calibrated_result(adc_unit_handle, temperature_calibration,
                                                          TEMPERATURE_SENSOR_CHANNEL, &millivolts));
        total_millivolts += millivolts;
    }

    float average_millivolts = total_millivolts / TEMPERATURE_SAMPLES;
    return (average_millivolts - TMP36_MILLIVOLTS_AT_ZERO_CELSIUS) / TMP36_MILLIVOLTS_PER_CELSIUS;
}

void sensors_init(void)
{
    adc_oneshot_unit_init_cfg_t adc_config = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_config, &adc_unit_handle));

    adc_oneshot_chan_cfg_t temperature_channel_config = {.atten = TEMPERATURE_SENSOR_ATTENUATION};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_unit_handle, TEMPERATURE_SENSOR_CHANNEL,
                                               &temperature_channel_config));
    adc_oneshot_chan_cfg_t light_channel_config = {.atten = LIGHT_SENSOR_ATTENUATION};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_unit_handle, LIGHT_SENSOR_CHANNEL, &light_channel_config));

    adc_cali_curve_fitting_config_t calibration_config = {
        .unit_id = ADC_UNIT_1,
        .chan = TEMPERATURE_SENSOR_CHANNEL,
        .atten = TEMPERATURE_SENSOR_ATTENUATION,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&calibration_config, &temperature_calibration));

    gpio_config_t door_switch_config = {
        .pin_bit_mask = BIT64(DOOR_SWITCH_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&door_switch_config));

    recent_temperatures[0] = measure_room_temperature();
    recent_temperatures[1] = measure_room_temperature();
    recent_temperatures[2] = measure_room_temperature();

    latest_readings_lock = xSemaphoreCreateMutex();
}

/**
 * @brief Measures the room temperature and returns the median of the last
 *        three measurements.
 *
 * @return Room temperature in degrees Celsius.
 */
static float read_room_temperature(void)
{
    recent_temperatures[0] = recent_temperatures[1];
    recent_temperatures[1] = recent_temperatures[2];
    recent_temperatures[2] = measure_room_temperature();

    float first = recent_temperatures[0];
    float second = recent_temperatures[1];
    float third = recent_temperatures[2];
    return fmaxf(fminf(first, second), fminf(fmaxf(first, second), third));
}

/**
 * @brief Measures the light on the air conditioner status LED.
 *
 * Takes LIGHT_SAMPLES samples, LIGHT_SAMPLE_GAP_US apart, and keeps the
 * highest.
 *
 * @return Light level, from 0 (dark) to 4095 (bright).
 */
static int measure_light_level(void)
{
    int highest_light_level = 0;
    for (int sample = 0; sample < LIGHT_SAMPLES; sample++) {
        int light_level;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_unit_handle, LIGHT_SENSOR_CHANNEL, &light_level));
        if (light_level > highest_light_level)
            highest_light_level = light_level;
        esp_rom_delay_us(LIGHT_SAMPLE_GAP_US);
    }

    return highest_light_level;
}

/**
 * @brief Reads all the room sensors.
 *
 * @param readings Filled with the new readings.
 */
static void read_sensors(sensor_readings_t *readings)
{
    readings->room_temperature_celsius = read_room_temperature();
    readings->light_level = measure_light_level();
    readings->door_open = gpio_get_level(DOOR_SWITCH_GPIO);
}

void sensors_task(void *argument)
{
    TickType_t last_wake_ticks = xTaskGetTickCount();

    while (1) {
        sensor_readings_t readings;
        read_sensors(&readings);

        xSemaphoreTake(latest_readings_lock, portMAX_DELAY);
        latest_readings = readings;
        xSemaphoreGive(latest_readings_lock);

        log_sensor_readings(&readings);
        vTaskDelayUntil(&last_wake_ticks, pdMS_TO_TICKS(SENSOR_READ_PERIOD_MS));
    }
}

void sensors_get_latest_readings(sensor_readings_t *readings)
{
    xSemaphoreTake(latest_readings_lock, portMAX_DELAY);
    *readings = latest_readings;
    xSemaphoreGive(latest_readings_lock);
}
