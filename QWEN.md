# CynoIOT Arduino Library - Project Context

## Project Overview

This is an Arduino library for the CynoIoT.com platform, designed to enable IoT devices (primarily ESP8266 and ESP32) to connect to the CynoIoT cloud service for remote monitoring and control. The library handles MQTT communication, device provisioning, sensor data publishing, and remote control capabilities.

### Key Features
- Automatic device identification using MAC addresses
- Secure MQTT communication with TLS
- Sensor data publishing to the cloud
- Remote GPIO control
- Event handling system
- Automation rule processing
- OTA (Over-The-Air) firmware updates
- Real-time clock synchronization
- Template-based dashboard configuration

### Supported Platforms
- ESP8266
- ESP32

### Dependencies
- MQTT library
- WiFi libraries (ESP8266WiFi or WiFi)
- ESP-specific HTTP and OTA update libraries

## Library API

### Core Class
```cpp
Cynoiot iot;
```

### Connection Methods
- `bool connect(String email)` - Connect to CynoIoT with email
- `bool connect(const char email[])` - Connect with char array email
- `bool connect(const char server[], const char email[])` - Connect to specific server
- `bool status()` - Check connection status
- `String getClientId()` - Get device's unique identifier

### Main Loop
- `void handle()` - Process MQTT messages and maintain connection (must be called in loop())

### Data Publishing
- `void setkeyname(String keyname[], uint8_t numElements)` - Set sensor variable names
- `void update(float val[])` - Publish sensor data to cloud

### Templates
- `void setTemplate(String templateName)` - Set dashboard template
- `void setTemplate(String templateName, uint8_t version)` - Set template with version

### Events
- `void setEventCallback(EventCallbackFunction callback)` - Set callback for server events
- `void triggerEvent(String event, String value)` - Manually trigger event callback
- `void eventUpdate(String event, String value)` - Send event to server
- `void eventUpdate(String event, int value)` - Send event to server (int value)

### GPIO Control
- `void gpioUpdate(int pin, String value)` - Send GPIO status to server
- `void gpioUpdate(int pin, int value)` - Send GPIO status to server
- `void gpioUpdate(String pin, int value)` - Send GPIO status to server

### Time Functions
- `uint32_t getTime()` - Get synchronized timestamp
- `uint32_t getDaytimestamps()` - Get seconds since midnight
- `uint8_t getDayofWeek()` - Get day of week (0-6)
- `uint8_t getHour()` - Get current hour
- `uint8_t getMinute()` - Get current minute
- `uint8_t getSecond()` - Get current second
- `void printTimeDetails()` - Print formatted time to Serial

### Debugging
- `void debug(String msg)` - Send debug message to server

## Usage Pattern

### Basic Setup
1. Include required headers:
   ```cpp
   #ifdef ESP8266
   #include <ESP8266WiFi.h>
   #elif defined(ESP32)
   #include <WiFi.h>
   #endif
   #include <cynoiot.h>
   ```

2. Define WiFi credentials and user email:
   ```cpp
   const char ssid[] = "YOUR_WIFI_SSID";
   const char pass[] = "YOUR_WIFI_PASSWORD";
   const char email[] = "your@email.com";
   ```

3. Create CynoIOT instance:
   ```cpp
   Cynoiot iot;
   ```

4. Initialize in setup():
   ```cpp
   void setup() {
     // Connect to WiFi
     WiFi.begin(ssid, pass);
     while (WiFi.status() != WL_CONNECTED) {
       delay(1000);
     }
     
     // Configure CynoIOT
     uint8_t numVariables = 2;
     String keyname[numVariables] = {"temperature", "humidity"};
     iot.setkeyname(keyname, numVariables);
     iot.connect(email);
   }
   ```

5. Handle communication in loop():
   ```cpp
   void loop() {
     iot.handle();  // Must be called regularly
     
     // Periodically send sensor data
     float sensorData[] = {temperature, humidity};
     iot.update(sensorData);
   }
   ```

### Event Handling
```cpp
void handleEvent(String event, String value) {
  if (event == "led") {
    digitalWrite(LED_PIN, value.toInt());
  }
}

void setup() {
  iot.setEventCallback(handleEvent);
}
```

## Automation System

The library supports rule-based automation where sensor data changes can trigger GPIO actions or events. Rules are received from the server and processed automatically when `update()` is called.

### Rule Format
```
actionType:variable:operator:threshold:target:mode:value[:timeLimit][:timeoutValue]
```

### Example Rules
- `gpio:temperature:>:30:2:digit:1` - Turn on pin 2 when temperature > 30°C
- `event:humidity:<:40:low_humidity::alert` - Send "low_humidity" event when humidity < 40%

## Directory Structure
```
CynoIOT/
├── cynoiot.cpp          # Main implementation
├── cynoiot.h            # Header file with API
├── library.properties   # Library metadata
├── README.md            # Basic project info
├── keywords.txt         # Syntax highlighting definitions
├── docs/
│   └── AUTOMATION.md    # Automation system documentation
└── examples/
    ├── simple/          # Basic usage example
    ├── event/           # Event handling example
    ├── sensor/          # Sensor integration example
    ├── gpio_control/    # GPIO control example
    ├── debug/           # Debugging example
    ├── kit/             # Complete kit examples
    ├── relay_2ch_kit/   # 2-channel relay kit example
    └── AutomationExample/ # Automation examples
```

## Development Guidelines

### Coding Style
- C++ with Arduino framework
- Platform-specific code wrapped in `#ifdef ESP8266`/`#elif defined(ESP32)`
- Debug output using custom macros when `CYNOIOT_DEBUG` is defined

### Key Implementation Details
- Uses MQTT over secure WebSocket for communication
- Implements automatic reconnection logic
- Stores OTA update status in EEPROM
- Processes automation rules in the `update()` method
- Handles time synchronization with the server
- Supports template-based dashboard configuration

## Testing and Debugging

Enable debugging by ensuring `CYNOIOT_DEBUG` is defined:
```cpp
#define CYNOIOT_DEBUG
```

Monitor Serial output for connection status, data publishing confirmations, and automation triggers.