
#ifndef BTHOME_OBSERVER_H
#define BTHOME_OBSERVER_H

#include <esp_http_server.h>
#include "settings.h"

void bthome_observer_init(settings_t *settings, httpd_handle_t server);

#endif // BTHOME_OBSERVER_H