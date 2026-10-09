#include "web_server.h"
#include <stdlib.h>
#include "cJSON.h"
#include "web_auth.h"
#include "ir/ir.h"
#include "log.h"
#include "sensors/sensors.h"

/** @brief Size in bytes of the buffer for the body of a command. */
#define COMMAND_BODY_SIZE 128

/** @brief The page, embedded in the firmware from index.html and ended by a NUL. */
extern const char index_html[] asm("_binary_index_html_start");

/**
 * @brief Answers GET / with the page.
 *
 * @param request HTTP request.
 * @return Result of sending the answer.
 */
static esp_err_t send_page(httpd_req_t *request)
{
    if (!web_auth_is_authorized(request))
        return web_auth_ask_for_credentials(request);

    return httpd_resp_sendstr(request, index_html);
}

/**
 * @brief Answers GET /state with the latest room readings and the believed
 *        air conditioner state, in JSON.
 *
 * The keys are the names of the fields of ac_state_t and sensor_readings_t,
 * for example {"power_on":true,"mode":1,"target_temperature_celsius":24,
 * "fan":0,"room_temperature_celsius":23.5,"light_level":812,"door_open":false}.
 *
 * @param request HTTP request.
 * @return Result of sending the answer.
 */
static esp_err_t send_state(httpd_req_t *request)
{
    if (!web_auth_is_authorized(request))
        return web_auth_ask_for_credentials(request);

    sensor_readings_t readings;
    sensors_get_latest_readings(&readings);
    ac_state_t believed_state;
    ir_ac_get_believed_state(&believed_state);

    cJSON *answer = cJSON_CreateObject();
    cJSON_AddBoolToObject(answer, "power_on", believed_state.power_on);
    cJSON_AddNumberToObject(answer, "mode", believed_state.mode);
    cJSON_AddNumberToObject(answer, "target_temperature_celsius", believed_state.target_temperature_celsius);
    cJSON_AddNumberToObject(answer, "fan", believed_state.fan);
    cJSON_AddNumberToObject(answer, "room_temperature_celsius", readings.room_temperature_celsius);
    cJSON_AddNumberToObject(answer, "light_level", readings.light_level);
    cJSON_AddBoolToObject(answer, "door_open", readings.door_open);

    char *answer_text = cJSON_PrintUnformatted(answer);
    cJSON_Delete(answer);

    httpd_resp_set_type(request, "application/json");
    esp_err_t result = httpd_resp_sendstr(request, answer_text);
    cJSON_free(answer_text);
    return result;
}

/**
 * @brief Reads one whole number from the body of a command and checks its range.
 *
 * @param body Body of the request, in the form "key=value&key=value".
 * @param key Name of the field.
 * @param min Lowest accepted value.
 * @param max Highest accepted value.
 * @param number Filled with the value when it is valid.
 * @return true if the field is present, is a whole number and is between min and max.
 */
static bool read_field(const char *body, const char *key, int min, int max, int *number)
{
    char text[8];
    if (httpd_query_key_value(body, key, text, sizeof(text)) != ESP_OK)
        return false;

    char *end;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < min || value > max)
        return false;
    *number = value;
    return true;
}

/**
 * @brief Answers POST /ac by sending the settings chosen on the page to the
 *        air conditioner.
 *
 * The body is a form, for example
 * "power_on=1&mode=1&target_temperature_celsius=24&fan=0", with the numbers
 * of ac_mode_t from auto to fan and of ac_fan_t from auto to max. A missing
 * or out of range field rejects the whole command with 400 Bad Request.
 * Without a learned protocol, answers 500 Internal Server Error.
 *
 * @param request HTTP request.
 * @return Result of sending the answer.
 */
static esp_err_t send_command(httpd_req_t *request)
{
    if (!web_auth_is_authorized(request))
        return web_auth_ask_for_credentials(request);

    char body[COMMAND_BODY_SIZE];
    if (request->content_len >= sizeof(body) ||
        httpd_req_recv(request, body, request->content_len) != request->content_len)
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid settings");
    body[request->content_len] = '\0';

    int power_on, mode, temperature, fan;
    if (!read_field(body, "power_on", 0, 1, &power_on) ||
        !read_field(body, "mode", AC_MODE_AUTO, AC_MODE_FAN, &mode) ||
        !read_field(body, "target_temperature_celsius", AC_MIN_TARGET_TEMPERATURE_CELSIUS,
                    AC_MAX_TARGET_TEMPERATURE_CELSIUS, &temperature) ||
        !read_field(body, "fan", AC_FAN_AUTO, AC_FAN_MAX, &fan))
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid settings");

    ac_state_t desired = {
        .power_on = power_on,
        .mode = mode,
        .target_temperature_celsius = temperature,
        .fan = fan,
    };

    if (!ir_ac_send(&desired))
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "No air conditioner protocol learned yet: press a key on its remote");

    log_ac_believed_state();
    return httpd_resp_sendstr(request, "Sent");
}

/** @brief Addresses served by the web server and the function that answers each. */
static const httpd_uri_t ROUTES[] = {
    {.uri = "/", .method = HTTP_GET, .handler = send_page},
    {.uri = "/state", .method = HTTP_GET, .handler = send_state},
    {.uri = "/ac", .method = HTTP_POST, .handler = send_command},
};

void web_server_start(void)
{
    web_auth_init();

    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    for (size_t route = 0; route < sizeof(ROUTES) / sizeof(ROUTES[0]); route++)
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ROUTES[route]));
}
