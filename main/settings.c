/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Non-Volatile Storage (NVS) Read and Write a Value - Example

   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "settings.h"

static const char *TAG = "settings";

esp_err_t settings_init(settings_t *settings)
{
    // Open NVS handle
    ESP_LOGI(TAG, "\nOpening Non-Volatile Storage (NVS) handle...");
    nvs_handle_t settings_handle;
    esp_err_t err = nvs_open("settings", NVS_READWRITE, &settings_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "\nReading 'update_url' from NVS...");
    err = nvs_get_str(settings_handle, "update_url", &settings->update_url);
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Read 'update_url' = %s", settings->update_url);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            settings->update_url = CONFIG_OTA_FIRMWARE_UPGRADE_URL;
            ESP_LOGI(TAG, "No value for 'update_url'; using default = %s", settings->update_url);
            break;
        default:
            ESP_LOGE(TAG, "Error (%s) reading update_url!", esp_err_to_name(err));
            return err;
    }

    ESP_LOGI(TAG, "\nReading 'password' from NVS...");
    err = nvs_get_str(settings_handle, "password", &settings->password);
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Read 'password' = %s", settings->password);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            settings->password = CONFIG_HTTPD_BASIC_AUTH_PASSWORD;
            ESP_LOGI(TAG, "No value for 'password'; using default = %s", settings->password);
            break;
        default:
            ESP_LOGE(TAG, "Error (%s) reading password!", esp_err_to_name(err));
            return err;
    }

    ESP_LOGI(TAG, "\nReading 'weight_tare' from NVS...");
    err = nvs_get_i32(settings_handle, "weight_tare", &settings->weight_tare);
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Read 'weight_tare' = %d", settings->weight_tare);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            settings->weight_tare = CONFIG_WEIGHT_TARE;
            ESP_LOGI(TAG, "No value for 'weight_tare'; using default = %d", settings->weight_tare);
            break;
        default:
            ESP_LOGE(TAG, "Error (%s) reading weight_tare!", esp_err_to_name(err));
            return err;
    }
    
    ESP_LOGI(TAG, "\nReading 'weight_scale' from NVS...");
    err = nvs_get_blob(settings_handle, "weight_scale", &settings->weight_scale, sizeof(settings->weight_scale));
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Read 'weight_scale' = %f", settings->weight_scale);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            settings->weight_scale = CONFIG_WEIGHT_SCALE;
            ESP_LOGI(TAG, "No value for 'weight_scale'; using default = %f", settings->weight_scale);
            break;
        default:
            ESP_LOGE(TAG, "Error (%s) reading weight_scale!", esp_err_to_name(err));
            return err;
    }

    ESP_LOGI(TAG, "\nReading 'weight_gain' from NVS...");
    err = nvs_get_i32(settings_handle, "weight_gain", &settings->weight_gain);
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Read 'weight_gain' = %d", settings->weight_gain);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            settings->weight_gain = CONFIG_WEIGHT_GAIN;
            ESP_LOGI(TAG, "No value for 'weight_gain'; using default = %d", settings->weight_gain);
            break;
        default:
            ESP_LOGE(TAG, "Error (%s) reading weight_gain!", esp_err_to_name(err));
            return err;
    }
    return ESP_OK
}

