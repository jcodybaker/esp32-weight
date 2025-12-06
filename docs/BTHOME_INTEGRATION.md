# BTHome Sensor Integration

## Overview

The sensor display system now automatically discovers and displays BTHome sensors. When a BTHome device broadcasts measurements, they are automatically registered and displayed on the main sensor page.

## How It Works

1. **Automatic Discovery**: When BTHome packets are received, the system extracts all measurements
2. **Dynamic Registration**: Each unique sensor (identified by MAC address + measurement type) is registered once
3. **Real-time Updates**: Sensor values are updated whenever new BTHome packets arrive
4. **Named Display**: Sensors use device names when available, or MAC address suffix for identification

## Features

### Smart Naming
- Uses BTHome device name if available (e.g., "Living Room Temperature")
- Falls back to measurement type + MAC suffix (e.g., "Temperature A3B4")
- Automatically includes proper units from BTHome specification

### Supported Measurements
All BTHome measurement types are supported, including:
- Temperature (°C)
- Humidity (%)
- Pressure (hPa)
- Battery (%)
- Illuminance (lux)
- Distance (mm)
- And many more...

## Example Output

### Sensor Display Page
The main page (http://device-ip/) will show:

```
┌─────────────────────────┐  ┌─────────────────────────┐
│ Weight                  │  │ Weight                  │
│        1,234.56         │  │          2.72           │
│           g             │  │          lbs            │
│        2s ago           │  │        2s ago           │
└─────────────────────────┘  └─────────────────────────┘

┌─────────────────────────┐  ┌─────────────────────────┐
│ Living Room Temperature │  │ Living Room Humidity    │
│         23.5            │  │         65.2            │
│          °C             │  │           %             │
│        3s ago           │  │        3s ago           │
└─────────────────────────┘  └─────────────────────────┘

┌─────────────────────────┐  ┌─────────────────────────┐
│ Outdoor Temperature     │  │ Outdoor Humidity        │
│         18.3            │  │         72.1            │
│          °C             │  │           %             │
│        5s ago           │  │        5s ago           │
└─────────────────────────┘  └─────────────────────────┘
```

## Configuration

### Maximum Sensors
Increased from 10 to 60 to accommodate multiple BTHome devices:
```c
#define MAX_SENSORS 60  // In sensors.h
```

### Maximum BTHome Sensor Mappings
```c
#define MAX_BTHOME_SENSORS 50  // In bthome_observer.c
```

## Technical Details

### Sensor Mapping
The system maintains a mapping between BTHome devices and registered sensors:
```c
typedef struct {
    esp_bd_addr_t addr;      // Device MAC address
    uint8_t object_id;        // BTHome measurement type ID
    int sensor_id;            // Registered sensor ID
    bool registered;          // Registration status
} bthome_sensor_mapping_t;
```

### Registration Flow
1. BTHome packet arrives via BLE
2. Packet is cached (existing behavior)
3. For each measurement in packet:
   - Check if sensor already registered (MAC + object_id)
   - If not, register new sensor with appropriate name and unit
   - Update sensor value
4. Sensor appears on main display page automatically

### Thread Safety
- Sensor map protected by `sensor_map_mutex`
- Safe concurrent access from BLE callback and web handlers

## Implementation Files Modified

### `bthome_observer.c`
- Added BTHome sensor mapping structure and tracking
- Added `find_or_register_bthome_sensor()` function
- Modified `bthome_packet_callback()` to register and update sensors
- Added sensor map mutex initialization

### `sensors.h`
- Increased `MAX_SENSORS` from 10 to 60

## Example Scenarios

### Single BTHome Device
```
Device: "Kitchen Sensor" (MAC: AA:BB:CC:DD:EE:FF)
Measurements:
  - Temperature: 21.5°C
  - Humidity: 58%
  - Battery: 95%

Registered Sensors:
  - "Kitchen Sensor Temperature" (°C)
  - "Kitchen Sensor Humidity" (%)
  - "Kitchen Sensor Battery" (%)
```

### Multiple BTHome Devices
```
Device 1: "Living Room" (MAC: 11:22:33:44:55:66)
  - Temperature: 23.5°C
  - Humidity: 62%

Device 2: "Bedroom" (MAC: AA:BB:CC:DD:EE:FF)
  - Temperature: 20.1°C
  - Humidity: 55%

Device 3: No name (MAC: 99:88:77:66:55:44)
  - Temperature: 18.3°C (registered as "Temperature 5544")
```

### BTHome + Weight Sensors
All sensors appear together on the same page:
```
- Weight (g)
- Weight (lbs)
- Living Room Temperature (°C)
- Living Room Humidity (%)
- Bedroom Temperature (°C)
- Bedroom Humidity (%)
```

## Notes

### NTP Requirement
BTHome sensor registration only occurs after NTP time is synchronized (existing requirement).

### Packet Caching
BTHome packets continue to be cached and accessible via `/bthome/packets` endpoint for detailed inspection.

### Automatic Cleanup
Currently, once a sensor is registered, it remains in the system. Future enhancement could add automatic cleanup of sensors that haven't been updated recently.

### Performance
- Minimal overhead: sensor lookup is O(n) but typically n < 50
- Registration only happens once per unique sensor
- Updates are fast: just a value assignment

## Future Enhancements

Possible improvements:
1. Add sensor expiration/cleanup for inactive sensors
2. Configurable sensor name templates
3. Option to filter which BTHome measurements to display
4. Sensor grouping by device on the web page
5. Historical graphs for sensor values
