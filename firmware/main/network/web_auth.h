#pragma once
#include <stdbool.h>
#include "esp_http_server.h"

/**
 * @brief Prepares the Authorization header that a browser sends when it
 *        knows the user name and password set in menuconfig.
 */
void web_auth_init(void);

/**
 * @brief Tells whether a request carries the user name and password of the page.
 *
 * @param request HTTP request just received.
 * @return true if its Authorization header is the expected one.
 */
bool web_auth_is_authorized(httpd_req_t *request);

/**
 * @brief Answers 401 Unauthorized, which makes the browser ask for user name
 *        and password.
 *
 * @param request HTTP request without valid credentials.
 * @return Result of sending the answer.
 */
esp_err_t web_auth_ask_for_credentials(httpd_req_t *request);
