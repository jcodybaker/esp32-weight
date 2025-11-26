#ifndef OTA_H
#define OTA_H

#include <esp_http_server.h>
#include "settings.h"

esp_err_t ota_init(settings_t *settings, httpd_handle_t http_server);

#endif // OTA_H
