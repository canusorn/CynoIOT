/* Smart Farming On/Off Control
 * Monitors soil sensors (humidity, temp, EC, pH, N, P, K) via RS485/Modbus
 * Controls a single water pump with auto/manual irrigation
 * Web config, OLED display, CynoIOT cloud integration
 * ESP8266/ESP32
 */
/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

// ==========================================================================================
// SENSOR MODEL SELECTION
// ==========================================================================================
// Uncomment the sensor model that matches your hardware configuration.
// Only one model should be defined at a time.

// #define NOSENSOR_MODEL          // No sensors connected - basic pump control only
// #define HUMID_MODEL             // Humidity sensor only
// #define TEMP_HUMID_MODEL        // Temperature and humidity sensors
// #define TEMP_HUMID_EC_MODEL     // Temperature, humidity, and EC (conductivity) sensors
#define ALL_7IN1_MODEL            // Full 7-in-1 sensor: Humidity, Temperature, EC, pH, N, P, K

// ==========================================================================================
// LIBRARY INCLUDES
// ==========================================================================================

// WiFi and network libraries for ESP8266
#ifdef ESP8266
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>

// WiFi and network libraries for ESP32
#elif defined(ESP32)
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <WiFi.h>
#endif

#include <EEPROM.h> // EEPROM library for storing configuration data
#include <Wire.h>   // Wire library for I2C communication with OLED display

// CynoIOT and Web Configuration libraries
// IoTWebConf from https://github.com/canusorn/IotWebConf-iotbundle
#include <IotWebConf.h>
#include <IotWebConfUsing.h>

// Display libraries
#include <Adafruit_GFX.h>     // Adafruit GFX library for graphics primitives
#include <Adafruit_SSD1306.h> // Adafruit SSD1306 library for OLED display control

// IoT and Sensor libraries
#include <cynoiot.h>          // CynoIOT library for cloud platform integration
#include <ModbusMaster.h>     // ModbusMaster library for RS485/Modbus communication

// ==========================================================================================
// RS485 SERIAL COMMUNICATION SETUP
// ==========================================================================================
// Software serial for ESP8266, hardware serial for ESP32
#ifdef ESP8266
SoftwareSerial RS485Serial;  // Software serial instance for RS485 communication
#elif defined(ESP32)
#define RS485Serial Serial1  // Hardware serial instance for RS485 communication
#endif

// ==========================================================================================
// PIN DEFINITIONS - ESP8266
// ==========================================================================================
// MAX485 RS485 Transceiver pins
#ifdef ESP8266
#define MAX485_RO D7 // Receiver Output - Connect to MAX485 RO pin
#define MAX485_RE D6 // Receiver Enable - Connect to MAX485 RE pin
#define MAX485_DE D6 // Driver Enable - Connect to MAX485 DE pin (shared with RE)
#define MAX485_DI D0 // Driver Input - Connect to MAX485 DI pin (TX)

// Output control pin for pump
#define PUMP D1      // Water pump control pin
#define RSTPIN D8    // Reset pin for EEPROM clear function

// ==========================================================================================
// PIN DEFINITIONS - ESP32
// ==========================================================================================
#elif defined(ESP32)
#define RSTPIN 8

// ESP32-S2 specific pin configuration
#ifdef CONFIG_IDF_TARGET_ESP32S2
#define MAX485_RO 18  // Receiver Output
#define MAX485_RE 9   // Receiver Enable
#define MAX485_DE 9   // Driver Enable (shared with RE)
#define MAX485_DI 21  // Driver Input

// Standard ESP32 pin configuration
#else
#define MAX485_RO 23  // Receiver Output
#define MAX485_RE 9   // Receiver Enable
#define MAX485_DE 9   // Driver Enable (shared with RE)
#define MAX485_DI 26  // Driver Input
#endif

// Output control pin for pump
#define PUMP 3        // Water pump control pin

#endif

// ==========================================================================================
// ACTIVE LEVEL CONFIGURATION - Pump
// ==========================================================================================
// Uncomment to use active-low pump control (default).
// Comment out for active-high pump control.
#define PUMP_ACTIVE_LOW

#ifdef PUMP_ACTIVE_LOW
  #define PUMP_ON  LOW
  #define PUMP_OFF HIGH
#else
  #define PUMP_ON  HIGH
  #define PUMP_OFF LOW
#endif

// ==========================================================================================
// SYSTEM CONFIGURATION
// ==========================================================================================

// WiFi and Device Configuration
const char thingName[] = "WaterControl";          // Device name for WiFi AP mode
const char wifiInitialApPassword[] = "iotbundle"; // Default password for AP mode

#define ADDRESS 1           // Modbus slave address of the soil sensor (default: 1)

#define STRING_LEN 128      // Maximum length for string parameters (e.g., email, SSID)
#define NUMBER_LEN 32       // Maximum length for numeric parameters

// Communication Objects
ModbusMaster node;          // Modbus master object for RS485 sensor communication
Cynoiot iot;               // CynoIOT object for cloud platform integration

// Display Configuration
#define OLED_RESET -1       // Reset pin for OLED display (-1 = use Arduino reset)
Adafruit_SSD1306 oled(OLED_RESET);  // OLED display object (128x64 pixels, I2C)

// ==========================================================================================
// WEB INTERFACE HTML TEMPLATE
// ==========================================================================================
// HTML template for the web configuration interface
// Stored in flash memory (PROGMEM) to save RAM
const char htmlTemplate[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
    <title>CynoIoT config page</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 500px;
            margin: 50px auto;
            padding: 20px;
            background: #f5f5f5;
        }
        .container {
            background: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            margin-bottom: 20px;
            font-size: 24px;
        }
        h3 {
            color: #333;
            margin-top: 25px;
            margin-bottom: 15px;
            font-size: 18px;
        }
        ul {
            list-style: none;
            padding: 0;
            margin: 20px 0;
        }
        li {
            padding: 10px 0;
            border-bottom: 1px solid #eee;
            color: #555;
        }
        li:last-child {
            border-bottom: none;
        }
        .btn {
            padding: 10px 20px;
            margin: 5px;
            font-size: 16px;
            cursor: pointer;
            border: none;
            border-radius: 5px;
            color: white;
        }
        .btn-on {
            background-color: #4CAF50;
        }
        .btn-on:hover {
            background-color: #45a049;
        }
        .btn-off {
            background-color: #f44336;
        }
        .btn-off:hover {
            background-color: #da190b;
        }
        .btn-group {
            margin: 15px 0;
            padding: 15px;
            background: #f9f9f9;
            border-radius: 5px;
        }
        .btn-group strong {
            display: inline-block;
            min-width: 80px;
            color: #333;
        }
        .status {
            display: inline-block;
            padding: 5px 10px;
            margin: 5px;
            border-radius: 3px;
            font-weight: bold;
            font-size: 14px;
        }
        .status-on {
            background-color: #4CAF50;
            color: white;
        }
        .status-off {
            background-color: #f44336;
            color: white;
        }
        button {
            background: #007bff;
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
            width: 100%;
            margin-top: 20px;
        }
        button:hover {
            background: #0056b3;
        }
        .link {
            display: block;
            text-align: center;
            background: #dc3545;
            color: white;
            text-decoration: none;
            padding: 12px 24px;
            border-radius: 5px;
            margin-top: 20px;
            font-size: 16px;
            cursor: pointer;
        }
        .link:hover {
            background: #c82333;
            text-decoration: none;
        }
    </style>
    <script>
    if (%STATE% == 0) {
        location.href='/config';
    }

    function togglePin(pin, state) {
        fetch('/gpio/' + pin + '?state=' + state)
            .then(response => response.text())
            .then(data => {
                updateStatus();
            })
            .catch(error => console.error('Error:', error));
    }

    function updateStatus() {
        fetch('/status')
            .then(response => response.json())
            .then(data => {
                updatePinStatus('pump', data.pump);
            });
    }

    function updatePinStatus(id, state) {
        const statusEl = document.getElementById(id + '_status');
        if (statusEl) {
            statusEl.className = 'status ' + (state ? 'status-on' : 'status-off');
            statusEl.textContent = state ? 'ON' : 'OFF';
        }
    }

    window.onload = updateStatus;
    setInterval(updateStatus, 10000);
    </script>
</head>
<body>
    <div class="container">
        <h1>CynoIoT config data</h1>
        <ul>
            <li>Device name: %THING_NAME%</li>
            <li>อีเมลล์: %EMAIL%</li>
            <li>WIFI SSID: %SSID%</li>
            <li>RSSI: %RSSI% dBm</li>
            <li>ESP ID: <a href='https://cynoiot.com/device/%ESP_ID%' target='_blank'>%ESP_ID%</a></li>
            <li>Version: %VERSION%</li>
        </ul>

        <a class="link" href='/config'>configure page แก้ไขข้อมูล wifi และ user</a>
        <button type='button' onclick="location.href='/reboot';">รีบูทอุปกรณ์</button>
    </div>
<br>
    <div class="container">
        <h3>GPIO Control</h3>
        <div class="btn-group">
            <strong>PUMP:</strong> <span id="pump_status" class="status">Loading...</span><br>
            <button class="btn btn-on" onclick="togglePin('pump', '1')">PUMP ON</button>
            <button class="btn btn-off" onclick="togglePin('pump', '0')">PUMP OFF</button>
        </div>
    </div>
</body>
</html>
)rawliteral";

// ==========================================================================================
// FUNCTION PROTOTYPES
// ==========================================================================================
void handleRoot();                                                    // Main web page handler
void showPopupMessage(String msg, uint8_t timeout = 3);              // Display popup message on OLED
void handleStatus();                                                  // HTTP handler for status JSON

// Callback functions for IotWebConf
void wifiConnected();                                                // Called when WiFi connection established
void configSaved();                                                  // Called when configuration is saved
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper); // Form validation handler

// ==========================================================================================
// TIMING VARIABLES
// ==========================================================================================
unsigned long previousMillis = 0;  // Timer for 1-second interval tasks

// ==========================================================================================
// NETWORK SERVER OBJECTS
// ==========================================================================================
DNSServer dnsServer;              // DNS server for captive portal
WebServer server(80);              // Web server on port 80 for configuration interface

// ==========================================================================================
// OTA (OVER-THE-AIR) UPDATE SERVER
// ==========================================================================================
// HTTP update server for remote firmware updates
#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif

// ==========================================================================================
// WEB CONFIGURATION PARAMETERS
// ==========================================================================================
char emailParamValue[STRING_LEN];  // Buffer for email parameter

// IotWebConf configuration object
// version is defined in iotbundle.h file
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword);

// Parameter group for login credentials
IotWebConfParameterGroup login = IotWebConfParameterGroup("login", "ล็อกอิน(สมัครที่เว็บก่อนนะครับ)");

// Email parameter for CynoIOT account login
IotWebConfTextParameter emailParam = IotWebConfTextParameter("อีเมลล์", "emailParam", emailParamValue, STRING_LEN);

// ==========================================================================================
// DISPLAY BITMAPS
// ==========================================================================================
// Logo bitmap for startup screen (33x30 pixels)
const uint8_t logo_bmp[] = { // 'cyno', 33x30px
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0,
    0x01, 0xf8, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x1f, 0xff, 0xff, 0xfc,
    0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x1f, 0xf8, 0x0f, 0xfe, 0x00, 0x3f,
    0xf0, 0x07, 0xfe, 0x00, 0x3f, 0xf0, 0x07, 0xfe, 0x00, 0x3f, 0xe0, 0x03,
    0xfe, 0x00, 0x3f, 0xc0, 0x01, 0xfe, 0x00, 0x3f, 0x80, 0x00, 0xfe, 0x00,
    0x7f, 0x00, 0x00, 0x7f, 0x00, 0x7f, 0x00, 0x00, 0x7f, 0x00, 0x7e, 0x00,
    0x00, 0x3f, 0x00, 0x7e, 0x00, 0x00, 0x3f, 0x00, 0x7e, 0x38, 0x0e, 0x3f,
    0x00, 0x3e, 0x38, 0x0e, 0x3e, 0x00, 0x0e, 0x10, 0x04, 0x38, 0x00, 0x0e,
    0x00, 0x00, 0x38, 0x00, 0x0e, 0x00, 0x00, 0x38, 0x00, 0x0e, 0x03, 0x20,
    0x38, 0x00, 0x0e, 0x07, 0xf0, 0x38, 0x00, 0x0e, 0x03, 0xe0, 0x30, 0x00,
    0x06, 0x01, 0xc0, 0x30, 0x00, 0x07, 0x01, 0xc0, 0x70, 0x00, 0x03, 0xff,
    0xff, 0xe0, 0x00, 0x01, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x7f, 0xff, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// WiFi status icons (8x8 pixels) for status display
const uint8_t wifi_on[] = {0x00, 0x3c, 0x42, 0x99, 0x24, 0x00, 0x18, 0x18};                                                 // WiFi connected icon
const uint8_t wifi_off[] = {0x01, 0x3e, 0x46, 0x99, 0x34, 0x20, 0x58, 0x98};                                                // WiFi disconnected icon
const uint8_t wifi_ap[] = {0x41, 0x00, 0x80, 0x80, 0xa2, 0x80, 0xaa, 0x80, 0xaa, 0x80, 0x88, 0x80, 0x49, 0x00, 0x08, 0x00}; // Access Point mode icon
const uint8_t wifi_nointernet[] = {0x03, 0x7b, 0x87, 0x33, 0x4b, 0x00, 0x33, 0x33};                                         // No internet connection icon

// ==========================================================================================
// GLOBAL VARIABLES - DISPLAY AND NETWORK STATUS
// ==========================================================================================
uint8_t t_connecting = 0;                    // Counter for connecting animation
uint8_t displaytime = 0;                     // Counter for display time management
iotwebconf::NetworkState prev_state = iotwebconf::Boot;  // Previous network state
uint8_t sampleUpdate = 0;                    // Counter for sensor update interval
String noti = "";                            // Notification message string
uint16_t timer_nointernet = 0;               // Counter for no-internet detection
uint8_t numVariables = 0;                    // Number of sensor variables to send

// ==========================================================================================
// GLOBAL VARIABLES - SENSOR READINGS
// ==========================================================================================
float humidity = 0, temperature = 0, ph = 0;           // Current sensor values (float)
uint32_t conductivity = 0, nitrogen = 0, phosphorus = 0, potassium = 0;  // Current sensor values (uint32)

// ==========================================================================================
// EMA (EXPONENTIAL MOVING AVERAGE) FILTER VARIABLES
// ==========================================================================================
// These variables implement EMA filtering to smooth out sensor reading fluctuations
// EMA formula: EMA = alpha * current_value + (1 - alpha) * previous_EMA
// Alpha value of 0.1 gives more weight to historical data (smoother but slower response)
float emaHumidity = 0, emaTemperature = 0, emaPh = 0;          // EMA values for float sensors
float emaConductivity = 0, emaNitrogen = 0, emaPhosphorus = 0, emaPotassium = 0;  // EMA values for integer sensors
const float EMA_ALPHA = 0.1;                      // Smoothing factor (0.0 to 1.0, lower = smoother)
bool emaInitialized = false;                      // Flag to track if EMA has been initialized

// ==========================================================================================
// GLOBAL VARIABLES - PUMP CONTROL
// ==========================================================================================
uint16_t interval = 600;                          // Max pump runtime in seconds (for protection)
bool pumpState = false;                           // Pump ON/OFF state
String displayStatus = "";                        // Status string for display
String popupStatus = "";                          // Popup message for display
uint8_t popupShowTimer = 0;                       // Timer for popup message display

// ==========================================================================================
// GLOBAL VARIABLES - SYSTEM CONFIGURATION
// ==========================================================================================
uint8_t humidLowCutoff = 10, humidHighCutoff = 40;        // Humidity thresholds (%)
uint32_t pumpOnProtectionTimer = 0;               // Safety timer for pump protection

// ==========================================================================================
// SYSTEM MODE DEFINITIONS
// ==========================================================================================
// Global operating modes
enum GlobalMode : uint8_t
{
    OFF = 1,    // Manual mode - pump controlled via P event only
    AUTO = 2    // Automatic mode - pump controlled by humidity thresholds
};
GlobalMode globalMode = OFF;  // Current global mode (loaded from EEPROM)

// ==========================================================================================
// EVENT HANDLER
// ==========================================================================================
void handleEvent(String event, String value)
{
    EEPROM.begin(512);

    // EVENT: M - Global Mode Selection
    if (event == "M")
    {
        Serial.println("Mode: " + value);

        // Set to automatic mode
        if (value == "auto")
        {
            globalMode = AUTO;
            showPopupMessage("Set to\n\nAuto\n\nMode");
        }
        // Set to manual/off mode
        else
        {
            globalMode = OFF;
            showPopupMessage("Set to\n\nOff\n\nMode");
        }

        // Persist mode setting to EEPROM (only if changed to reduce write cycles)
        uint8_t currentGlobalMode = EEPROM.read(500);
        if (currentGlobalMode != (uint8_t)globalMode)
        {
            EEPROM.write(500, globalMode);
            EEPROM.commit();
        }
    }

    // EVENT: In - Irrigation Interval Setting
    else if (event == "In")
    {
        Serial.println("Interval: " + value);
        showPopupMessage(String("Interval\n\nSet to\n\n") + value + String(" s"));
        interval = (uint16_t)value.toInt(); // Convert string to integer and store

        uint16_t currentValue = (uint16_t)EEPROM.read(498) | ((uint16_t)EEPROM.read(499) << 8);
        if (currentValue != interval)
        {
            // Store interval as 16-bit value (low byte and high byte)
            EEPROM.write(498, interval & 0xFF);        // low byte
            EEPROM.write(499, (interval >> 8) & 0xFF); // high byte
            EEPROM.commit();
            Serial.println("Verified written value: " + String((uint16_t)EEPROM.read(498) | ((uint16_t)EEPROM.read(499) << 8)));
        }
        else
        {
            Serial.println("Value already matches, skipping write");
        }
    }

    // EVENT: P - Pump Manual Control
    else if (event == "P")
    {
        // Turn pump ON
        if (value == "1")
        {
            pumpState = 1;
            iot.eventUpdate("P", 1);  // Notify server of state change
            showPopupMessage("Pump\n\nOn");
        }
        // Turn pump OFF
        else if (value == "0")
        {
            pumpState = 0;
            iot.eventUpdate("P", 0);  // Notify server of state change
            showPopupMessage("Pump\n\nOff");
        }
    }

    // EVENT: Hl - Humidity Low Cutoff Threshold
    else if (event == "Hl")
    {
        Serial.println("Humid low cutoff : " + value);
        humidLowCutoff = (uint8_t)value.toInt();
        showPopupMessage(String("Humid Low\n\nCutoff to\n\n") + value + String("%"));

        // Persist to EEPROM (only if changed)
        uint8_t currentHumidLowCutoff = EEPROM.read(489);
        if (currentHumidLowCutoff != (uint8_t)humidLowCutoff)
        {
            EEPROM.write(489, (uint8_t)humidLowCutoff);
            EEPROM.commit();
        }
    }

    // EVENT: Hh - Humidity High Cutoff Threshold
    else if (event == "Hh")
    {
        Serial.println("Humid high cutoff : " + value);
        humidHighCutoff = (uint8_t)value.toInt();
        showPopupMessage(String("Humid High\n\nCutoff to\n\n") + value + String("%"));

        // Persist to EEPROM (only if changed)
        uint8_t currentHumidHighCutoff = EEPROM.read(488);
        if (currentHumidHighCutoff != (uint8_t)humidHighCutoff)
        {
            EEPROM.write(488, (uint8_t)humidHighCutoff);
            EEPROM.commit();
        }
    }

    EEPROM.end();  // Close EEPROM session
}

// ==========================================================================================
// IOT SETUP FUNCTION
// ==========================================================================================
void iotSetup()
{
    // Begin EEPROM session with 512 bytes
    Serial.println("Loading setting from EEPROM");
    EEPROM.begin(512);

    // Load irrigation interval from EEPROM (16-bit value: low byte + high byte)
    interval = (uint16_t)EEPROM.read(498) | ((uint16_t)EEPROM.read(499) << 8);
    if (interval == 65535) // 0xFFFF indicates uninitialized EEPROM
    {
        Serial.println("Load interval = " + String(interval) + ", interval not found in EEPROM, using default value");
        interval = 600;  // Default: 10 minutes (600 seconds)
    }
    Serial.println("interval: " + String(interval));

    // Load global operating mode from EEPROM
    uint8_t eepromValue = EEPROM.read(500);
    if (eepromValue == 255) // 0xFF indicates uninitialized EEPROM
    {
        Serial.println("Load globalMode = " + String(eepromValue) + ", globalMode not found in EEPROM, using default value");
        globalMode = OFF;  // Default: OFF mode
    }
    else
    {
        globalMode = static_cast<GlobalMode>(eepromValue);
    }
    Serial.println("globalMode: " + String(globalMode));

    // Load humidity low cutoff threshold from EEPROM
    humidLowCutoff = (uint8_t)EEPROM.read(489);
    if (humidLowCutoff == 255) // 0xFF indicates uninitialized EEPROM
    {
        Serial.println("Load humidLowCutoff = " + String(humidLowCutoff) + ", humidLowCutoff not found in EEPROM, using default value = 10%");
        humidLowCutoff = 10;  // Default: 10%
    }
    Serial.println("humidLowCutoff: " + String(humidLowCutoff));

    // Load humidity high cutoff threshold from EEPROM
    humidHighCutoff = (uint8_t)EEPROM.read(488);
    if (humidHighCutoff == 255) // 0xFF indicates uninitialized EEPROM
    {
        Serial.println("Load humidHighCutoff = " + String(humidHighCutoff) + ", humidHighCutoff not found in EEPROM, using default value = 40%");
        humidHighCutoff = 40;  // Default: 40%
    }
    Serial.println("humidHighCutoff: " + String(humidHighCutoff));

    EEPROM.end();  // Close EEPROM session

    // Register callback function to handle events from CynoIOT server
    iot.setEventCallback(handleEvent);

    const uint8_t version = 1; // Project version number for template selection

// ==========================================================================================
// CONFIGURE DATA TEMPLATE BASED ON SENSOR MODEL
// ==========================================================================================
#ifdef NOSENSOR_MODEL
    // Basic configuration - no sensors, only on/off state
    numVariables = 1;
    String keyname[numVariables] = {"on"};           // Variable names for cloud
    iot.setTemplate("water_control_nosensor", version); // Select corresponding template

#elif defined(HUMID_MODEL)
    // Humidity sensor only
    numVariables = 2;
    String keyname[numVariables] = {"on", "humid"};  // Variable names: state, humidity
    iot.setTemplate("water_control_humid", version);    // Select corresponding template

#elif defined(TEMP_HUMID_MODEL)
    // Temperature and humidity sensors
    numVariables = 3;
    String keyname[numVariables] = {"on", "humid", "temp"}; // Variable names: state, humidity, temperature
    iot.setTemplate("water_control_temp_humid", version);      // Select corresponding template

#elif defined(TEMP_HUMID_EC_MODEL)
    // Temperature, humidity, and EC (conductivity) sensors
    numVariables = 4;
    String keyname[numVariables] = {"on", "humid", "temp", "ec"}; // Variable names: state, humidity, temperature, EC
    iot.setTemplate("water_control_temp_humid_ec", version);         // Select corresponding template

#else // ALL_7IN1_MODEL (default)
    // Full 7-in-1 sensor: humidity, temperature, EC, pH, N, P, K
    numVariables = 8;
    String keyname[numVariables] = {"on", "humid", "temp", "ec", "ph", "n", "p", "k"}; // All 7 sensor values
    iot.setTemplate("water_control_7in1", version);                                       // Select corresponding template
#endif

    // Register variable names with CynoIOT
    iot.setkeyname(keyname, numVariables);

    // Display client ID for debugging
    Serial.println("ClinetID:" + String(iot.getClientId()));
}

// ==========================================================================================
// 1-SECOND PERIODIC TASK - Network status monitoring
// ==========================================================================================
void time1sec()
{
    // Check connection to CynoIOT server
    if (iotWebConf.getState() == iotwebconf::OnLine)
    {
        if (iot.status())
        {
            timer_nointernet = 0;
        }
        else
        {
            timer_nointernet++;
            if (timer_nointernet > 30)
                Serial.println("No connection time : " + String(timer_nointernet));
        }
    }

    // Reconnect WiFi if unable to reach the server
    if (timer_nointernet == 60)
    {
        Serial.println("Can't connect to server -> Restart wifi");
        iotWebConf.goOffLine();
        timer_nointernet++;
    }
    else if (timer_nointernet >= 65)
    {
        timer_nointernet = 0;
        iotWebConf.goOnLine(false);
    }
    else if (timer_nointernet >= 61)
        timer_nointernet++;
}

// ==========================================================================================
// SETUP FUNCTION - One-Time Initialization
// ==========================================================================================
void setup()
{
    // Initialize Serial Monitor for debugging (115200 baud)
    Serial.begin(115200);

    // ==========================================================================================
    // RS485 SERIAL INITIALIZATION
    // ==========================================================================================
    // Initialize RS485 communication for Modbus sensor at 4800 baud
#ifdef ESP8266
    RS485Serial.begin(4800, SWSERIAL_8N1, MAX485_RO, MAX485_DI); // Software serial for ESP8266
#elif defined(ESP32)
    RS485Serial.begin(4800, SERIAL_8N1, MAX485_RO, MAX485_DI); // Hardware serial for ESP32
#endif

    // OLED DISPLAY INITIALIZATION
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    oled.clearDisplay();
    oled.drawBitmap(16, 5, logo_bmp, 33, 30, 1); // Display CynoIOT logo
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 40);
    oled.print("  CYNOIOT");
    oled.display();

    // EEPROM CLEAR FUNCTION
    pinMode(RSTPIN, INPUT_PULLUP);
    if (digitalRead(RSTPIN) == false)
    {
        delay(1000);  // Debounce - wait 1 second
        if (digitalRead(RSTPIN) == false)  // Still low after debounce
        {
            oled.clearDisplay();
            oled.setCursor(0, 0);
            oled.print("Clear All data\n rebooting");
            oled.display();
            delay(1000);
            clearEEPROM();  // Clear all EEPROM and restart
        }
    }

    // Configure pump control pin
    pinMode(PUMP, INPUT);  // Start in INPUT mode (pump OFF)
    digitalWrite(PUMP, PUMP_OFF);

    // IOT WEBCONF CONFIGURATION
    login.addItem(&emailParam);

    // Optional: Set status LED pin for ESP32-S2
#ifdef CONFIG_IDF_TARGET_ESP32S2
    iotWebConf.setStatusPin(15);
#endif

    // Configure IotWebConf callbacks and parameters
    iotWebConf.addParameterGroup(&login);  // Add login parameter group
    iotWebConf.setConfigSavedCallback(&configSaved);  // Called when config is saved
    iotWebConf.setFormValidator(&formValidator);  // Custom form validation
    iotWebConf.getApTimeoutParameter()->visible = false;  // Hide AP timeout setting
    iotWebConf.setWifiConnectionCallback(&wifiConnected);  // Called when WiFi connects

    // OTA UPDATE SERVER SETUP
    iotWebConf.setupUpdateServer(
        [](const char *updatePath)
        {
            httpUpdater.setup(&server, updatePath);  // Setup update handler
        },
        [](const char *userName, char *password)
        {
            httpUpdater.updateCredentials(userName, password);  // Set update credentials
        });

    // INITIALIZE IOT WEBCONF
    iotWebConf.init();  // Start WiFi configuration portal

    // WEB SERVER URL HANDLERS
    server.on("/", handleRoot);  // Main page - displays status and controls

    // Configuration page - handled by IotWebConf
    server.on("/config", []
              { iotWebConf.handleConfig(); });

    // Utility endpoints
    server.on("/cleareeprom", clearEEPROM);  // Clear EEPROM and reboot
    server.on("/reboot", reboot);            // Reboot device

    // Manual pump control via HTTP
    server.on("/gpio/pump", []()
              {
                  String state = server.arg("state");
                  if (state == "1")
                      pumpState = 1;
                  else if (state == "0")
                      pumpState = 0;
                  updateHardwareOutputs();  // Apply changes to hardware
                  server.send(200, "text/plain", "OK");
              });

    // Status endpoint - returns JSON with current pump state
    server.on("/status", handleStatus);

    // 404 handler - let IotWebConf handle unknown URLs
    server.onNotFound([]()
                      { iotWebConf.handleNotFound(); });

    Serial.println("Ready.");

    // MODBUS SENSOR INITIALIZATION
    node.preTransmission(preTransmission);   // Called before transmission - sets DE/RE high
    node.postTransmission(postTransmission); // Called after transmission - sets DE/RE low
    node.begin(ADDRESS, RS485Serial);        // Initialize Modbus with sensor address

    // Wait for sensor to stabilize
    delay(1000);

    // Initialize CynoIOT connection and load configuration
    iotSetup();
}

// ==========================================================================================
// MAIN LOOP FUNCTION
// ==========================================================================================
void loop()
{clo
    // ==========================================================================================
    // BACKGROUND TASKS
    // ==========================================================================================
    iotWebConf.doLoop();   // Process WiFi configuration portal (captive portal, etc.)
    server.handleClient(); // Handle incoming HTTP requests from web clients
    iot.handle();          // Handle CynoIOT cloud communication
#ifdef ESP8266
    MDNS.update();         // Update mDNS responder for local hostname resolution
#endif

    // ==========================================================================================
    // TIMED TASKS (Every 1 Second)
    // ==========================================================================================
    uint32_t currentMillis = millis();
    if (currentMillis - previousMillis >= 1000) /* Check if 1 second has elapsed */
    {
        previousMillis = currentMillis; /* Reset timer for next 1-second interval */
        sampleUpdate++;  // Increment counter to track 5-second intervals

        // Run functions that need to execute every second
        time1sec();           // Check network status
        updateSystemState();  // Update pump timers and states
        updateDisplay();      // Refresh OLED display

        // SENSOR READING (Every 5 Seconds)
        if (sampleUpdate >= 5)
        {
            readAndSendSensorData();  // Read sensors and send to CynoIOT
            sampleUpdate = 0;         // Reset counter
        }
    }

    // HARDWARE OUTPUT UPDATE
    updateHardwareOutputs();
}

// ==========================================================================================
// SYSTEM STATE UPDATE FUNCTION
// ==========================================================================================
void updateSystemState()
{
    displayStatus = "";  // Clear display status string

#if !defined(NOSENSOR_MODEL) // Only run humidity control if sensors are present

    // ==========================================================================================
    // AUTO MODE - Humidity-based pump control
    // ==========================================================================================
    // Turns pump ON when soil is dry, OFF when soil is wet enough.
    // Hysteresis between low and high thresholds prevents rapid on/off cycling.
    if (globalMode == AUTO)
    {
        // Soil is too dry - turn pump ON
        if (humidity > 0 && humidity < humidLowCutoff)
        {
            pumpState = 1;
        }
        // Soil is wet enough - turn pump OFF
        else if (humidity >= humidHighCutoff)
        {
            pumpState = 0;
        }
        // Between thresholds: maintain current state (hysteresis)
    }
#endif

    // ==========================================================================================
    // DISPLAY STATUS
    // ==========================================================================================
    if (globalMode == AUTO)
        displayStatus = "AUTO ";
    else
        displayStatus = "OFF ";
    displayStatus += (digitalRead(PUMP) == PUMP_ON) ? "ON" : "OFF";

    // ==========================================================================================
    // SAFETY PROTECTION - Pump Runaway Detection
    // ==========================================================================================
    // Protects against pump running too long (e.g., forgotten manual activation)
    if (digitalRead(PUMP) == PUMP_ON)
    {
        pumpOnProtectionTimer++;  // Increment protection counter

        // If pump has been running too long (5x interval), shut it down
        if (pumpOnProtectionTimer >= (uint32_t)interval * 5)
        {
            pumpOnProtectionTimer = 0;  // Reset counter
            pumpState = 0;              // Emergency shutoff
            showPopupMessage("Pump\n\nProtection\n\nStop");
        }
    }
    else
    {
        pumpOnProtectionTimer = 0;  // Reset counter when pump is off
    }
}

// ==========================================================================================
// EMA (EXPONENTIAL MOVING AVERAGE) FILTER - FLOAT VERSION
// ==========================================================================================
float applyEmaFilter(float currentValue, float emaValue, bool firstReading = false)
{
    // Initialize filter on first reading - use raw value as starting point
    if (firstReading || !emaInitialized)
    {
        return round(currentValue * 10) / 10.0; // Initialize with first reading, round to 1 decimal
    }

    // Apply EMA formula: α × current + (1-α) × previous
    // EMA_ALPHA = 0.1 gives 10% weight to new reading, 90% to historical data
    float filteredValue = EMA_ALPHA * currentValue + (1.0 - EMA_ALPHA) * emaValue;

    // Round to 1 decimal place for consistent output format
    return round(filteredValue * 10) / 10.0;
}

// ==========================================================================================
// EMA (EXPONENTIAL MOVING AVERAGE) FILTER - INTEGER VERSION
// ==========================================================================================
float applyEmaFilterInt(uint32_t currentValue, float emaValue, bool firstReading = false)
{
    // Initialize filter on first reading - use raw value as starting point
    if (firstReading || !emaInitialized)
    {
        return round((float)currentValue * 10) / 10.0; // Initialize with first reading, round to 1 decimal
    }

    // Apply EMA formula: α × current + (1-α) × previous
    // Cast integer to float for precise calculation
    float filteredValue = EMA_ALPHA * (float)currentValue + (1.0 - EMA_ALPHA) * emaValue;

    // Round to 1 decimal place for consistent output format
    return round(filteredValue * 10) / 10.0;
}

// ==========================================================================================
// SENSOR READING AND CLOUD DATA UPLOAD FUNCTION
// ==========================================================================================
void readAndSendSensorData()
{
    uint32_t onState = getSystemState();

#if defined(NOSENSOR_MODEL)

    float payload[numVariables] = {onState};
    iot.update(payload);

#elif defined(HUMID_MODEL)

    uint8_t result = node.readHoldingRegisters(0x0000, 1); // Read 1 register: humidity
    disConnect();  // Disable RS485 transceiver after reading

    // Check if Modbus communication was successful
    if (result == node.ku8MBSuccess)
    {
        // Register 0: Humidity (resolution 0.1 %RH, range 0-1000 = 0-100%)
        float rawHumidity = node.getResponseBuffer(0) / 10.0; // Convert to %RH

        // Apply EMA filter to smooth out sensor noise
        humidity = applyEmaFilter(rawHumidity, emaHumidity, !emaInitialized);
        emaHumidity = humidity;
        emaInitialized = true;

        Serial.println("----- Soil Parameters -----");
        Serial.print("Humidity  : ");
        Serial.print(humidity, 1);
        Serial.print(" %RH (raw: ");
        Serial.print(rawHumidity);
        Serial.println(")");

        // Format: [system_state, humidity]
        float payload[numVariables] = {onState, humidity};
        iot.update(payload);
    }
    else
    {
        Serial.println("Modbus error reading humidity!");
    }

#elif defined(TEMP_HUMID_MODEL)
    uint8_t result = node.readHoldingRegisters(0x0000, 2); // Read 2 registers: humidity, temperature
    disConnect();

    if (result == node.ku8MBSuccess)
    {
        float rawHumidity = node.getResponseBuffer(0) / 10.0;    // %RH
        float rawTemperature = node.getResponseBuffer(1) / 10.0; // °C

        humidity = applyEmaFilter(rawHumidity, emaHumidity, !emaInitialized);
        temperature = applyEmaFilter(rawTemperature, emaTemperature, !emaInitialized);
        emaHumidity = humidity;
        emaTemperature = temperature;
        emaInitialized = true;

        Serial.println("----- Soil Parameters -----");
        Serial.print("Humidity  : ");
        Serial.print(humidity, 1);
        Serial.print(" %RH (raw: ");
        Serial.print(rawHumidity);
        Serial.println(")");
        Serial.print("Temperature: ");
        Serial.print(temperature, 1);
        Serial.print(" °C (raw: ");
        Serial.print(rawTemperature);
        Serial.println(")");

        float payload[numVariables] = {onState, humidity, temperature};
        iot.update(payload);
    }
    else
    {
        Serial.println("Modbus error reading temp/humidity!");
    }

#elif defined(TEMP_HUMID_EC_MODEL)
    uint8_t result = node.readHoldingRegisters(0x0000, 3); // Read 3 registers
    disConnect();

    if (result == node.ku8MBSuccess)
    {
        float rawHumidity = node.getResponseBuffer(0) / 10.0;    // %RH
        float rawTemperature = node.getResponseBuffer(1) / 10.0; // °C
        uint32_t rawConductivity = node.getResponseBuffer(2);    // µS/cm

        humidity = applyEmaFilter(rawHumidity, emaHumidity, !emaInitialized);
        temperature = applyEmaFilter(rawTemperature, emaTemperature, !emaInitialized);
        conductivity = (uint32_t)applyEmaFilterInt(rawConductivity, emaConductivity, !emaInitialized);
        emaHumidity = humidity;
        emaTemperature = temperature;
        emaConductivity = conductivity;
        emaInitialized = true;

        Serial.println("----- Soil Parameters -----");
        Serial.print("Humidity  : ");
        Serial.print(humidity, 1);
        Serial.print(" %RH (raw: ");
        Serial.print(rawHumidity);
        Serial.println(")");
        Serial.print("Temperature: ");
        Serial.print(temperature, 1);
        Serial.print(" °C (raw: ");
        Serial.print(rawTemperature);
        Serial.println(")");
        Serial.print("Conductivity: ");
        Serial.print(conductivity, 1);
        Serial.print(" µS/cm (raw: ");
        Serial.print(rawConductivity);
        Serial.println(")");

        float payload[numVariables] = {onState, humidity, temperature, conductivity};
        iot.update(payload);
    }
    else
    {
        Serial.println("Modbus error reading temp/humidity/EC!");
    }

#elif defined(ALL_7IN1_MODEL)
    uint8_t result = node.readHoldingRegisters(0x0000, 7); // Read all 7 registers
    disConnect();

    if (result == node.ku8MBSuccess)
    {
        float rawHumidity = node.getResponseBuffer(0) / 10.0;    // %RH
        float rawTemperature = node.getResponseBuffer(1) / 10.0; // °C
        uint32_t rawConductivity = node.getResponseBuffer(2);    // µS/cm
        float rawPh = node.getResponseBuffer(3) / 10.0;          // pH
        uint32_t rawNitrogen = node.getResponseBuffer(4);        // mg/kg
        uint32_t rawPhosphorus = node.getResponseBuffer(5);      // mg/kg
        uint32_t rawPotassium = node.getResponseBuffer(6);       // mg/kg

        // Apply EMA filter to all readings
        humidity = applyEmaFilter(rawHumidity, emaHumidity, !emaInitialized);
        temperature = applyEmaFilter(rawTemperature, emaTemperature, !emaInitialized);
        conductivity = (uint32_t)applyEmaFilterInt(rawConductivity, emaConductivity, !emaInitialized);
        ph = applyEmaFilter(rawPh, emaPh, !emaInitialized);
        nitrogen = (uint32_t)applyEmaFilterInt(rawNitrogen, emaNitrogen, !emaInitialized);
        phosphorus = (uint32_t)applyEmaFilterInt(rawPhosphorus, emaPhosphorus, !emaInitialized);
        potassium = (uint32_t)applyEmaFilterInt(rawPotassium, emaPotassium, !emaInitialized);

        emaHumidity = humidity;
        emaTemperature = temperature;
        emaConductivity = conductivity;
        emaPh = ph;
        emaNitrogen = nitrogen;
        emaPhosphorus = phosphorus;
        emaPotassium = potassium;
        emaInitialized = true;

        Serial.println("----- Soil Parameters -----");
        Serial.print("Humidity  : ");
        Serial.print(humidity, 1);
        Serial.print(" %RH (raw: ");
        Serial.print(rawHumidity);
        Serial.println(")");
        Serial.print("Temperature: ");
        Serial.print(temperature, 1);
        Serial.print(" °C (raw: ");
        Serial.print(rawTemperature);
        Serial.println(")");
        Serial.print("Conductivity: ");
        Serial.print(conductivity, 1);
        Serial.print(" µS/cm (raw: ");
        Serial.print(rawConductivity);
        Serial.println(")");
        Serial.print("pH        : ");
        Serial.print(ph, 1);
        Serial.print(" (raw: ");
        Serial.print(rawPh);
        Serial.println(")");
        Serial.print("Nitrogen  : ");
        Serial.print(nitrogen, 1);
        Serial.print(" mg/kg (raw: ");
        Serial.print(rawNitrogen);
        Serial.println(")");
        Serial.print("Phosphorus: ");
        Serial.print(phosphorus, 1);
        Serial.print(" mg/kg (raw: ");
        Serial.print(rawPhosphorus);
        Serial.println(")");
        Serial.print("Potassium : ");
        Serial.print(potassium, 1);
        Serial.print(" mg/kg (raw: ");
        Serial.print(rawPotassium);
        Serial.println(")");

        // Format: [system_state, humidity, temperature, conductivity, pH, N, P, K]
        float payload[numVariables] = {onState, humidity, temperature, conductivity, ph, nitrogen, phosphorus, potassium};
        iot.update(payload);
    }
    else
    {
        Serial.println("Modbus error!");
    }
#endif
}

// ==========================================================================================
// OLED DISPLAY UPDATE FUNCTION
// ==========================================================================================
void updateDisplay()
{
    // Get the current WiFi/network connection state from IotWebConf
    iotwebconf::NetworkState curr_state = iotWebConf.getState();

    // ==========================================================================================
    // NETWORK STATE CHANGE DETECTION
    // ==========================================================================================

    // BOOT state - Device just powered on
    if (curr_state == iotwebconf::Boot)
    {
        prev_state = curr_state;
    }

    // NOT CONFIGURED state - No WiFi credentials saved
    else if (curr_state == iotwebconf::NotConfigured)
    {
        if (prev_state == iotwebconf::Boot)
        {
            displaytime = 5;
            prev_state = curr_state;
            noti = "-State-\n\nno config\nstay in\nAP Mode";
        }
    }

    // AP MODE state - Configuration portal active
    else if (curr_state == iotwebconf::ApMode)
    {
        if (prev_state == iotwebconf::Boot)
        {
            displaytime = 5;
            prev_state = curr_state;
            noti = "-State-\n\nAP Mode\nfor 30 sec";
        }
        else if (prev_state == iotwebconf::Connecting)
        {
            displaytime = 5;
            prev_state = curr_state;
            noti = "-State-\n\nX  can't\nconnect\nwifi\ngo AP Mode";
        }
    }

    // ONLINE state - Successfully connected to WiFi
    else if (curr_state == iotwebconf::OnLine)
    {
        if (prev_state == iotwebconf::Connecting)
        {
            displaytime = 5;
            prev_state = curr_state;
            noti = "-State-\n\nwifi\nconnect\nsuccess\n" + String(WiFi.RSSI()) + " dBm";
        }
    }

    // OFFLINE state - WiFi connection lost
    else if (curr_state == iotwebconf::OffLine)
    {
        displaytime = 10;
        prev_state = curr_state;
        noti = "-State-\n\nX wifi\ndisconnect\ngo AP Mode";
    }

    // CONNECTING state - Attempting to connect to WiFi
    else if (curr_state == iotwebconf::Connecting)
    {
        if (prev_state == iotwebconf::ApMode)
        {
            displaytime = 5;
            prev_state = curr_state;
            noti = "-State-\n\nwifi\nconnecting";
        }
        else if (prev_state == iotwebconf::OnLine)
        {
            displaytime = 10;
            prev_state = curr_state;
            noti = "-State-\n\nX  wifi\ndisconnect\nreconnecting";
        }
    }

    // ==========================================================================================
    // CYNIIOT CLOUD NOTIFICATIONS
    // ==========================================================================================
    if (iot.noti != "" && displaytime == 0)
    {
        displaytime = 3;
        noti = iot.noti;
        iot.noti = "";
    }

    // ==========================================================================================
    // DISPLAY NOTIFICATION MESSAGES
    // ==========================================================================================
    if (displaytime)
    {
        displaytime--;
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print(noti);
        Serial.println(noti);
    }

    // ==========================================================================================
    // DISPLAY POPUP MESSAGES
    // ==========================================================================================
    else if (popupShowTimer)
    {
        popupShowTimer--;
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print(popupStatus);
    }

    // ==========================================================================================
    // REGULAR STATUS DISPLAY (DEFAULT SCREEN)
    // ==========================================================================================
    else
    {
        oled.clearDisplay();

        // MAIN DISPLAY AREA - SENSOR VALUES OR MODE
        oled.setTextSize(2);
        oled.setCursor(0, 15);

#if !defined(NOSENSOR_MODEL)
        // If sensors are present, display humidity as main value
        oled.print(humidity, 0);
        oled.print(" %");
#else
        // If no sensors, display operating mode instead
        oled.setTextSize(1);
        if (globalMode == OFF)
            oled.print("Off mode");
        else if (globalMode == AUTO)
            oled.print("Auto mode");
#endif

        // TITLE BAR
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print("SmartFarm");

        // STATUS BAR - Pump timer or OFF status
        oled.setCursor(0, 40);
        oled.print(displayStatus);
    }

    // ==========================================================================================
    // WIFI STATUS ICON DISPLAY
    // ==========================================================================================
    if (curr_state == iotwebconf::NotConfigured || curr_state == iotwebconf::ApMode)
        oled.drawBitmap(55, 0, wifi_ap, 9, 8, 1);

    else if (curr_state == iotwebconf::Connecting)
    {
        if (t_connecting == 1)
        {
            oled.drawBitmap(56, 0, wifi_on, 8, 8, 1);
            t_connecting = 0;
        }
        else
        {
            t_connecting = 1;
        }
    }
    else if (curr_state == iotwebconf::OnLine)
    {
        if (iot.status())
        {
            oled.drawBitmap(56, 0, wifi_on, 8, 8, 1);
        }
        else
        {
            oled.drawBitmap(56, 0, wifi_nointernet, 8, 8, 1);
        }
    }
    else if (curr_state == iotwebconf::OffLine)
        oled.drawBitmap(56, 0, wifi_off, 8, 8, 1);

    // RENDER DISPLAY TO OLED SCREEN
    oled.display();
}

// ==========================================================================================
// POPUP MESSAGE DISPLAY FUNCTION
// ==========================================================================================
void showPopupMessage(String msg, uint8_t timeout)
{
    popupStatus = msg;       // Store message text
    popupShowTimer = timeout;  // Set display duration in seconds
}

// ==========================================================================================
// HARDWARE OUTPUT UPDATE FUNCTION
// ==========================================================================================
void updateHardwareOutputs()
{
    // PUMP OUTPUT UPDATE
    // Check if pump state has changed by reading the actual pin state
    bool lastPumpState = (digitalRead(PUMP) == PUMP_ON);
    if (pumpState != lastPumpState)
    {
        if (pumpState)
        {
            // Pump ON: Set to OUTPUT mode and drive active state
            pinMode(PUMP, OUTPUT);
            digitalWrite(PUMP, PUMP_ON);
        }
        else
        {
            // Pump OFF: Set to INPUT mode (high impedance)
            pinMode(PUMP, INPUT);
            digitalWrite(PUMP, PUMP_OFF);
        }
        iot.eventUpdate("P", pumpState);  // Notify cloud platform of state change
    }
}

// ==========================================================================================
// SYSTEM STATE CALCULATION FUNCTION
// ==========================================================================================
uint32_t getSystemState()
{
    // Return current pump state: 1 = ON, 0 = OFF
    return (digitalRead(PUMP) == PUMP_ON) ? 1 : 0;
}

// ==========================================================================================
// RS485 PRE-TRANSMISSION CALLBACK
// ==========================================================================================
void preTransmission()
{
    // Configure RE and DE pins as outputs
    pinMode(MAX485_RE, OUTPUT);
    pinMode(MAX485_DE, OUTPUT);

    // Set both pins HIGH to enable transmission mode
    digitalWrite(MAX485_RE, 1);  // HIGH disables receiver (RE is active LOW)
    digitalWrite(MAX485_DE, 1);  // HIGH enables transmitter driver

    delay(1);  // Wait 1ms for MAX485 to stabilize
}

// ==========================================================================================
// RS485 POST-TRANSMISSION CALLBACK
// ==========================================================================================
void postTransmission()
{
    delay(3);  // Wait for transmission to complete

    // Set both pins LOW to enable reception mode
    digitalWrite(MAX485_RE, 0);  // LOW enables receiver
    digitalWrite(MAX485_DE, 0);  // LOW disables transmitter driver
}

// ==========================================================================================
// RS485 TRANSCEIVER DISABLE FUNCTION
// ==========================================================================================
void disConnect()
{
    // Set control pins to INPUT (high-impedance state) for low-power mode
    pinMode(MAX485_RE, INPUT);
    pinMode(MAX485_DE, INPUT);
}

// ==========================================================================================
// WEB SERVER ROOT PAGE HANDLER
// ==========================================================================================
void handleRoot()
{
    // Check for captive portal requests first
    if (iotWebConf.handleCaptivePortal())
    {
        return;
    }

    // Load HTML template and replace placeholders with actual values
    String s = FPSTR(htmlTemplate);

    s.replace("%STATE%", String(iotWebConf.getState()));
    s.replace("%THING_NAME%", String(iotWebConf.getThingName()));
    s.replace("%EMAIL%", String(emailParamValue));
    s.replace("%SSID%", String(iotWebConf.getSSID()));
    s.replace("%RSSI%", String(WiFi.RSSI()));
    s.replace("%ESP_ID%", String(iot.getClientId()));
    s.replace("%VERSION%", String(IOTVERSION));

    server.send(200, "text/html", s);
}

// ==========================================================================================
// CONFIGURATION SAVED CALLBACK
// ==========================================================================================
void configSaved()
{
    Serial.println("Configuration was updated.");
}

// ==========================================================================================
// WIFI CONNECTION SUCCESS CALLBACK
// ==========================================================================================
void wifiConnected()
{
    Serial.println("WiFi was connected.");

    // Start mDNS responder with device hostname
    MDNS.begin(iotWebConf.getThingName());
    MDNS.addService("http", "tcp", 80);

    Serial.printf("Ready! Open http://%s.local in your browser\n", String(iotWebConf.getThingName()));

    // Connect to CynoIOT cloud if email is provided
    if ((String)emailParamValue != "")
    {
        Serial.println("login with " + (String)emailParamValue);
        iot.connect((String)emailParamValue);
    }
}

// ==========================================================================================
// FORM VALIDATION CALLBACK
// ==========================================================================================
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper)
{
    Serial.println("Validating form.");
    bool valid = true;
    return valid;
}

// ==========================================================================================
// EEPROM CLEAR FUNCTION
// ==========================================================================================
void clearEEPROM()
{
    Serial.println("clearEEPROM() called!");

    EEPROM.begin(512);

    // Write 0 to all 512 bytes (complete erase)
    for (int i = 0; i < 512; i++)
    {
        EEPROM.write(i, 0);
    }

    EEPROM.end();

    server.send(200, "text/plain", "Clear all data\nrebooting");

    delay(1000);
    ESP.restart();
}

// ==========================================================================================
// DEVICE REBOOT FUNCTION
// ==========================================================================================
void reboot()
{
    server.send(200, "text/plain", "rebooting");

    delay(1000);
    ESP.restart();
}

// ==========================================================================================
// STATUS JSON ENDPOINT HANDLER
// ==========================================================================================
void handleStatus()
{
    // Build JSON string with current pump state
    String json = "{";
    json += "\"pump\":" + String(pumpState);
    json += "}";

    server.send(200, "application/json", json);
}
