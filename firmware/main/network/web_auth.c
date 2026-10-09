#include "web_auth.h"
#include <string.h>
#include "mbedtls/base64.h"

/** @brief User name and password of the page, joined by a colon. */
#define WEB_CREDENTIALS CONFIG_CLIMA_WEB_USERNAME ":" CONFIG_CLIMA_WEB_PASSWORD

/** @brief Start of the Authorization header of HTTP Basic auth, before the encoded credentials. */
#define BASIC_AUTH_PREFIX "Basic "

/**
 * @brief Authorization header sent by a browser that knows WEB_CREDENTIALS:
 *        BASIC_AUTH_PREFIX followed by WEB_CREDENTIALS in base64.
 */
static char expected_authorization[sizeof(BASIC_AUTH_PREFIX) + (sizeof(WEB_CREDENTIALS) + 2) / 3 * 4];

void web_auth_init(void)
{
    size_t encoded_length;
    strlcpy(expected_authorization, BASIC_AUTH_PREFIX, sizeof(expected_authorization));

    mbedtls_base64_encode((unsigned char *)expected_authorization + strlen(BASIC_AUTH_PREFIX),
                          sizeof(expected_authorization) - strlen(BASIC_AUTH_PREFIX), &encoded_length,
                          (const unsigned char *)WEB_CREDENTIALS, strlen(WEB_CREDENTIALS));
}

bool web_auth_is_authorized(httpd_req_t *request)
{
    char authorization[sizeof(expected_authorization)];
    return httpd_req_get_hdr_value_str(request, "Authorization", authorization, sizeof(authorization)) == ESP_OK &&
           strcmp(authorization, expected_authorization) == 0;
}

esp_err_t web_auth_ask_for_credentials(httpd_req_t *request)
{
    httpd_resp_set_hdr(request, "WWW-Authenticate", "Basic realm=\"clima\"");
    return httpd_resp_send_err(request, HTTPD_401_UNAUTHORIZED, "User name and password required");
}
