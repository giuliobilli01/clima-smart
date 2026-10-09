#pragma once

/**
 * @brief Connects to the home WiFi network set in menuconfig and announces
 *        the board on it as clima.local.
 *
 * Accepts only networks protected with WPA2 or stronger. Returns at once: the
 * connection completes in the background, and logs the address received from
 * the router. Connects again whenever the connection drops.
 */
void wifi_start(void);

/**
 * @brief Waits until the board is connected to the WiFi network and has an
 *        address; returns at once if it already is.
 */
void wifi_wait_for_connection(void);
