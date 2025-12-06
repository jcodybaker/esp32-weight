# Quick Setup Guide: BTHome Sensors

## Prerequisites
- ESP32 device running the weight station firmware
- BTHome-compatible BLE devices broadcasting measurements
- Web browser to access the settings page

## Step-by-Step Setup

### 1. Find Your BTHome Device MAC Addresses

You can find MAC addresses in two ways:

**Option A: Check /bthome/packets page**
1. Navigate to `http://[device-ip]/bthome/packets`
2. You'll see all detected BTHome devices with their MAC addresses
3. Note down the MAC addresses you want to monitor

**Option B: Check device documentation**
- Most BTHome devices display or print their MAC address
- Some show it in their companion apps

### 2. Configure MAC Filters

1. Navigate to `http://[device-ip]/settings`
2. Scroll to the "BTHome MAC Address Filters" section
3. For each device you want to monitor:
   - Click "Add MAC Filter"
   - Enter MAC address (format: `AA:BB:CC:DD:EE:FF`)
   - Enter a friendly name (e.g., "Living Room", "Kitchen", "Bedroom")
   - Check the "Enabled" checkbox
4. Click "Save Settings"

Example configuration:
```
MAC Filter 1:
  MAC: AA:BB:CC:DD:EE:FF
  Name: Living Room
  ☑ Enabled

MAC Filter 2:
  MAC: 11:22:33:44:55:66
  Name: Kitchen
  ☑ Enabled
```

### 3. Select Measurement Types

1. In the same Settings page, scroll to "BTHome Object IDs"
2. Check the measurements you want to display:
   - ☑ Temperature (0x02)
   - ☑ Humidity (0x03)
   - ☑ Pressure (0x04)
   - ☑ Battery (0x01)
   - And others as needed
3. Click "Save Settings"

### 4. View Your Sensors

1. Navigate to the home page: `http://[device-ip]/`
2. You should now see your sensors displayed:
   - Living Room Temperature: 23.5 °C
   - Living Room Humidity: 65 %
   - Kitchen Temperature: 21.0 °C
   - Kitchen Humidity: 58 %

The page auto-refreshes every second with live data!

## Common Object IDs

Here are the most common BTHome measurement types:

| Object ID | Measurement    | Unit  |
|-----------|----------------|-------|
| 0x01      | Battery        | %     |
| 0x02      | Temperature    | °C    |
| 0x03      | Humidity       | %     |
| 0x04      | Pressure       | hPa   |
| 0x05      | Illuminance    | lux   |
| 0x06      | Weight         | kg    |
| 0x0D      | Distance       | mm    |
| 0x14      | PM2.5          | µg/m³ |
| 0x15      | PM10           | µg/m³ |
| 0x40      | Distance       | mm    |
| 0x45      | Count          | -     |

## Troubleshooting

### Sensors Not Appearing

**Check 1: Is NTP synchronized?**
- BTHome integration requires NTP time synchronization
- Wait a few minutes after device boot for NTP to sync
- Check device logs for "NTP synchronized" message

**Check 2: Is the MAC filter enabled?**
- Go to `/settings` and verify the checkbox is checked
- Verify the MAC address is correct (case-insensitive)

**Check 3: Are the object IDs selected?**
- Go to `/settings` and verify measurement types are checked
- Temperature is usually object ID 0x02

**Check 4: Is the device broadcasting?**
- Visit `/bthome/packets` to see all detected devices
- If your device isn't listed, check:
  - Device is powered on
  - Device is within BLE range
  - Device is configured to broadcast BTHome format

### Wrong Device Name

**Problem**: Sensor shows as "Temperature" instead of "Living Room Temperature"

**Solution**: 
- Verify the MAC address in settings exactly matches the device
- MAC addresses are case-insensitive but must include all colons
- Restart the ESP32 after changing names

### Too Many Sensors

**Problem**: Display is cluttered with many sensors

**Solution**: 
- Uncheck unnecessary measurement types in settings
- Disable MAC filters you don't need
- Only enabled filters with selected object IDs will appear

## Example Configurations

### Minimal Setup (Temperature Only)
```
MAC Filters:
  ☑ AA:BB:CC:DD:EE:FF - "Indoor"

Selected Object IDs:
  ☑ Temperature (0x02)
  
Result: Shows "Indoor Temperature"
```

### Full Environment Monitoring
```
MAC Filters:
  ☑ AA:BB:CC:DD:EE:FF - "Living Room"
  ☑ 11:22:33:44:55:66 - "Bedroom"
  ☑ 22:33:44:55:66:77 - "Outdoor"

Selected Object IDs:
  ☑ Temperature (0x02)
  ☑ Humidity (0x03)
  ☑ Pressure (0x04)
  
Result: Shows 9 sensors
  - Living Room Temperature
  - Living Room Humidity  
  - Living Room Pressure
  - Bedroom Temperature
  - Bedroom Humidity
  - Bedroom Pressure
  - Outdoor Temperature
  - Outdoor Humidity
  - Outdoor Pressure
```

### Weight + BTHome
```
Weight sensors (built-in):
  - Weight (g)
  - Weight (lbs)

MAC Filters:
  ☑ AA:BB:CC:DD:EE:FF - "Environment"

Selected Object IDs:
  ☑ Temperature (0x02)
  ☑ Humidity (0x03)
  
Result: Shows 4 sensors
  - Weight (g)
  - Weight (lbs)
  - Environment Temperature
  - Environment Humidity
```

## Tips

1. **Use descriptive names**: Instead of "Sensor 1", use location-based names like "Living Room" or "Bedroom"
2. **Start small**: Begin with one device and one measurement type, then expand
3. **Check /bthome/packets**: Use this page to debug and see all available devices
4. **Disable unused sensors**: Uncheck measurements you don't care about to reduce clutter
5. **Consider device placement**: BLE range is typically 10-30 meters indoors
