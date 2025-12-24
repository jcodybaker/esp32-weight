#ifndef PUMP_H
#define PUMP_H

#include "settings.h"
#include <esp_http_server.h>

void pump_init(settings_t *settings, httpd_handle_t server);

const char* pump_get_last_error();

#endif // PUMP_H

