#ifndef METRICS_H
#define METRICS_H

#include "settings.h"
#include <esp_http_server.h>

void metrics_init(settings_t *settings, httpd_handle_t server);

#endif // METRICS_H
