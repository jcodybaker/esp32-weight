#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bthome.h"
#include "bthome_ble.h"

static const char *TAG = "bthome_observer";

// Callback function that gets called when a BTHome packet is received
static void bthome_packet_callback(esp_bd_addr_t addr, int rssi, 
                                    const bthome_packet_t *packet, void *user_data) {
    // Format MAC address
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    
    ESP_LOGI(TAG, "BTHome packet from %s (RSSI: %d dBm)", mac_str, rssi);
    
    // Print device name if present
    if (packet->device_name != NULL && packet->device_name_len > 0) {
        // Create a temporary buffer to null-terminate the name for printing
        char name_buffer[256];
        size_t copy_len = packet->device_name_len < sizeof(name_buffer) - 1 ? 
                         packet->device_name_len : sizeof(name_buffer) - 1;
        memcpy(name_buffer, packet->device_name, copy_len);
        name_buffer[copy_len] = '\0';
        
        ESP_LOGI(TAG, "  Device Name: \"%s\" (%s)", name_buffer, 
                 packet->use_complete_name ? "Complete" : "Shortened");
    }
    
    ESP_LOGI(TAG, "  Version: %d, Encrypted: %d, Trigger-based: %d",
             packet->device_info.version,
             packet->device_info.encrypted,
             packet->device_info.trigger_based);
    
    if (packet->has_packet_id) {
        ESP_LOGI(TAG, "  Packet ID: %d", packet->packet_id);
    }
    
    // Print all measurements
    for (size_t i = 0; i < packet->measurement_count; i++) {
        const bthome_measurement_t *m = &packet->measurements[i];
        float factor = bthome_get_scaling_factor(m->object_id);
        float value = bthome_get_scaled_value(m, factor);
        
        ESP_LOGI(TAG, "  Measurement 0x%02X: %.2f", m->object_id, value);
        
        // Specific sensor type examples
        switch (m->object_id) {
            case BTHOME_SENSOR_TEMPERATURE:
                ESP_LOGI(TAG, "    Temperature: %.2f °C", value);
                break;
            case BTHOME_SENSOR_HUMIDITY:
                ESP_LOGI(TAG, "    Humidity: %.2f %%", value);
                break;
            case BTHOME_SENSOR_BATTERY:
                ESP_LOGI(TAG, "    Battery: %d %%", (int)value);
                break;
            case BTHOME_SENSOR_PRESSURE:
                ESP_LOGI(TAG, "    Pressure: %.2f hPa", value);
                break;
            case BTHOME_SENSOR_ILLUMINANCE:
                ESP_LOGI(TAG, "    Illuminance: %.2f lux", value);
                break;
            case BTHOME_SENSOR_DISTANCE_MM:
                ESP_LOGI(TAG, "    Distance: %.2f mm", value);
                break;
            case BTHOME_BINARY_VIBRATION:
                ESP_LOGI(TAG, "    Vibration: %s", value ? "Detected" : "Not Detected");
                break;
            default:
                break;
        }
    }
    
    // Print all events
    for (size_t i = 0; i < packet->event_count; i++) {
        const bthome_event_t *e = &packet->events[i];
        ESP_LOGI(TAG, "  Event 0x%02X: value=%d, steps=%d", 
                 e->event_type, e->event_value, e->steps);
        
        if (e->event_type == BTHOME_EVENT_BUTTON) {
            const char *event_str = "Unknown";
            switch (e->event_value) {
                case BTHOME_BUTTON_PRESS: event_str = "Press"; break;
                case BTHOME_BUTTON_DOUBLE_PRESS: event_str = "Double Press"; break;
                case BTHOME_BUTTON_TRIPLE_PRESS: event_str = "Triple Press"; break;
                case BTHOME_BUTTON_LONG_PRESS: event_str = "Long Press"; break;
                case BTHOME_BUTTON_HOLD_PRESS: event_str = "Hold Press"; break;
            }
            ESP_LOGI(TAG, "    Button Event: %s", event_str);
        }
    }
}

void bthome_observer_init(void) {
    // Initialize NVS (required for BLE)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "Starting BTHome BLE Scanner");
    
    // Initialize the BLE scanner
    ret = bthome_ble_scanner_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BLE scanner: %s", esp_err_to_name(ret));
        return;
    }
    
    // Configure scanner
    bthome_ble_scanner_config_t config;
    bthome_ble_scanner_get_default_config(&config);
    
    // Set callback function
    config.callback = bthome_packet_callback;
    config.user_data = NULL;
    
    // Use passive scanning (lower power)
    config.scan_type = BLE_SCAN_TYPE_PASSIVE;
    
    // Scan interval and window (in units of 0.625ms)
    config.scan_interval = 0x50;  // 50ms
    config.scan_window = 0x30;    // 30ms
    
    // Start continuous scanning (scan_duration = 0)
    config.scan_duration = 0;
    
    // Start scanning
    ret = bthome_ble_scanner_start(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE scanner: %s", esp_err_to_name(ret));
        bthome_ble_scanner_deinit();
        return;
    }
    
    ESP_LOGI(TAG, "BLE scanner started, listening for BTHome advertisements...");

}
