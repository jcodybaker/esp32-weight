# Sensor Display System

## Overview

The sensor display system has been refactored to support displaying arbitrary sensor values from multiple sources. Each sensor can have:
- A descriptive name
- A unit string (e.g., "g", "°C", "lbs", "%")
- A floating-point value
- A last-updated timestamp
- An availability status

## API

### Registration

```c
#include "sensors.h"

// Register a new sensor
int sensor_id = sensors_register("Temperature", "°C");
```

### Updating Values

```c
// Update sensor value
bool success = sensors_update(sensor_id, 23.5, true);  // value, available
```

### Reading Values

```c
bool available;
float value = sensors_get_value(sensor_id, &available);
```

## Initialization

The sensor system must be initialized before any sensors are registered:

```c
void app_main(void) {
    // ... other initialization ...
    
    httpd_handle_t http_server = http_server_init();
    sensors_init(settings, http_server);  // Initialize sensor system first
    
    weight_init(settings, http_server);   // Can now register sensors
    // ... other modules ...
}
```

## Web Interface

The sensor display page is available at the root URL (`/`) and shows:
- All registered sensors in a responsive grid layout
- Each sensor's current value and unit
- Time since last update
- Visual indication of unavailable sensors (grayed out)

The page automatically refreshes every second to show live data.

## Example: Weight Sensors

The weight module now registers two sensors:

```c
void weight_init(settings_t *settings, httpd_handle_t server) {
    // Register weight sensors
    sensor_id_grams = sensors_register("Weight", "g");
    sensor_id_lbs = sensors_register("Weight", "lbs");
    
    // Start the weight reading task
    xTaskCreate(weight, "weight", configMINIMAL_STACK_SIZE * 5, settings, 5, NULL);
}
```

Then updates them in the reading loop:

```c
if (sensor_id_grams >= 0) {
    sensors_update(sensor_id_grams, g_latest_weight_grams, true);
}
if (sensor_id_lbs >= 0) {
    float lbs = g_latest_weight_grams / 453.59237f;
    sensors_update(sensor_id_lbs, lbs, true);
}
```

## Integration with BTHome

The BTHome observer can be updated to register sensors dynamically when new measurements are detected. Example:

```c
// In bthome_packet_callback
for (size_t i = 0; i < packet->measurement_count; i++) {
    const bthome_measurement_t *m = &packet->measurements[i];
    float factor = bthome_get_scaling_factor(m->object_id);
    float value = bthome_get_scaled_value(m, factor);
    const char *name = bthome_get_object_name(m->object_id);
    const char *unit = bthome_get_object_unit(m->object_id);
    
    // Create unique sensor name with MAC address
    char sensor_name[32];
    snprintf(sensor_name, sizeof(sensor_name), "%s (%s)", 
             name ? name : "Unknown", mac_str_short);
    
    // Register or update sensor
    int sensor_id = find_or_register_sensor(sensor_name, unit ? unit : "");
    sensors_update(sensor_id, value, true);
}
```

## Configuration

Maximum sensors and string lengths are defined in `sensors.h`:

```c
#define MAX_SENSORS 10              // Maximum number of sensors
#define SENSOR_NAME_MAX_LEN 32      // Maximum sensor name length
#define SENSOR_UNIT_MAX_LEN 16      // Maximum unit string length
```

## API Endpoints

### `/sensors/data`

Returns JSON with all sensor data:

```json
{
  "sensors": [
    {
      "name": "Weight",
      "unit": "g",
      "value": 1234.56,
      "last_updated": 1733456789,
      "available": true
    },
    {
      "name": "Temperature",
      "unit": "°C",
      "value": 23.5,
      "last_updated": 1733456790,
      "available": true
    }
  ]
}
```

### `/`

Main sensor display page with auto-refreshing grid of all sensors.

### `/version`

Firmware version information (unchanged from original implementation).
