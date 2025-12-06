# BTHome Sensor Integration

## Overview

The sensor display system integrates with BTHome sensors based on your configuration in the Settings page. Only devices with enabled MAC filters and selected measurement types will be registered and displayed on the main sensor page.

## How It Works

1. **Configuration-Based Filtering**: Only BTHome devices with enabled MAC filters in settings are processed
2. **Measurement Selection**: Only selected measurement types (object IDs) configured in settings are displayed
3. **Custom Naming**: Sensors use the names you configured in the MAC filter settings
4. **Dynamic Registration**: Each unique sensor (MAC address + measurement type) is registered once when first detected
5. **Real-time Updates**: Sensor values are updated whenever new BTHome packets arrive
## Configuration

### Step 1: Add MAC Address Filters

In the Settings page (`/settings`), add BTHome devices you want to monitor:

1. Click "Add MAC Filter"
2. Enter the device's MAC address (e.g., `AA:BB:CC:DD:EE:FF`)
3. Enter a descriptive name (e.g., "Living Room")
4. Check "Enabled"
5. Click "Save Settings"

### Step 2: Select Measurement Types

In the same Settings page, select which measurements to display:

- ☑ Temperature
- ☑ Humidity
- ☑ Pressure
- ☐ Battery
- ☐ Illuminance
- And more...

Only measurements you select will appear on the sensor page.

## Features

### Custom Naming
- Sensors use the name from your MAC filter configuration
- Format: `[Device Name] [Measurement Type]`
- Example: "Living Room Temperature", "Bedroom Humidity"

### Selective Display
- Only enabled MAC addresses are monitored
- Only selected measurement types are shown
- Reduces clutter and focuses on what matters to you

### Supported Measurements
All BTHome measurement types can be selected, including:
- Temperature (°C)
- Humidity (%)
- Pressure (hPa)
- Battery (%)
- Illuminance (lux)
- Distance (mm)
- And many more...
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
### Registration Flow
1. BTHome packet arrives via BLE
2. Packet is cached (existing behavior)
3. For each measurement in packet:
   - Check if MAC address is in enabled filters → Skip if not
   - Check if object ID is in selected measurements → Skip if not
   - Check if sensor already registered (MAC + object_id)
   - If not registered, create new sensor with configured name + measurement type
   - Update sensor value
4. Sensor appears on main display page automaticallyme devices:
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
## Example Scenarios

### Scenario 1: Basic Setup

**Configuration in Settings:**
```
MAC Filters:
  ☑ AA:BB:CC:DD:EE:FF - "Kitchen"    [Enabled]
  ☑ 11:22:33:44:55:66 - "Living Room" [Enabled]

Selected Measurements:
  ☑ Temperature
  ☑ Humidity
  ☐ Battery (not selected)
```

**Result on Sensor Page:**
- Kitchen Temperature: 21.5 °C
- Kitchen Humidity: 58 %
- Living Room Temperature: 23.5 °C
- Living Room Humidity: 62 %

Note: Battery measurements are not shown because they're not selected.

### Scenario 2: Selective Monitoring

**Configuration in Settings:**
```
MAC Filters:
  ☑ AA:BB:CC:DD:EE:FF - "Bedroom"    [Enabled]
  ☐ 11:22:33:44:55:66 - "Garage"     [Disabled]

Selected Measurements:
  ☑ Temperature
```

**Result on Sensor Page:**
- Bedroom Temperature: 20.1 °C

Note: Garage sensor is not shown because its filter is disabled.
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
## Notes

### Configuration Required
BTHome sensors will **only** appear if:
1. Their MAC address is in the enabled filters list in Settings
2. Their measurement type is in the selected object IDs list in Settings

### NTP Requirement
BTHome sensor registration only occurs after NTP time is synchronized (existing requirement).

### Packet Caching
BTHome packets continue to be cached and accessible via `/bthome/packets` endpoint for detailed inspection (shows all devices, not just filtered ones).

### Name Changes
If you change a device name in Settings, you'll need to restart the device for the sensor display name to update.

### Performance
- Minimal overhead: MAC and object ID filtering is very fast
- Registration only happens once per unique sensor
- Updates are fast: just a value assignment
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
