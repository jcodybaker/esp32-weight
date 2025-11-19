#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <esp_http_server.h>

httpd_handle_t http_server_init();

void httpd_register_basic_auth(httpd_handle_t server);

#endif // HTTP_SERVER_H