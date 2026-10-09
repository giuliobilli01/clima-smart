#pragma once

/**
 * @brief Starts the web server on port 80.
 *
 * Checks on every request the user name and password set in menuconfig, and
 * asks the browser for them when they are missing or wrong. Serves:
 * - GET / : the page;
 * - GET /state : latest room readings and believed air conditioner state, in JSON;
 * - POST /ac : new air conditioner settings, sent at once by IR.
 *
 * The server answers from its own task.
 */
void web_server_start(void);
