#include "wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "log.h"
#include "mdns.h"

/** @brief Name of the board on the network: the browser finds it at http://clima.local. */
static const char *HOSTNAME = "clima";

/** @brief Bit of connection_events that is set while the board has an address on the network. */
#define CONNECTED_BIT BIT0

/** @brief State of the connection, in CONNECTED_BIT. */
static EventGroupHandle_t connection_events;

/**
 * @brief Reacts to the WiFi and IP events.
 *
 * Connects when the WiFi driver has started, connects again when the
 * connection drops and logs the address received from the router.
 * Keeps CONNECTED_BIT up to date.
 *
 * @param argument Unused.
 * @param event_base WIFI_EVENT or IP_EVENT.
 * @param event_id Which event of that base.
 * @param event_data Details of the event; its type depends on event_id.
 */
static void handle_network_event(void *argument, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconnection = event_data;
        ESP_LOGW(LOG_TAG, "WiFi disconnected, reason %d, connecting again", disconnection->reason);
        xEventGroupClearBits(connection_events, CONNECTED_BIT);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *connection = event_data;
        ESP_LOGI(LOG_TAG, "connected: http://%s.local (" IPSTR ")", HOSTNAME, IP2STR(&connection->ip_info.ip));
        xEventGroupSetBits(connection_events, CONNECTED_BIT);
    }
}

void wifi_start(void)
{
    connection_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, handle_network_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, handle_network_event, NULL));

    wifi_init_config_t driver_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&driver_config));

    wifi_config_t network_config = {
        .sta = {
            .ssid = CONFIG_CLIMA_WIFI_SSID,
            .password = CONFIG_CLIMA_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &network_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(HOSTNAME));
}

void wifi_wait_for_connection(void)
{
    xEventGroupWaitBits(connection_events, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}
