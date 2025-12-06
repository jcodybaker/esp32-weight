// Example: Integrating BTHome measurements with the sensor system
//
// This shows how to dynamically register BTHome sensor measurements
// and display them on the main sensor page.

#include "sensors.h"
#include "bthome.h"

// Structure to track BTHome sensor registrations
typedef struct {
    esp_bd_addr_t addr;
    uint8_t object_id;
    int sensor_id;
    bool registered;
} bthome_sensor_mapping_t;

#define MAX_BTHOME_SENSORS 20
static bthome_sensor_mapping_t bthome_sensor_map[MAX_BTHOME_SENSORS];
static int bthome_sensor_count = 0;

// Find or create a sensor registration for a BTHome measurement
static int find_or_register_bthome_sensor(esp_bd_addr_t addr, 
                                           uint8_t object_id,
                                           const char *name,
                                           const char *unit) {
    // First, check if we already have this sensor registered
    for (int i = 0; i < bthome_sensor_count; i++) {
        if (memcmp(bthome_sensor_map[i].addr, addr, 6) == 0 &&
            bthome_sensor_map[i].object_id == object_id) {
            return bthome_sensor_map[i].sensor_id;
        }
    }
    
    // Not found, register a new sensor
    if (bthome_sensor_count >= MAX_BTHOME_SENSORS) {
        ESP_LOGE(TAG, "Maximum BTHome sensors reached");
        return -1;
    }
    
    // Create a unique sensor name with MAC address suffix
    char sensor_name[SENSOR_NAME_MAX_LEN];
    snprintf(sensor_name, sizeof(sensor_name), "%s %02X%02X", 
             name ? name : "Unknown",
             addr[4], addr[5]);  // Last 2 bytes of MAC for uniqueness
    
    // Register the sensor
    int sensor_id = sensors_register(sensor_name, unit ? unit : "");
    if (sensor_id < 0) {
        return -1;
    }
    
    // Store the mapping
    memcpy(bthome_sensor_map[bthome_sensor_count].addr, addr, 6);
    bthome_sensor_map[bthome_sensor_count].object_id = object_id;
    bthome_sensor_map[bthome_sensor_count].sensor_id = sensor_id;
    bthome_sensor_map[bthome_sensor_count].registered = true;
    bthome_sensor_count++;
    
    return sensor_id;
}

// Modify the existing bthome_packet_callback to integrate with sensors:
static void bthome_packet_callback_with_sensors(esp_bd_addr_t addr, int rssi, 
                                                 const bthome_packet_t *packet, 
                                                 void *user_data) {
    // ... existing caching code ...
    
    // Process all measurements and update sensor system
    for (size_t i = 0; i < packet->measurement_count; i++) {
        const bthome_measurement_t *m = &packet->measurements[i];
        float factor = bthome_get_scaling_factor(m->object_id);
        float value = bthome_get_scaled_value(m, factor);
        const char *name = bthome_get_object_name(m->object_id);
        const char *unit = bthome_get_object_unit(m->object_id);
        
        // Find or register this sensor
        int sensor_id = find_or_register_bthome_sensor(addr, m->object_id, name, unit);
        if (sensor_id >= 0) {
            // Update the sensor value
            sensors_update(sensor_id, value, true);
        }
    }
}

// Alternative: Register only specific sensor types
static void register_specific_bthome_sensors(esp_bd_addr_t addr,
                                              const bthome_packet_t *packet) {
    for (size_t i = 0; i < packet->measurement_count; i++) {
        const bthome_measurement_t *m = &packet->measurements[i];
        
        // Only register temperature, humidity, and pressure sensors
        if (m->object_id == BTHOME_SENSOR_TEMPERATURE ||
            m->object_id == BTHOME_SENSOR_HUMIDITY ||
            m->object_id == BTHOME_SENSOR_PRESSURE) {
            
            float factor = bthome_get_scaling_factor(m->object_id);
            float value = bthome_get_scaled_value(m, factor);
            const char *name = bthome_get_object_name(m->object_id);
            const char *unit = bthome_get_object_unit(m->object_id);
            
            int sensor_id = find_or_register_bthome_sensor(addr, m->object_id, name, unit);
            if (sensor_id >= 0) {
                sensors_update(sensor_id, value, true);
            }
        }
    }
}

// Alternative: Use device name in sensor registration
static void register_bthome_sensor_with_device_name(esp_bd_addr_t addr,
                                                     const bthome_packet_t *packet) {
    // Get device name or create default
    char device_name[32] = "Unknown";
    if (packet->device_name != NULL && packet->device_name_len > 0) {
        size_t copy_len = packet->device_name_len < sizeof(device_name) - 1 ? 
                         packet->device_name_len : sizeof(device_name) - 1;
        memcpy(device_name, packet->device_name, copy_len);
        device_name[copy_len] = '\0';
    }
    
    for (size_t i = 0; i < packet->measurement_count; i++) {
        const bthome_measurement_t *m = &packet->measurements[i];
        float factor = bthome_get_scaling_factor(m->object_id);
        float value = bthome_get_scaled_value(m, factor);
        const char *type = bthome_get_object_name(m->object_id);
        const char *unit = bthome_get_object_unit(m->object_id);
        
        // Create sensor name like "Living Room Temp" or "Unknown Humidity"
        char sensor_name[SENSOR_NAME_MAX_LEN];
        snprintf(sensor_name, sizeof(sensor_name), "%s %s", device_name, type ? type : "?");
        
        int sensor_id = find_or_register_bthome_sensor(addr, m->object_id, sensor_name, unit);
        if (sensor_id >= 0) {
            sensors_update(sensor_id, value, true);
        }
    }
}
