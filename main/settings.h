#ifndef SETTINGS_H
#define SETTINGS_H

#include <esp_err.h>
#include <hx711.h>

typedef struct {
    char *update_url;
    char *password;
    int32_t weight_tare;
    float weight_scale;
    hx711_gain_t weight_gain;
} settings_t;

esp_err_t nvs_init(settings_t *settings);

#endif // SETTINGS_H