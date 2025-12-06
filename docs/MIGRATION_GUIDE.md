# Migration Guide: Converting to the Sensor System

## Quick Start

### Before (Old Weight-Specific Code)
```c
// weight.c - directly exposing weight values
float g_latest_weight_grams = 0;
bool g_weight_available = false;

void update_weight() {
    g_latest_weight_grams = calculate_weight();
    g_weight_available = true;
}
```

### After (New Sensor System)
```c
// your_sensor.c
#include "sensors.h"

static int sensor_id = -1;

void your_sensor_init() {
    sensor_id = sensors_register("Weight", "g");
}

void update_weight() {
    float weight = calculate_weight();
    sensors_update(sensor_id, weight, true);
}
```

## Step-by-Step Migration

### Step 1: Initialize Sensors in main.c
```c
// main.c
#include "sensors.h"

void app_main(void) {
    // ... existing initialization ...
    
    httpd_handle_t http_server = http_server_init();
    
    // Add this line BEFORE initializing sensor modules
    sensors_init(settings, http_server);
    
    // Now initialize your sensor modules
    weight_init(settings, http_server);
    temperature_init(settings, http_server);
    // ... etc
}
```

### Step 2: Register Your Sensors
```c
// your_module.c
#include "sensors.h"

// Static variables to store sensor IDs
static int temp_sensor_id = -1;
static int humidity_sensor_id = -1;

void your_module_init(settings_t *settings, httpd_handle_t server) {
    // Register sensors with name and unit
    temp_sensor_id = sensors_register("Room Temperature", "°C");
    humidity_sensor_id = sensors_register("Humidity", "%");
    
    // Check for errors
    if (temp_sensor_id < 0) {
        ESP_LOGE(TAG, "Failed to register temperature sensor");
    }
    
    // Continue with module initialization...
}
```

### Step 3: Update Sensor Values
```c
// In your sensor reading loop/callback
void sensor_reading_task(void *pvParameters) {
    while (1) {
        float temperature = read_temperature_sensor();
        float humidity = read_humidity_sensor();
        
        // Update sensors (value, availability)
        sensors_update(temp_sensor_id, temperature, true);
        sensors_update(humidity_sensor_id, humidity, true);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### Step 4: Handle Unavailable Sensors
```c
// When sensor is disconnected or has an error
void sensor_error_handler() {
    // Mark sensor as unavailable
    sensors_update(temp_sensor_id, 0.0, false);
    
    // The display will show the sensor as grayed out
}
```

## Common Patterns

### Pattern 1: Multiple Sensors from One Source
```c
// HX711 weight sensor with multiple units
static int sensor_id_grams = -1;
static int sensor_id_lbs = -1;
static int sensor_id_oz = -1;

void weight_init() {
    sensor_id_grams = sensors_register("Weight", "g");
    sensor_id_lbs = sensors_register("Weight", "lbs");
    sensor_id_oz = sensors_register("Weight", "oz");
}

void update_all_weight_sensors(float grams) {
    sensors_update(sensor_id_grams, grams, true);
    sensors_update(sensor_id_lbs, grams / 453.59237, true);
    sensors_update(sensor_id_oz, grams / 28.3495, true);
}
```

### Pattern 2: Dynamic Sensor Registration (BTHome)
```c
// Track registered sensors
typedef struct {
    esp_bd_addr_t mac;
    uint8_t type;
    int sensor_id;
} sensor_mapping_t;

static sensor_mapping_t mappings[20];
static int mapping_count = 0;

int find_or_register_sensor(esp_bd_addr_t mac, uint8_t type, 
                             const char *name, const char *unit) {
    // Check if already registered
    for (int i = 0; i < mapping_count; i++) {
        if (memcmp(mappings[i].mac, mac, 6) == 0 && 
            mappings[i].type == type) {
            return mappings[i].sensor_id;
        }
    }
    
    // Register new sensor
    int id = sensors_register(name, unit);
    if (id >= 0 && mapping_count < 20) {
        memcpy(mappings[mapping_count].mac, mac, 6);
        mappings[mapping_count].type = type;
        mappings[mapping_count].sensor_id = id;
        mapping_count++;
    }
    return id;
}
```

### Pattern 3: Sensor with Calculated Values
```c
// Power calculation from voltage and current
static int voltage_id = -1;
static int current_id = -1;
static int power_id = -1;

void power_monitor_init() {
    voltage_id = sensors_register("Voltage", "V");
    current_id = sensors_register("Current", "A");
    power_id = sensors_register("Power", "W");
}

void update_power_sensors() {
    float voltage = read_voltage();
    float current = read_current();
    float power = voltage * current;
    
    sensors_update(voltage_id, voltage, true);
    sensors_update(current_id, current, true);
    sensors_update(power_id, power, true);
}
```

## Error Handling

### Check Registration Success
```c
int sensor_id = sensors_register("Temperature", "°C");
if (sensor_id < 0) {
    ESP_LOGE(TAG, "Failed to register sensor - max sensors reached?");
    // Handle error - maybe disable this feature
    return ESP_FAIL;
}
```

### Check Update Success
```c
bool success = sensors_update(sensor_id, value, true);
if (!success) {
    ESP_LOGE(TAG, "Failed to update sensor %d", sensor_id);
    // Handle error
}
```

### Validate Sensor ID Before Use
```c
void update_if_valid(int sensor_id, float value) {
    if (sensor_id >= 0) {
        sensors_update(sensor_id, value, true);
    }
}
```

## Configuration

### Increase Maximum Sensors
If you need more than 10 sensors, edit `main/sensors.h`:
```c
#define MAX_SENSORS 20  // Increase from 10 to 20
```

### Longer Sensor Names
```c
#define SENSOR_NAME_MAX_LEN 64  // Increase from 32 to 64
```

### Longer Unit Strings
```c
#define SENSOR_UNIT_MAX_LEN 32  // Increase from 16 to 32
```

## Testing

### Verify Sensors Appear
1. Build and flash the firmware
2. Open browser to device IP address
3. Verify all registered sensors appear in the grid
4. Check that values update in real-time

### Test Unavailable State
```c
// In your test code
sensors_update(sensor_id, 0.0, false);
// Sensor should appear grayed out on the web page
```

### Debug Sensor Registration
```c
ESP_LOGI(TAG, "Registered sensor ID %d: %s (%s)", 
         sensor_id, "Temperature", "°C");
```

## Troubleshooting

### Sensor Not Appearing
- Check that `sensors_init()` is called before your module's init
- Verify sensor registration returned valid ID (>= 0)
- Check that `sensors_update()` is being called

### Values Not Updating
- Verify `sensors_update()` is called regularly
- Check that correct sensor_id is being used
- Ensure `available` parameter is `true`

### Max Sensors Reached
- Increase `MAX_SENSORS` in `sensors.h`
- Or reduce number of registered sensors
- Check for duplicate registrations

## Example: Complete Module

```c
// temperature.c
#include "sensors.h"
#include <esp_log.h>

static const char *TAG = "temperature";
static int sensor_id_celsius = -1;
static int sensor_id_fahrenheit = -1;

void temperature_init(settings_t *settings, httpd_handle_t server) {
    // Register sensors
    sensor_id_celsius = sensors_register("Temperature", "°C");
    sensor_id_fahrenheit = sensors_register("Temperature", "°F");
    
    if (sensor_id_celsius < 0 || sensor_id_fahrenheit < 0) {
        ESP_LOGE(TAG, "Failed to register temperature sensors");
        return;
    }
    
    // Start reading task
    xTaskCreate(temperature_task, "temp", 2048, NULL, 5, NULL);
}

static void temperature_task(void *arg) {
    while (1) {
        float celsius = read_temperature_celsius();
        bool available = (celsius != -999.0);  // Error value
        
        if (available) {
            float fahrenheit = celsius * 9.0 / 5.0 + 32.0;
            sensors_update(sensor_id_celsius, celsius, true);
            sensors_update(sensor_id_fahrenheit, fahrenheit, true);
        } else {
            sensors_update(sensor_id_celsius, 0.0, false);
            sensors_update(sensor_id_fahrenheit, 0.0, false);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```
