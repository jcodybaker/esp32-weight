#ifndef WIFI_H
#define WIFI_H

#include "settings.h"
#include "esp_wifi.h"

void wifi_init(settings_t *settings);
int8_t wifi_get_rssi(void);

#endif // WIFI_H
