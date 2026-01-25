/* OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "string.h"
#include "esp_crt_bundle.h"

#include "nvs.h"
#include "nvs_flash.h"
#include <sys/socket.h>

#include "ota.h"
#include "http_server.h"

#define HASH_LEN 32
#define OTA_NVS_NAMESPACE "ota"
#define OTA_PENDING_KEY "pending"
#define OTA_STATUS_KEY "status"
#define OTA_STATUS_MAX_LEN 128

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
/* The interface name value can refer to if_desc in esp_netif_defaults.h */
#if CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_ETH
static const char *bind_interface_name = EXAMPLE_NETIF_DESC_ETH;
#elif CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF_STA
static const char *bind_interface_name = EXAMPLE_NETIF_DESC_STA;
#endif
#endif

static const char *TAG = "ota";

static int8_t update_in_progress = 0;
static settings_t *ota_settings = NULL;
static bool ota_pending_on_wifi = false;
static char ota_last_status[OTA_STATUS_MAX_LEN] = {0};

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#define OTA_URL_SIZE 256

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

// Set OTA pending flag in NVS
static esp_err_t ota_set_pending(bool pending)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle for OTA pending: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t value = pending ? 1 : 0;
    err = nvs_set_u8(nvs_handle, OTA_PENDING_KEY, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing OTA pending flag: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

// Get OTA pending flag from NVS
static bool ota_get_pending(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }

    uint8_t value = 0;
    err = nvs_get_u8(nvs_handle, OTA_PENDING_KEY, &value);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        return false;
    }

    return value != 0;
}

// Set OTA status message in NVS
static esp_err_t ota_set_status(const char* status)
{
    if (!status) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle for OTA status: %s", esp_err_to_name(err));
        return err;
    }

    // Truncate status to max length
    char truncated[OTA_STATUS_MAX_LEN];
    strncpy(truncated, status, OTA_STATUS_MAX_LEN - 1);
    truncated[OTA_STATUS_MAX_LEN - 1] = '\0';

    err = nvs_set_str(nvs_handle, OTA_STATUS_KEY, truncated);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing OTA status: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Also update the in-memory copy
    strncpy(ota_last_status, truncated, OTA_STATUS_MAX_LEN - 1);
    ota_last_status[OTA_STATUS_MAX_LEN - 1] = '\0';

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}

// Get OTA status message from NVS
const char* ota_get_last_status(void)
{
    return ota_last_status;
}

static void ota_load_status(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ota_last_status[0] = '\0';
        return;
    }

    size_t required_size = OTA_STATUS_MAX_LEN;
    err = nvs_get_str(nvs_handle, OTA_STATUS_KEY, ota_last_status, &required_size);
    nvs_close(nvs_handle);

    if (err != ESP_OK) {
        ota_last_status[0] = '\0';
    }
}

void ota_task(void *pvParameter)
{
    settings_t *settings = (settings_t *)pvParameter;
    ESP_LOGI(TAG, "Starting OTA example task");
    esp_http_client_config_t config = {
        .url = settings->update_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
        .buffer_size = 4096,
        .buffer_size_tx = 2048,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Succeed, Rebooting...");
        ota_set_status("OTA update successful");
        ota_set_pending(false);  // Clear pending flag
        vTaskDelay(pdMS_TO_TICKS(1000));  // Give time for status to be written
        esp_restart();
    } else {
        __atomic_clear(&update_in_progress, __ATOMIC_RELEASE);
        ota_pending_on_wifi = false;
        
        // Store error message
        char error_msg[OTA_STATUS_MAX_LEN];
        snprintf(error_msg, OTA_STATUS_MAX_LEN, "OTA failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "%s", error_msg);
        ota_set_status(error_msg);
        ota_set_pending(false);  // Clear pending flag
        
        // Reboot after failure
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }
}

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

static void get_sha256_of_partitions(void)
{
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    // get sha256 digest for bootloader
    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");
}

static esp_err_t ota_post_handler(httpd_req_t *req) {
    if (__atomic_test_and_set(&update_in_progress, __ATOMIC_ACQUIRE)) {
        ESP_LOGW(TAG, "OTA update already in progress");
        httpd_resp_send_custom_err(req, "409", "Conflict: OTA update already in progress");
        return ESP_FAIL;
    }
    
    settings_t *settings = (settings_t *)req->user_ctx;
    
    // Set pending flag in NVS
    esp_err_t err = ota_set_pending(true);
    if (err != ESP_OK) {
        __atomic_clear(&update_in_progress, __ATOMIC_RELEASE);
        httpd_resp_send_custom_err(req, "500", "Internal Server Error: Failed to set OTA pending flag");
        return ESP_FAIL;
    }
    
    // Set status message
    ota_set_status("OTA update scheduled, rebooting...");
    
    // Send response before rebooting
    httpd_resp_sendstr(req, "OTA update scheduled. Device will reboot and update.");
    
    // Reboot after a short delay
    ESP_LOGI(TAG, "OTA update scheduled, rebooting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    
    return ESP_OK;
}

static httpd_uri_t ota_post_uri = {
    .uri       = "/ota",
    .method    = HTTP_POST,
    .handler   = ota_post_handler,
    .user_ctx  = NULL  // Will be set during initialization
};


esp_err_t ota_init(settings_t *settings, httpd_handle_t http_server)
{
    ESP_LOGI(TAG, "OTA init start");
    ota_post_uri.user_ctx = settings;
    ota_settings = settings;
    get_sha256_of_partitions();
    
    // Load last status from NVS
    ota_load_status();
    
    esp_err_t err = httpd_register_uri_handler_with_basic_auth(settings, http_server, &ota_post_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) registering settings GET handler!", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

// Check if OTA update is pending and prepare for it
esp_err_t ota_check_pending_update(settings_t *settings)
{
    ota_settings = settings;
    
    if (ota_get_pending()) {
        ESP_LOGI(TAG, "OTA update is pending. Will execute after WiFi connection.");
        ota_pending_on_wifi = true;
        return ESP_OK;
    }
    
    return ESP_ERR_NOT_FOUND;
}

// Called when WiFi connects - triggers OTA if pending
void ota_trigger_update_on_wifi_connect(void)
{
    if (!ota_pending_on_wifi || !ota_settings) {
        return;
    }
    
    ESP_LOGI(TAG, "WiFi connected, starting OTA update task");
    
    if (__atomic_test_and_set(&update_in_progress, __ATOMIC_ACQUIRE)) {
        ESP_LOGW(TAG, "OTA update already in progress");
        return;
    }
    
    // Create OTA task
    BaseType_t ret = xTaskCreate(&ota_task, "ota_task", 8192, ota_settings, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        __atomic_clear(&update_in_progress, __ATOMIC_RELEASE);
        ota_pending_on_wifi = false;
        
        // Store error and reboot
        ota_set_status("OTA failed: Could not create task");
        ota_set_pending(false);
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }
}
