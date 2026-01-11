#ifndef SYSLOG_H
#define SYSLOG_H

#include <esp_err.h>
#include <esp_http_server.h>
#include "settings.h"

/**
 * Initialize the syslog client
 * 
 * @param settings Pointer to settings structure
 * @return ESP_OK on success
 */
esp_err_t syslog_init(settings_t *settings);

/**
 * Register syslog settings HTTP handlers
 * 
 * @param settings Pointer to settings structure
 * @param http_server HTTP server handle
 * @return ESP_OK on success
 */
esp_err_t syslog_register(settings_t *settings, httpd_handle_t http_server);

/**
 * Shutdown the syslog client
 */
void syslog_deinit(void);

#endif // SYSLOG_H
