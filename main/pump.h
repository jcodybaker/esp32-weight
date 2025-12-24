#ifndef PUMP_H
#define PUMP_H

#include "settings.h"
#include <esp_http_server.h>

typedef struct pump_context_t pump_context_t;

pump_context_t* pump_init(settings_t *settings, httpd_handle_t server);

char* pump_send_cmd(pump_context_t *pump_ctx, const char *cmd);

const char* pump_get_last_error();

#endif // PUMP_H

