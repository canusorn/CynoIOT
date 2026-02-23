/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*
 * Smart Farming Soil Sensor System
 * =================================
 * 
 * This Arduino sketch implements a comprehensive smart farming soil monitoring and irrigation control system.
 * It supports multiple sensor configurations and integrates with the CynoIOT cloud platform for remote monitoring.
 * 
 * Features:
 * - Multiple sensor model support (humidity, temperature, EC, pH, N, P, K)
 * - RS485/Modbus communication with soil sensors
 * - Automatic irrigation control with pump and 4-channel valve management
 * - Humidity-based cutoff protection
 * - WiFi connectivity with web-based configuration interface
 * - OLED display for local status monitoring
 * - EMA (Exponential Moving Average) filtering for smooth sensor readings
 * - Integration with CynoIOT cloud platform
 * 
 * Supported Hardware:
 * - ESP8266 or ESP32 microcontrollers
 * - MAX485 RS485 transceiver module
 * - Soil sensors (various models from basic to 7-in-1)
 * - OLED display (SSD1306, 128x64 pixels)
 * - Water pump (optional)
 * - 4-channel solenoid valves (optional)
 * 
 * Author: CynoIOT / IoTbundle
 * License: See project documentation
 * Version: 1.0
 */
/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

// ==========================================================================================
// SENSOR MODEL SELECTION
// ==========================================================================================
// Uncomment the sensor model that matches your hardware configuration.
// Only one model should be defined at a time.

// #define NOSENSOR_MODEL          // No sensors connected - basic pump/valve control only
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

// Output control pins for pump and valves
#define PUMP D1      // Water pump control pin
#define CH1 D2       // Channel 1 (solenoid valve 1) control pin
#define CH2 D3       // Channel 2 (solenoid valve 2) control pin
#define CH3 D4       // Channel 3 (solenoid valve 3) control pin
#define CH4 D5       // Channel 4 (solenoid valve 4) control pin
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

// Output control pins for pump and valves
#define PUMP 3        // Water pump control pin
#define CH1 7         // Channel 1 (solenoid valve 1) control pin
#define CH2 5         // Channel 2 (solenoid valve 2) control pin
#define CH3 11        // Channel 3 (solenoid valve 3) control pin
#define CH4 12        // Channel 4 (solenoid valve 4) control pin

#endif

// ==========================================================================================
// SYSTEM CONFIGURATION
// ==========================================================================================

// WiFi and Device Configuration
const char thingName[] = "SoilSensor";            // Device name for WiFi AP mode
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
                updatePinStatus('ch1', data.ch1);
                updatePinStatus('ch2', data.ch2);
                updatePinStatus('ch3', data.ch3);
                updatePinStatus('ch4', data.ch4);
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
        <div class="btn-group">
            <strong>CH1:</strong> <span id="ch1_status" class="status">Loading...</span><br>
            <button class="btn btn-on" onclick="togglePin('ch1', '1')">CH1 ON</button>
            <button class="btn btn-off" onclick="togglePin('ch1', '0')">CH1 OFF</button>
        </div>
        <div class="btn-group">
            <strong>CH2:</strong> <span id="ch2_status" class="status">Loading...</span><br>
            <button class="btn btn-on" onclick="togglePin('ch2', '1')">CH2 ON</button>
            <button class="btn btn-off" onclick="togglePin('ch2', '0')">CH2 OFF</button>
        </div>
        <div class="btn-group">
            <strong>CH3:</strong> <span id="ch3_status" class="status">Loading...</span><br>
            <button class="btn btn-on" onclick="togglePin('ch3', '1')">CH3 ON</button>
            <button class="btn btn-off" onclick="togglePin('ch3', '0')">CH3 OFF</button>
        </div>
        <div class="btn-group">
            <strong>CH4:</strong> <span id="ch4_status" class="status">Loading...</span><br>
            <button class="btn btn-on" onclick="togglePin('ch4', '1')">CH4 ON</button>
            <button class="btn btn-off" onclick="togglePin('ch4', '0')">CH4 OFF</button>
        </div>
    </div>
</body>
</html>
)rawliteral";

// -- ประกาศฟังก์ชัน
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
// GLOBAL VARIABLES - IRRIGATION CONTROL
// ==========================================================================================
uint16_t interval = 600;                          // Irrigation interval in seconds (default: 10 minutes)
uint32_t pumpTimer = 0;                           // Pump operation timer (seconds)
uint16_t chTimer[4] = {0, 0, 0, 0};               // Channel (valve) timers (seconds)
bool pumpState = false, chState[4] = {false, false, false, false};  // ON/OFF states for pump and channels
bool humidityCutoffApplied = false;               // Flag for humidity cutoff activation
String displayStatus = "";                        // Status string for display
String popupStatus = "";                          // Popup message for display
uint8_t popupShowTimer = 0;                       // Timer for popup message display

// ==========================================================================================
// GLOBAL VARIABLES - SYSTEM CONFIGURATION
// ==========================================================================================
bool pumpUse = false;                             // Enable/disable pump usage
uint8_t chUse = 0;                                // Number of channels in use (1-4)
const uint8_t pumpDelayConst = 5, overlapValveConst = 5;  // Timing constants (seconds)
uint8_t pumpDelayTimer = 0;                       // Pump startup delay timer
uint8_t humidLowCutoff = 10, humidHighCutoff = 40;        // Humidity thresholds (%)
uint32_t pumpOnProtectionTimer = 0;               // Safety timer for pump protection

// ==========================================================================================
// SYSTEM MODE DEFINITIONS
// ==========================================================================================
// Global operating modes
enum GlobalMode : uint8_t
{
    OFF = 1,    // Manual/off mode - no automatic operation
    AUTO = 2    // Automatic mode - responds to sensor values
};
GlobalMode globalMode = OFF;  // Current global mode (loaded from EEPROM)

// Working modes for irrigation system
enum WorkingMode : uint8_t
{
    NO_WORKING = 1,  // Idle state - no irrigation active
    SEQUENCE = 2     // Sequential irrigation mode - waters channels one after another
};
WorkingMode workingMode = NO_WORKING;  // Current working mode

// ==========================================================================================
// EVENT HANDLER
// ==========================================================================================
/**
 * @brief Handle events received from the CynoIOT server
 * 
 * This function processes incoming events from the cloud platform.
 * It updates system state based on the received event type and value.
 * Configuration changes are persisted to EEPROM.
 * 
 * @param event The event type (e.g., "SQ" for sequence start)
 * @param value The event value or parameter
 */
void handleEvent(String event, String value)
{
    EEPROM.begin(512);
    
    // ==========================================================================================
    // EVENT: SQ - Sequence Mode Control
    // ==========================================================================================
    // Starts or stops sequential irrigation mode
    // Value "1" = Start sequence mode (only if currently idle)
    // Value "0" = Stop sequence mode immediately
    if (event == "SQ")
    {
        Serial.println("Sequence: " + value);
        // iot.debug("Event SQ received with value: " + value);
        
        // Start sequence mode only if system is idle
        if (value == "1" && workingMode == NO_WORKING)
        {
            startSequenceMode();  // Begin sequential irrigation through all channels
            iot.debug("Sequence mode started by event SQ");
        }
        // Stop sequence mode immediately
        else if (value == "0")
        {
            offSeq();  // Turn off all pumps and valves
            iot.debug("Sequence mode stopped by event SQ");
        }
        else
        {
            iot.debug("Invalid value: " + value + "  workingmode: " + String(workingMode));
        }
    }
    
    // ==========================================================================================
    // EVENT: M - Global Mode Selection
    // ==========================================================================================
    // Sets the operating mode of the irrigation system
    // Value "auto" = Automatic mode - responds to sensor values and timers
    // Value "off" = Manual/off mode - no automatic operation
    else if (event == "M")
    {
        Serial.println("Mode: " + value);
        // iot.debug("Event M received with value: " + value);
        
        // Set to automatic mode
        if (value == "auto")
        {
            globalMode = AUTO;
            showPopupMessage("Set to\n\nAuto\n\nMode");
            iot.debug("Mode set to Auto by event M");
        }
        // Set to manual/off mode
        else // if (value == "off" || value == "null")
        {
            globalMode = OFF;
            showPopupMessage("Set to\n\nOff\n\nMode");
            iot.debug("Mode set to Off by event M");
        }

        // Persist mode setting to EEPROM (only if changed to reduce write cycles)
        uint8_t currentGlobalMode = EEPROM.read(500);
        if (currentGlobalMode != (uint8_t)globalMode)
        {
            EEPROM.write(500, globalMode);
            EEPROM.commit();
            iot.debug("Global mode setting saved to EEPROM");
        }
        else
        {
            iot.debug("Global mode value unchanged, skipping EEPROM write");
        }
    }
    
    // ==========================================================================================
    // EVENT: In - Irrigation Interval Setting
    // ==========================================================================================
    // Sets the time interval for automatic irrigation cycles
    // Value = Time in seconds (e.g., "600" = 10 minutes)
    // This is stored in EEPROM and used for timing calculations
    else if (event == "In")
    {
        Serial.println("Interval: " + value);
        // iot.debug("Event In received with value: " + value);
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
            iot.debug("Interval set to: " + String(interval) + " seconds from event");
        }
        else
        {
            Serial.println("Value already matches, skipping write");
            iot.debug("Interval value already matches EEPROM, skipping write");
        }
    }
    
    // ==========================================================================================
    // EVENT: P - Pump Manual Control
    // ==========================================================================================
    // Manual control for the water pump (only works in NO_WORKING mode)
    // Value "1" = Turn pump ON
    // Value "0" = Turn pump OFF
    // In SEQUENCE mode, returns current pump state instead (no control)
    else if (event == "P")
    {
        // iot.debug("Event P received with value: " + value);
        if (workingMode == NO_WORKING)  // Only allow manual control when idle
        {
            // Turn pump ON
            if (value == "1")
            {
                pumpState = 1;
                iot.eventUpdate("P", 1);  // Notify server of state change
                showPopupMessage("Pump\n\nOn");
                iot.debug("Pump turned ON from event");
            }
            // Turn pump OFF
            else if (value == "0")
            {
                pumpTimer = 0;  // Reset pump timer
                pumpState = 0;
                iot.eventUpdate("P", 0);  // Notify server of state change
                showPopupMessage("Pump\n\nOff");
                iot.debug("Pump turned OFF from event");
            }
        }
        // In SEQUENCE mode, pump is controlled automatically - return current state
        else
        {
            iot.eventUpdate("P", digitalRead(PUMP));
            iot.debug("Pump in SEQUENCE mode, not change");
        }
    }

    
    // ==========================================================================================
    // EVENT: c1, c2, c3, c4 - Channel (Valve) Manual Control
    // ==========================================================================================
    // Manual control for individual irrigation channels/valves
    // Only works in NO_WORKING mode when channels are not in sequence
    // Value "1" or non-zero = Turn channel ON
    // Value "0" = Turn channel OFF
    // Each channel is only available if chUse is set appropriately
    
    else if (event == "c1")  // Channel 1 control
    {
        // iot.debug("Event c1 received with value: " + value);
        if (workingMode == NO_WORKING)
        {
            // Turn channel ON (only if channel 1 is enabled)
            if (chUse >= 1 && value.toInt())
            {
                chState[0] = 1;
                iot.eventUpdate("c1", 1);
                showPopupMessage("Ch1 On");
                iot.debug("Channel 1 turned ON from event");
            }
            // Turn channel OFF
            else if (value.toInt() == 0)
            {
                chTimer[0] = 0;  // Reset channel timer
                chState[0] = 0;
                iot.eventUpdate("c1", 0);
                showPopupMessage("Ch1 Off");
                iot.debug("Channel 1 turned OFF from event");
            }
        }
        else
        {
            iot.eventUpdate("c1", digitalRead(CH1));
            iot.debug("Channel 1 in SEQUENCE mode, not change");
        }
    }
    else if (event == "c2")  // Channel 2 control
    {
        // iot.debug("Event c2 received with value: " + value);
        if (workingMode == NO_WORKING)
        {
            // Turn channel ON (only if channel 2 is enabled - requires chUse >= 2)
            if (chUse >= 2 && value.toInt())
            {
                chState[1] = 1;
                iot.eventUpdate("c2", 1);
                showPopupMessage("Ch2 On");
                iot.debug("Channel 2 turned ON from event");
            }
            // Turn channel OFF
            else if (value.toInt() == 0)
            {
                chTimer[1] = 0;
                chState[1] = 0;
                iot.eventUpdate("c2", 0);
                showPopupMessage("Ch2 Off");
                iot.debug("Channel 2 turned OFF from event");
            }
        }
        else
        {
            iot.eventUpdate("c2", digitalRead(CH2));
            iot.debug("Channel 2 in SEQUENCE mode, not change");
        }
    }
    else if (event == "c3")  // Channel 3 control
    {
        // iot.debug("Event c3 received with value: " + value);
        if (workingMode == NO_WORKING)
        {
            // Turn channel ON (only if channel 3 is enabled - requires chUse >= 3)
            if (chUse >= 3 && value.toInt())
            {
                chState[2] = 1;
                iot.eventUpdate("c3", 1);
                showPopupMessage("Ch3 On");
                iot.debug("Channel 3 turned ON from event");
            }
            // Turn channel OFF
            else if (value.toInt() == 0)
            {
                chTimer[2] = 0;
                chState[2] = 0;
                iot.eventUpdate("c3", 0);
                showPopupMessage("Ch3 Off");
                iot.debug("Channel 3 turned OFF from event");
            }
        }
        else
        {
            iot.eventUpdate("c3", digitalRead(CH3));
            iot.debug("Channel 3 in SEQUENCE mode, not change");
        }
    }
    else if (event == "c4")  // Channel 4 control
    {
        // iot.debug("Event c4 received with value: " + value);
        if (workingMode == NO_WORKING)
        {
            // Turn channel ON (only if channel 4 is enabled - requires chUse >= 4)
            if (chUse >= 4 && value.toInt())
            {
                chState[3] = 1;
                iot.eventUpdate("c4", 1);
                showPopupMessage("Ch4 On");
                iot.debug("Channel 4 turned ON");
            }
            // Turn channel OFF
            else if (value.toInt() == 0)
            {
                chTimer[3] = 0;
                chState[3] = 0;
                iot.eventUpdate("c4", 0);
                showPopupMessage("Ch4 Off");
                iot.debug("Channel 4 turned OFF");
            }
        }
        else
        {
            iot.eventUpdate("c4", digitalRead(CH4));
            iot.debug("Channel 4 in SEQUENCE mode, not change");
        }
    }
    
    // ==========================================================================================
    // EVENT: Pu - Pump Usage Configuration
    // ==========================================================================================
    // Enables or disables the pump functionality
    // Value "1" = Pump enabled (will be used in irrigation cycles)
    // Value "0" = Pump disabled (valves only, no pump)
    // This is useful for gravity-fed systems or when pump is not needed
    else if (event == "Pu")
    {
        Serial.println("Pump use : " + value);
        pumpUse = (bool)value.toInt();
        showPopupMessage(String("Pump\n\nUse\n\n") + String(value.toInt() != 0 ? "On" : "Off"));

        // Persist to EEPROM (only if changed)
        uint8_t currentPumpUse = EEPROM.read(497);
        if (currentPumpUse != (uint8_t)pumpUse)
        {
            EEPROM.write(497, (uint8_t)pumpUse);
            EEPROM.commit();
            iot.debug("Pump use setting saved to EEPROM from event");
        }
        else
        {
            iot.debug("Pump use value unchanged, skipping EEPROM write");
        }
    }
    
    // ==========================================================================================
    // EVENT: Cu - Number of Channels in Use Configuration
    // ==========================================================================================
    // Sets how many irrigation channels (valves) are available
    // Value "1" = Use only channel 1
    // Value "2" = Use channels 1 and 2
    // Value "3" = Use channels 1, 2, and 3
    // Value "4" = Use all 4 channels
    // This allows the system to be configured for different hardware setups
    else if (event == "Cu")
    {
        Serial.println("CH use : " + value + "ch");
        // iot.debug("Event Cu received with value: " + value);
        chUse = value.toInt();  // Set number of active channels (1-4)
        showPopupMessage(String("Ch\n\nUse\n\n") + String(value.toInt()));

        // Persist to EEPROM (only if changed)
        uint8_t currentChUse = EEPROM.read(496);
        if (currentChUse != (uint8_t)chUse)
        {
            EEPROM.write(496, (uint8_t)chUse);
            EEPROM.commit();
            iot.debug("Channel use " + String(chUse) + ", setting saved to EEPROM from event");
        }
        else
        {
            iot.debug("Channel use value unchanged, skipping EEPROM write");
        }
    }
    
    // ==========================================================================================
    // EVENT: Hl - Humidity Low Cutoff Threshold
    // ==========================================================================================
    // Sets the minimum soil moisture threshold
    // When humidity drops below this level, it may trigger irrigation (in AUTO mode)
    // Value = Humidity percentage (e.g., "10" = 10%)
    // This prevents irrigation when soil is already sufficiently moist
    else if (event == "Hl")
    {
        Serial.println("Humid low cutoff : " + value);
        // iot.debug("Event Hl received with value: " + value);
        humidLowCutoff = (uint8_t)value.toInt();
        showPopupMessage(String("Humid Low\n\nCutoff to\n\n") + value + String("%"));

        // Persist to EEPROM (only if changed)
        uint8_t currentHumidLowCutoff = EEPROM.read(489);
        if (currentHumidLowCutoff != (uint8_t)humidLowCutoff)
        {
            EEPROM.write(489, (uint8_t)humidLowCutoff);
            EEPROM.commit();
            iot.debug("Humidity low cutoff set to: " + String(humidLowCutoff) + "% from event");
        }
        else
        {
            iot.debug("Humidity low cutoff value unchanged, skipping EEPROM write");
        }
    }
    
    // ==========================================================================================
    // EVENT: Hh - Humidity High Cutoff Threshold
    // ==========================================================================================
    // Sets the maximum soil moisture threshold
    // When humidity reaches this level during irrigation, the cycle will stop early
    // Value = Humidity percentage (e.g., "40" = 40%)
    // This prevents over-watering by stopping irrigation when soil is sufficiently wet
    else if (event == "Hh")
    {
        Serial.println("Humid high cutoff : " + value);
        // iot.debug("Event Hh received with value: " + value);
        humidHighCutoff = (uint8_t)value.toInt();
        showPopupMessage(String("Humid High\n\nCutoff to\n\n") + value + String("%"));

        // Persist to EEPROM (only if changed)
        uint8_t currentHumidHighCutoff = EEPROM.read(488);
        if (currentHumidHighCutoff != (uint8_t)humidHighCutoff)
        {
            EEPROM.write(488, (uint8_t)humidHighCutoff);
            EEPROM.commit();
            iot.debug("Humidity high cutoff set to: " + String(humidHighCutoff) + "% from event");
        }
        else
        {
            iot.debug("Humidity high cutoff value unchanged, skipping EEPROM write");
        }
    }
    
    EEPROM.end();  // Close EEPROM session
}

// ==========================================================================================
// IOT SETUP FUNCTION
// ==========================================================================================
/**
 * @brief Initialize CynoIOT connection and load configuration from EEPROM
 * 
 * This function performs the following tasks:
 * 1. Loads saved configuration settings from EEPROM
 * 2. Sets default values if EEPROM is empty (first boot)
 * 3. Registers the event callback function for cloud communication
 * 4. Configures the data template based on the selected sensor model
 * 5. Sets up variable names for cloud data transmission
 * 
 * EEPROM Memory Map:
 * - Address 488: humidHighCutoff (uint8_t)
 * - Address 489: humidLowCutoff (uint8_t)
 * - Address 496: chUse (uint8_t) - Number of channels in use
 * - Address 497: pumpUse (uint8_t) - Pump enable flag
 * - Address 498-499: interval (uint16_t) - Irrigation interval in seconds
 * - Address 500: globalMode (uint8_t) - Operating mode (OFF=1, AUTO=2)
 */
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

    // Load pump usage configuration from EEPROM
    pumpUse = (uint8_t)EEPROM.read(497);
    if (pumpUse == 255) // 0xFF indicates uninitialized EEPROM
    {
        Serial.println("Load pumpUse = " + String(pumpUse) + ", pumpUse not found in EEPROM, using default value");
        pumpUse = 0;  // Default: Pump disabled
    }
    Serial.println("pumpUse: " + String(pumpUse));

    // Load number of channels in use from EEPROM
    chUse = (uint8_t)EEPROM.read(496);
    if (chUse == 255) // 0xFF indicates uninitialized EEPROM
    {
        Serial.println("Load chUse = " + String(chUse) + ", chUse not found in EEPROM, using default value");
        chUse = 1;  // Default: 1 channel
    }
    Serial.println("chUse: " + String(chUse));

    // Load humidity low cutoff threshold from EEPROM
    humidLowCutoff = (uint8_t)EEPROM.read(489);
    if (humidLowCutoff == 255) // 0xFF indicates uninitialized EEPROM
    {
        Serial.println("Load humidLowCutoff = " + String(humidLowCutoff) + ", humidLowCutoff not found in EEPROM, using default value = 40%");
        humidLowCutoff = 10;  // Default: 10%
    }
    Serial.println("humidLowCutoff: " + String(humidLowCutoff));

    // Load humidity high cutoff threshold from EEPROM
    humidHighCutoff = (uint8_t)EEPROM.read(488);
    if (humidHighCutoff == 255) // 0xFF indicates uninitialized EEPROM
    {
        Serial.println("Load humidHighCutoff = " + String(humidHighCutoff) + ", humidHighCutoff not found in EEPROM, using default value = 60%");
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
// The template and variable names are configured based on the defined sensor model.
// This determines what data is sent to the CynoIOT cloud platform.

#ifdef NOSENSOR_MODEL
    // Basic configuration - no sensors, only on/off state
    numVariables = 1;
    String keyname[numVariables] = {"on"};           // Variable names for cloud
    iot.setTemplate("smart_farm_nosensor", version); // Select corresponding template
    
#elif defined(HUMID_MODEL)
    // Humidity sensor only
    numVariables = 2;
    String keyname[numVariables] = {"on", "humid"};  // Variable names: state, humidity
    iot.setTemplate("smart_farm_humid", version);    // Select corresponding template
    
#elif defined(TEMP_HUMID_MODEL)
    // Temperature and humidity sensors
    numVariables = 3;
    String keyname[numVariables] = {"on", "humid", "temp"}; // Variable names: state, humidity, temperature
    iot.setTemplate("smart_farm_temp_humid", version);      // Select corresponding template
    
#elif defined(TEMP_HUMID_EC_MODEL)
    // Temperature, humidity, and EC (conductivity) sensors
    numVariables = 4;
    String keyname[numVariables] = {"on", "humid", "temp", "ec"}; // Variable names: state, humidity, temperature, EC
    iot.setTemplate("smart_farm_temp_humid_ec", version);         // Select corresponding template
    
#else // ALL_7IN1_MODEL (default)
    // Full 7-in-1 sensor: humidity, temperature, EC, pH, N, P, K
    numVariables = 8;
    String keyname[numVariables] = {"on", "humid", "temp", "ec", "ph", "n", "p", "k"}; // All 7 sensor values
    iot.setTemplate("smart_farm_7in1", version);                                       // Select corresponding template
#endif

    // Register variable names with CynoIOT
    iot.setkeyname(keyname, numVariables);

    // Display client ID for debugging
    Serial.println("ClinetID:" + String(iot.getClientId()));
}

// ฟังก์ชันที่ทำงานทุก 1 วินาที
void time1sec()
{

    // ตรวจสอบการเชื่อมต่อกับเซิร์ฟเวอร์
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

    // รีคอนเน็ค WiFi ถ้าไม่สามารถเชื่อมต่อกับเซิร์ฟเวอร์ได้
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
/**
 * @brief Initialize all hardware, systems, and connections
 * 
 * This function runs once when the device powers on or resets.
 * It performs the following initialization tasks:
 * 1. Initialize serial communication for debugging
 * 2. Initialize RS485 serial for sensor communication
 * 3. Initialize OLED display and show startup logo
 * 4. Check for EEPROM clear condition (RSTPIN held low)
 * 5. Configure all GPIO pins for pump and valve control
 * 6. Set up WiFi configuration portal with IotWebConf
 * 7. Configure web server endpoints for control and monitoring
 * 8. Initialize Modbus communication for soil sensor
 * 9. Set up CynoIOT cloud connection
 */
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

    // ==========================================================================================
    // OLED DISPLAY INITIALIZATION
    // ==========================================================================================
    // Initialize OLED display (I2C address 0x3C)
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    oled.clearDisplay();
    oled.drawBitmap(16, 5, logo_bmp, 33, 30, 1); // Display CynoIOT logo
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 40);
    oled.print("  CYNOIOT");
    oled.display();

    // ==========================================================================================
    // EEPROM CLEAR FUNCTION
    // ==========================================================================================
    // If RSTPIN is held low during boot, clear all EEPROM data
    // This is useful for resetting the device to factory defaults
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

    // ==========================================================================================
    // GPIO INITIALIZATION
    // ==========================================================================================
    // Set all control pins to known state (LOW) before configuring as outputs
    digitalWrite(PUMP, LOW);
    digitalWrite(CH1, LOW);
    digitalWrite(CH2, LOW);
    digitalWrite(CH3, LOW);
    digitalWrite(CH4, LOW);

    // Configure pump and valve control pins as outputs
    pinMode(PUMP, OUTPUT);
    pinMode(CH1, OUTPUT);
    pinMode(CH2, OUTPUT);
    pinMode(CH3, OUTPUT);
    pinMode(CH4, OUTPUT);

    // ==========================================================================================
    // IOT WEBCONF CONFIGURATION
    // ==========================================================================================
    // Add email parameter to configuration form
    login.addItem(&emailParam);

    // Optional: Set status LED pin for ESP32-S2
#ifdef CONFIG_IDF_TARGET_ESP32S2
    iotWebConf.setStatusPin(15);
#endif

    // Configure IotWebConf callbacks and parameters
    // iotWebConf.setConfigPin(CONFIG_PIN);  // Uncomment to use custom config pin
    iotWebConf.addParameterGroup(&login);  // Add login parameter group
    iotWebConf.setConfigSavedCallback(&configSaved);  // Called when config is saved
    iotWebConf.setFormValidator(&formValidator);  // Custom form validation
    iotWebConf.getApTimeoutParameter()->visible = false;  // Hide AP timeout setting
    iotWebConf.setWifiConnectionCallback(&wifiConnected);  // Called when WiFi connects

    // ==========================================================================================
    // OTA UPDATE SERVER SETUP
    // ==========================================================================================
    // Configure HTTP update server for remote firmware updates
    iotWebConf.setupUpdateServer(
        [](const char *updatePath)
        {
            httpUpdater.setup(&server, updatePath);  // Setup update handler
        },
        [](const char *userName, char *password)
        {
            httpUpdater.updateCredentials(userName, password);  // Set update credentials
        });

    // ==========================================================================================
    // INITIALIZE IOT WEBCONF
    // ==========================================================================================
    iotWebConf.init();  // Start WiFi configuration portal

    // ==========================================================================================
    // WEB SERVER URL HANDLERS
    // ==========================================================================================
    // Main page - displays status and controls
    server.on("/", handleRoot);
    
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
    
    // Manual channel 1 control via HTTP
    server.on("/gpio/ch1", []()
              {
                  String state = server.arg("state");
                  if (state == "1")
                      chState[0] = 1;
                  else if (state == "0")
                      chState[0] = 0;
                  updateHardwareOutputs();  // Apply changes to hardware
                  server.send(200, "text/plain", "OK");
              });
    
    // Manual channel 2 control via HTTP
    server.on("/gpio/ch2", []()
              {
                  String state = server.arg("state");
                  if (state == "1")
                      chState[1] = 1;
                  else if (state == "0")
                      chState[1] = 0;
                  updateHardwareOutputs();  // Apply changes to hardware
                  server.send(200, "text/plain", "OK");
              });
    
    // Manual channel 3 control via HTTP
    server.on("/gpio/ch3", []()
              {
                  String state = server.arg("state");
                  if (state == "1")
                      chState[2] = 1;
                  else if (state == "0")
                      chState[2] = 0;
                  updateHardwareOutputs();  // Apply changes to hardware
                  server.send(200, "text/plain", "OK");
              });
    
    // Manual channel 4 control via HTTP
    server.on("/gpio/ch4", []()
              {
                  String state = server.arg("state");
                  if (state == "1")
                      chState[3] = 1;
                  else if (state == "0")
                      chState[3] = 0;
                  updateHardwareOutputs();  // Apply changes to hardware
                  server.send(200, "text/plain", "OK");
              });
    
    // Status endpoint - returns JSON with current pump and valve states
    server.on("/status", handleStatus);
    
    // 404 handler - let IotWebConf handle unknown URLs
    server.onNotFound([]()
                      { iotWebConf.handleNotFound(); });

    Serial.println("Ready.");

    // ==========================================================================================
    // MODBUS SENSOR INITIALIZATION
    // ==========================================================================================
    // Set up callbacks for RS485 transceiver control
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
/**
 * @brief Main program loop - runs continuously
 * 
 * This function runs repeatedly in an infinite loop and handles:
 * 1. WiFi configuration portal processing (IotWebConf)
 * 2. Web server request handling
 * 3. CynoIOT cloud communication
 * 4. mDNS updates (ESP8266 only)
 * 5. Timed tasks (every 1 second):
 *    - System state updates
 *    - Display refresh
 *    - Sensor reading (every 5 seconds)
 * 6. Hardware output updates
 * 7. Safety check - ensures pump doesn't run without open valve
 */
void loop()
{
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
        time1sec();           // Check network status, auto-irrigation logic
        updateSystemState();  // Update irrigation timers and states
        updateDisplay();      // Refresh OLED display

        // ==========================================================================================
        // SENSOR READING (Every 5 Seconds)
        // ==========================================================================================
        // Read sensors and send data to cloud every 5 seconds
        // This prevents flooding the cloud with too frequent updates
        if (sampleUpdate >= 5)
        {
            readAndSendSensorData();  // Read sensors and send to CynoIOT
            sampleUpdate = 0;         // Reset counter
        }
    }

    // ==========================================================================================
    // HARDWARE OUTPUT UPDATE
    // ==========================================================================================
    // Apply the current state (pumpState, chState[]) to the actual GPIO pins
    updateHardwareOutputs();

    // ==========================================================================================
    // SAFETY PROTECTION - Pump Without Valve Detection
    // ==========================================================================================
    // Prevent pump from running without any valve open
    // This protects the pump from damage due to dead-head pressure
    // Conditions that trigger auto-opening of channel 1:
    // 1. Pump is ON (digitalRead returns HIGH)
    // 2. No active channel timers (no irrigation in progress)
    // 3. All valves are physically closed (all GPIO pins read LOW)
    // 4. Channels are enabled (chUse > 0)
    if (digitalRead(PUMP) && !chTimer[0] && !chTimer[1] && !chTimer[2] && !chTimer[3] && !digitalRead(CH1) && !digitalRead(CH2) && !digitalRead(CH3) && !digitalRead(CH4) && chUse)
    {
        chState[0] = 1;  // Force open channel 1 to protect pump
    }
}

// ==========================================================================================
// SYSTEM STATE UPDATE FUNCTION
// ==========================================================================================
/**
 * @brief Update irrigation system state and timers
 * 
 * This function is called every second to manage the irrigation sequence.
 * It handles:
 * 1. Humidity-based cutoff - stops irrigation early if soil is too wet
 * 2. Pump delay countdown - ensures pump starts before valves
 * 3. Pump timer management - controls pump operation duration
 * 4. Channel/valve sequential timing - opens valves one after another
 * 5. Valve overlap management - smoothly transitions between valves
 * 6. Display status updates - provides visual feedback
 * 
 * The sequence works as follows:
 * - Pump starts first (with delay if configured)
 * - Channel 1 opens, runs for configured time
 * - Channel 2 opens before Channel 1 closes (overlap period)
 * - This pattern repeats through all active channels
 * - Pump turns off after all channels complete
 */
void updateSystemState()
{
    displayStatus = "";  // Clear display status string

#if !defined(NOSENSOR_MODEL) // Only execute if sensors are present
    
    // ==========================================================================================
    // HUMIDITY CUTOFF PROTECTION
    // ==========================================================================================
    // If soil moisture reaches the high cutoff threshold during irrigation,
    // immediately stop the current valve and adjust remaining valves proportionally.
    // This prevents over-watering when soil is already sufficiently moist.
    //
    // Conditions for cutoff check:
    // 1. Channel 1 timer is running OR pump is running without channels (pump-only mode)
    // 2. System is in SEQUENCE working mode
    //
    // The -3 hysteresis (humidity < humidHighCutoff - 3) prevents rapid on/off cycling
    // when humidity is near the threshold.
    
    if ((chTimer[0] || (pumpTimer && !chUse)) && workingMode == SEQUENCE)
    {
        // Check if humidity has reached the high cutoff threshold
        if (humidity >= humidHighCutoff && !humidityCutoffApplied)
        {
            // Calculate how much time had elapsed on channel 1
            uint16_t valve1Time = interval - chTimer[0];
            
            // Immediately advance channel 1 timer to near completion (overlap + 1)
            // This will cause it to close on next iteration
            chTimer[0] = overlapValveConst + 1;
            humidityCutoffApplied = true; // Mark that cutoff has been applied

            // Adjust remaining channels to match the same proportion
            // If channel 1 only ran for 50% of its time, all other channels
            // will be reduced to 50% of their original times
            if (chUse >= 2 && chTimer[1] > 0)
                chTimer[1] = valve1Time;
            if (chUse >= 3 && chTimer[2] > 0)
                chTimer[2] = valve1Time;
            if (chUse >= 4 && chTimer[3] > 0)
                chTimer[3] = valve1Time;

            // Recalculate pump timer to match the reduced channel times
            if (pumpUse)
            {
                // New pump time = sum of all channel times - pump delay - overlap adjustment
                int32_t pumpNewTimer = chTimer[0] + chTimer[1] + chTimer[2] + chTimer[3] - pumpDelayConst - 3;
                if (pumpNewTimer < 0)
                    pumpTimer = 0;  // Ensure pump timer doesn't go negative
                else
                    pumpTimer = pumpNewTimer;
            }
        }
        // Reset cutoff flag when humidity drops 3% below threshold (hysteresis)
        // This prevents the cutoff from triggering repeatedly when humidity
        // is fluctuating near the threshold value
        else if (humidity < humidHighCutoff - 3)
        {
            humidityCutoffApplied = false;
        }
    }
    else
    {
        // Reset timers and flags when not in sequence mode
        pumpOnProtectionTimer = 0;
        humidityCutoffApplied = false;
    }
#endif

    // ==========================================================================================
    // SEQUENTIAL IRRIGATION MODE
    // ==========================================================================================
    // This mode waters channels one after another in sequence.
    // The pump starts first (with optional delay), then channels open sequentially.
    // Each channel overlaps with the next for smooth transitions.
    
    if (workingMode == SEQUENCE)
    {
        // --------------------------------------------------------------------------------------
        // PHASE 1: PUMP DELAY COUNTDOWN
        // --------------------------------------------------------------------------------------
        // Before the pump starts, there's an optional delay period.
        // This allows the system to prepare before pressurizing the water lines.
        // During this phase, "D" + countdown is displayed (e.g., "D5" for 5 seconds remaining)
        
        if (pumpDelayTimer)
        {
            pumpDelayTimer--;  // Count down the pump delay timer

            // Display different messages depending on whether channels are active
            if (chTimer[0] == 0 && chTimer[1] == 0 && chTimer[2] == 0 && chTimer[3] == 0)
            {
                displayStatus = "D" + String(pumpDelayTimer);  // Show countdown when no channels active
            }
            else
            {
                displayStatus = "D+";  // Show "D+" when channels are already active
            }
        }
        
        // --------------------------------------------------------------------------------------
        // PHASE 2: PUMP OPERATION
        // --------------------------------------------------------------------------------------
        // After the delay (if any), the pump turns on and runs for the calculated duration.
        // The pump timer counts down each second.
        // Display shows "P" + remaining seconds (e.g., "P120" for 2 minutes remaining)
        
        else if (pumpTimer && !pumpDelayTimer)
        {
            pumpState = 1;  // Turn on the pump

            // Different display for pump-only vs pump with channels
            if (chTimer[0] == 0 && chTimer[1] == 0 && chTimer[2] == 0 && chTimer[3] == 0)
                displayStatus = "P" + String(pumpTimer);  // Show pump timer when no channels
            else
                displayStatus = "P+";  // Show "P+" when channels are also active

            pumpTimer--;  // Decrement pump timer each second
        }
        
        // --------------------------------------------------------------------------------------
        // PHASE 3: PUMP SHUTOFF
        // --------------------------------------------------------------------------------------
        // When pump timer reaches zero, turn off the pump.
        // Check if all operations are complete to display "OFF" status.
        
        else if (!pumpTimer)
        {
            pumpState = 0;  // Turn off the pump
            
            // Display "OFF" only when pump and all channels are done
            if (!pumpTimer && !chTimer[0] && !chTimer[1] && !chTimer[2] && !chTimer[3])
            {
                displayStatus = "OFF";
            }
        }

        // --------------------------------------------------------------------------------------
        // PHASE 4: CHANNEL 1 OPERATION
        // --------------------------------------------------------------------------------------
        // Channel 1 is the first valve to open.
        // It runs for its configured time, then the next channel starts opening
        // during the overlap period to ensure smooth transition.
        
        if (chTimer[0])
        {
            chState[0] = 1;  // Turn on channel 1
            displayStatus += "C1:" + String(chTimer[0]);  // Show remaining time
            chTimer[0]--;  // Decrement timer

            // Open next valve when we reach the overlap time
            // This ensures the next valve starts opening before this one closes
            // Example: If overlapValveConst = 5, open channel 2 when 5 seconds remain
            if (chTimer[0] <= overlapValveConst && chTimer[1])
            {
                chState[1] = 1;  // Pre-open channel 2 for smooth transition
            }

            // When timer reaches zero, turn off channel 1
            if (chTimer[0] == 0)
            {
                chState[0] = 0;  // Turn off channel 1
            }
        }
        
        // --------------------------------------------------------------------------------------
        // PHASE 5: CHANNEL 2 OPERATION
        // --------------------------------------------------------------------------------------
        // Channel 2 operates after channel 1, with overlap period.
        // Ensures channel 1 is fully closed before this one becomes the primary.
        
        else if (chTimer[1])
        {
            chState[0] = 0;  // Ensure channel 1 is fully off
            chState[1] = 1;  // Turn on channel 2
            displayStatus += "C2:" + String(chTimer[1]);  // Show remaining time
            chTimer[1]--;  // Decrement timer

            // Open next valve when we reach the overlap time
            if (chTimer[1] <= overlapValveConst && chTimer[2])
            {
                chState[2] = 1;  // Pre-open channel 3 for smooth transition
            }

            // When timer reaches zero, turn off channel 2
            if (chTimer[1] == 0)
                chState[1] = 0;  // Turn off channel 2
        }
        
        // --------------------------------------------------------------------------------------
        // PHASE 6: CHANNEL 3 OPERATION
        // --------------------------------------------------------------------------------------
        // Channel 3 operates after channel 2, with overlap period.
        
        else if (chTimer[2])
        {
            chState[1] = 0;  // Ensure channel 2 is fully off
            chState[2] = 1;  // Turn on channel 3
            displayStatus += "C3:" + String(chTimer[2]);  // Show remaining time
            chTimer[2]--;  // Decrement timer

            // Open next valve when we reach the overlap time
            if (chTimer[2] <= overlapValveConst && chTimer[3])
            {
                chState[3] = 1;  // Pre-open channel 4 for smooth transition
            }

            // When timer reaches zero, turn off channel 3
            if (chTimer[2] == 0)
                chState[2] = 0;  // Turn off channel 3
        }
        
        // --------------------------------------------------------------------------------------
        // PHASE 7: CHANNEL 4 OPERATION
        // --------------------------------------------------------------------------------------
        // Channel 4 is the last valve. No overlap needed after this one.
        
        else if (chTimer[3])
        {
            chState[2] = 0;  // Ensure channel 3 is fully off
            chState[3] = 1;  // Turn on channel 4
            displayStatus += "C4:" + String(chTimer[3]);  // Show remaining time
            chTimer[3]--;  // Decrement timer

            // When timer reaches zero, turn off channel 4
            if (chTimer[3] == 0)
                chState[3] = 0;  // Turn off channel 4
        }

        // --------------------------------------------------------------------------------------
        // PHASE 8: SEQUENCE COMPLETION
        // --------------------------------------------------------------------------------------
        // When all timers reach zero, the irrigation cycle is complete.
        // Reset working mode to idle and notify the cloud platform.
        
        if (!pumpTimer && !chTimer[0] && !chTimer[1] && !chTimer[2] && !chTimer[3])
        {
            workingMode = NO_WORKING;  // Return to idle mode
            iot.eventUpdate("SQ", 0);  // Notify cloud that sequence is complete
        }
    }
    // ==========================================================================================
    // SAFETY PROTECTION - Pump Runaway Detection
    // ==========================================================================================
    // This is a safety mechanism to prevent pump damage.
    // If the pump is running but the system is NOT in SEQUENCE mode,
    // it means the pump was turned on manually or something went wrong.
    // 
    // Protection logic:
    // - If pump runs for 5x the configured interval without being in sequence mode,
    //   automatically turn off all outputs to protect the pump
    // - This prevents dead-head operation (pump running without open valves)
    // - Example: If interval is 600 seconds, pump will be shut off after 3000 seconds
    
    // protection for forget to close valve
    else if (digitalRead(PUMP))  // Pump is running but we're not in sequence mode
    {
        pumpOnProtectionTimer++;  // Increment protection counter
        
        // If pump has been running too long (5x interval), shut everything down
        if (pumpOnProtectionTimer >= interval * 5)
        {
            pumpOnProtectionTimer = 0;  // Reset counter
            offSeq();  // Emergency shutoff - turn off pump and all valves
        }
    }
    else
    {
        pumpOnProtectionTimer = 0;  // Reset counter when pump is off
    }
    
    // ==========================================================================================
    // DEBUG OUTPUT (Commented Out)
    // ==========================================================================================
    // Uncomment these lines for detailed debugging of timer states
    // This can help visualize the irrigation sequence during development
    
    // Serial.println("---------------------------------------------------------");
    // Serial.println("param\tch1\tch2\tch3\tch4\tpump");
    // Serial.println("time \t" + String(chTimer[0]) + "\t" + String(chTimer[1]) + "\t" + String(chTimer[2]) + "\t" + String(chTimer[3]) + "\t" + String(pumpTimer));
    // // iot.debug("time \t" + String(chTimer[0]) + "\t" + String(chTimer[1]) + "\t" + String(chTimer[2]) + "\t" + String(chTimer[3]) + "\t" + String(pumpTimer));
    // Serial.println("state\t" + String(digitalRead(CH1)) + "\t" + String(digitalRead(CH2)) + "\t" + String(digitalRead(CH3)) + "\t" + String(digitalRead(CH4)) + "\t" + String(digitalRead(PUMP)));
    // // iot.debug("state\t" + String(digitalRead(CH1)) + "\t" + String(digitalRead(CH2)) + "\t" + String(digitalRead(CH3)) + "\t" + String(digitalRead(CH4)) + "\t" + String(digitalRead(PUMP)));
    // Serial.println("---------------------------------------------------------");
}
}

// ==========================================================================================
// EMA (EXPONENTIAL MOVING AVERAGE) FILTER - FLOAT VERSION
// ==========================================================================================
/**
 * @brief Apply Exponential Moving Average filter to smooth sensor readings
 * 
 * The EMA filter reduces noise in sensor readings by applying a weighted average
 * that gives more importance to recent readings while still considering historical data.
 * 
 * Mathematical Formula:
 * EMA = α × current_value + (1 - α) × previous_EMA
 * 
 * Where:
 * - α (alpha) = smoothing factor (0.0 to 1.0)
 * - current_value = new sensor reading
 * - previous_EMA = filtered value from previous iteration
 * 
 * Alpha Value Behavior (EMA_ALPHA = 0.1):
 * - Lower alpha (e.g., 0.1) = smoother output, slower response to changes
 * - Higher alpha (e.g., 0.5) = faster response, less smoothing
 * - α = 0.1 means: 10% weight to new reading, 90% weight to historical data
 * 
 * Initialization:
 * - On first reading, the filter is initialized with the raw value
 * - This prevents the filter from starting at zero and taking time to converge
 * 
 * Rounding:
 * - Results are rounded to 1 decimal place for consistency
 * - This reduces floating-point precision issues and output variability
 * 
 * Use Case:
 * - Used for float sensors: humidity (%), temperature (°C), pH
 * - These sensors have continuous values that benefit from smoothing
 * 
 * @param currentValue The new raw sensor reading to filter
 * @param emaValue The previous EMA value (historical data)
 * @param firstReading True if this is the first reading (filter initialization)
 * @return Filtered value rounded to 1 decimal place
 */
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
/**
 * @brief Apply Exponential Moving Average filter to integer sensor readings
 * 
 * This is the integer version of the EMA filter, designed for sensors that return
 * whole number values (conductivity, nitrogen, phosphorus, potassium).
 * 
 * Key Differences from Float Version:
 * - Accepts uint32_t input (integer sensor values)
 * - Casts to float for calculation precision
 * - Returns float for consistency with other sensor values
 * 
 * Mathematical Formula (same as float version):
 * EMA = α × current_value + (1 - α) × previous_EMA
 * 
 * Why Use This Version:
 * - Integer sensors like EC (µS/cm) and NPK (mg/kg) naturally return whole numbers
 * - However, EMA calculation requires floating-point arithmetic
 * - Result is returned as float to maintain precision in filtered values
 * 
 * Use Case:
 * - Used for integer sensors: conductivity (µS/cm), nitrogen (mg/kg), 
 *   phosphorus (mg/kg), potassium (mg/kg)
 * - These sensors measure discrete counts or concentrations
 * 
 * @param currentValue The new raw sensor reading (unsigned 32-bit integer)
 * @param emaValue The previous EMA value (historical data)
 * @param firstReading True if this is the first reading (filter initialization)
 * @return Filtered value as float, rounded to 1 decimal place
 */
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

// ------------------------------------------------------------------
// Read sensor data based on the defined sensor model
// ------------------------------------------------------------------

// ==========================================================================================
// SENSOR READING AND CLOUD DATA UPLOAD FUNCTION
// ==========================================================================================
/**
 * @brief Read soil sensor data and send to CynoIOT cloud platform
 * 
 * This function is called every 5 seconds (controlled by sampleUpdate counter).
 * It performs the following operations:
 * 
 * 1. Get current system state (remaining irrigation time)
 * 2. Read sensor data via RS485/Modbus communication
 * 3. Apply EMA filtering to smooth out sensor noise
 * 4. Print debug information to Serial Monitor
 * 5. Send filtered data to CynoIOT cloud platform
 * 
 * Sensor Reading Process:
 * - Uses Modbus protocol over RS485 communication
 * - Reads holding registers starting at address 0x0000
 * - Different sensor models require different numbers of registers
 * - Register count depends on which sensors are present:
 *   • 1 register: Humidity only
 *   • 2 registers: Humidity + Temperature
 *   • 3 registers: Humidity + Temperature + Conductivity
 *   • 7 registers: All 7 sensors (Humidity, Temp, EC, pH, N, P, K)
 * 
 * Data Processing:
 * - Raw values are scaled from Modbus registers:
 *   • Humidity: raw / 10.0 = %RH (0-100%)
 *   • Temperature: raw / 10.0 = °C
 *   • EC: raw = µS/cm
 *   • pH: raw / 10.0 = pH value (0-14)
 *   • N, P, K: raw = mg/kg
 * - EMA filter applied to all readings for noise reduction
 * - Filtered values stored in global variables
 * 
 * Error Handling:
 * - Checks Modbus communication result
 * - Logs errors via Serial and iot.debug()
 * - Continues operation even if sensor read fails
 * 
 * Cloud Data Format:
 * - First element is always the system state (remaining irrigation time)
 * - Subsequent elements are sensor values in defined order
 * - Data sent as float array to CynoIOT platform
 * 
 * @note This function uses conditional compilation (#if/#elif) to support
 *       multiple sensor configurations in the same firmware
 * @note EMA filter initializes on first successful sensor read
 * @note Sensor readings are in metric units (°C, %RH, µS/cm, mg/kg)
 */
void readAndSendSensorData()
{
    // Get current system state (remaining irrigation time in seconds)
    // This is the first value sent to cloud - shows how long until next cycle
    uint32_t onState = getSystemState();
    
    // ==========================================================================================
    // SENSOR MODEL SELECTION - COMPILATION TIME BRANCHING
    // ==========================================================================================
    // The code below uses conditional compilation to include only the sensor
    // reading code for the configured sensor model. This optimizes code size
    // by excluding unused sensor reading code.
    
#if defined(NOSENSOR_MODEL)
    // ==========================================================================================
    // NO SENSOR MODEL - Basic pump/valve control only
    // ==========================================================================================
    // No sensors connected - only send system state (on/off timer)
    // This is useful for systems that only need pump/valve control
    // without any soil moisture monitoring
    
    float payload[numVariables] = {onState};
    iot.update(payload);
    
#elif defined(HUMID_MODEL)
    // No sensors - only send the onState state
    float payload[numVariables] = {onState};
    iot.update(payload);
#elif defined(HUMID_MODEL)
    // ==========================================================================================
    // HUMIDITY MODEL - Single humidity sensor
    // ==========================================================================================
    // Reads only humidity from sensor at Modbus address 0x0000
    // Typical use case: Basic soil moisture monitoring
    
    uint8_t result = node.readHoldingRegisters(0x0000, 1); // Read 1 register: humidity
    disConnect();  // Disable RS485 transceiver after reading
    
    // Check if Modbus communication was successful
    if (result == node.ku8MBSuccess)
    {
        // Extract raw humidity value from Modbus response buffer
        // Register 0: Humidity (resolution 0.1 %RH, range 0-1000 = 0-100%)
        float rawHumidity = node.getResponseBuffer(0) / 10.0; // Convert to %RH
        
        // Apply EMA filter to smooth out sensor noise
        // First reading initializes the filter, subsequent readings apply smoothing
        humidity = applyEmaFilter(rawHumidity, emaHumidity, !emaInitialized);
        emaHumidity = humidity;  // Store for next filter iteration
        emaInitialized = true;   // Mark filter as initialized

        // Print sensor data to Serial Monitor for debugging
        Serial.println("----- Soil Parameters -----");
        Serial.print("Humidity  : ");
        Serial.print(humidity, 1);
        Serial.print(" %RH (raw: ");
        Serial.print(rawHumidity);
        Serial.println(")");

        // Prepare data array for cloud transmission
        // Format: [system_state, humidity]
        float payload[numVariables] = {onState, humidity};
        iot.update(payload);  // Send to CynoIOT cloud platform
    }
    else
    {
        // Modbus communication failed - log error
        Serial.println("Modbus error reading humidity!");
        iot.debug("error read humidity sensor");
    }
    
#elif defined(TEMP_HUMID_MODEL)
    // ==========================================================================================
    // TEMPERATURE + HUMIDITY MODEL - Two sensor configuration
    // ==========================================================================================
    // Reads humidity and temperature from sensor
    // Typical use case: Soil moisture monitoring with temperature compensation
    
    uint8_t result = node.readHoldingRegisters(0x0000, 2); // Read 2 registers: humidity, temperature
    disConnect();  // Disable RS485 transceiver after reading
    
    // Check if Modbus communication was successful
    if (result == node.ku8MBSuccess)
    {
        // Extract raw values from Modbus response buffer
        // Register 0: Humidity (resolution 0.1 %RH)
        // Register 1: Temperature (resolution 0.1 °C)
        float rawHumidity = node.getResponseBuffer(0) / 10.0;    // Convert to %RH
        float rawTemperature = node.getResponseBuffer(1) / 10.0; // Convert to °C
        
        // Apply EMA filter to both readings for noise reduction
        humidity = applyEmaFilter(rawHumidity, emaHumidity, !emaInitialized);
        temperature = applyEmaFilter(rawTemperature, emaTemperature, !emaInitialized);
        emaHumidity = humidity;      // Store for next filter iteration
        emaTemperature = temperature;  // Store for next filter iteration
        emaInitialized = true;        // Mark filters as initialized

        // Print sensor data to Serial Monitor for debugging
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

        // Prepare data array for cloud transmission
        // Format: [system_state, humidity, temperature]
        float payload[numVariables] = {onState, humidity, temperature};
        iot.update(payload);  // Send to CynoIOT cloud platform
    }
    else
    {
        // Modbus communication failed - log error
        Serial.println("Modbus error reading temp/humidity!");
        iot.debug("error read temp/humidity sensor");
    }
    
#elif defined(TEMP_HUMID_EC_MODEL)
    // ==========================================================================================
    // TEMPERATURE + HUMIDITY + EC MODEL - Three sensor configuration
    // ==========================================================================================
    // Reads humidity, temperature, and electrical conductivity
    // Typical use case: Complete soil monitoring with nutrient level indicator
    
    uint8_t result = node.readHoldingRegisters(0x0000, 3); // Read 3 registers: humidity, temperature, EC
    disConnect();  // Disable RS485 transceiver after reading
    
    // Check if Modbus communication was successful
    if (result == node.ku8MBSuccess)
    {
        // Extract raw values from Modbus response buffer
        // Register 0: Humidity (resolution 0.1 %RH)
        // Register 1: Temperature (resolution 0.1 °C)
        // Register 2: Electrical Conductivity (µS/cm, integer value)
        float rawHumidity = node.getResponseBuffer(0) / 10.0;    // Convert to %RH
        float rawTemperature = node.getResponseBuffer(1) / 10.0; // Convert to °C
        uint32_t rawConductivity = node.getResponseBuffer(2);    // µS/cm (no scaling needed)
        
        // Apply EMA filter to all readings
        // Note: conductivity uses integer version of EMA filter
        humidity = applyEmaFilter(rawHumidity, emaHumidity, !emaInitialized);
        temperature = applyEmaFilter(rawTemperature, emaTemperature, !emaInitialized);
        conductivity = (uint32_t)applyEmaFilterInt(rawConductivity, emaConductivity, !emaInitialized);
        emaHumidity = humidity;
        emaTemperature = temperature;
        emaConductivity = conductivity;
        emaInitialized = true;

        // Print sensor data to Serial Monitor for debugging
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

        // Prepare data array for cloud transmission
        // Format: [system_state, humidity, temperature, conductivity]
        float payload[numVariables] = {onState, humidity, temperature, conductivity};
        iot.update(payload);  // Send to CynoIOT cloud platform
    }
    else
    {
        // Modbus communication failed - log error
        Serial.println("Modbus error reading temp/humidity/EC!");
        iot.debug("error read temp/humidity/EC sensor");
    }
    
#else // ALL_7IN1_MODEL (default if no other model is defined)
#elif defined(ALL_7IN1_MODEL)
    // ==========================================================================================
    // ALL_7IN1_MODEL - Full 7-in-1 sensor configuration (DEFAULT)
    // ==========================================================================================
    // Reads all 7 parameters from the soil sensor:
    // 1. Humidity - Soil moisture content (%RH)
    // 2. Temperature - Soil temperature (°C)
    // 3. Electrical Conductivity - Nutrient concentration indicator (µS/cm)
    // 4. pH - Soil acidity/alkalinity level (0-14 pH)
    // 5. Nitrogen (N) - Primary macronutrient (mg/kg)
    // 6. Phosphorus (P) - Primary macronutrient (mg/kg)
    // 7. Potassium (K) - Primary macronutrient (mg/kg)
    //
    // Typical use case: Comprehensive smart farming with complete soil analysis
    // This is the default configuration if no sensor model is explicitly defined
    
    uint8_t result = node.readHoldingRegisters(0x0000, 7); // Read all 7 registers
    disConnect();  // Disable RS485 transceiver after reading
    
    // Check if Modbus communication was successful
    if (result == node.ku8MBSuccess)
    {
        // Extract all raw values from Modbus response buffer
        // Register 0: Humidity (resolution 0.1 %RH, range 0-1000 = 0-100%)
        float rawHumidity = node.getResponseBuffer(0) / 10.0;    // Convert to %RH
        // Register 1: Temperature (resolution 0.1 °C, range -500-1500 = -50-150°C)
        float rawTemperature = node.getResponseBuffer(1) / 10.0; // Convert to °C
        // Register 2: Electrical Conductivity (µS/cm, integer, typically 0-5000)
        uint32_t rawConductivity = node.getResponseBuffer(2);    // No scaling needed
        // Register 3: pH level (resolution 0.1 pH, range 0-140 = 0-14 pH)
        float rawPh = node.getResponseBuffer(3) / 10.0;          // Convert to pH
        // Register 4: Nitrogen content (mg/kg, integer, typically 0-200)
        uint32_t rawNitrogen = node.getResponseBuffer(4);        // No scaling needed
        // Register 5: Phosphorus content (mg/kg, integer, typically 0-200)
        uint32_t rawPhosphorus = node.getResponseBuffer(5);      // No scaling needed
        // Register 6: Potassium content (mg/kg, integer, typically 0-200)
        uint32_t rawPotassium = node.getResponseBuffer(6);       // No scaling needed

        // ==========================================================================================
        // APPLY EMA FILTER TO ALL SENSOR READINGS
        // ==========================================================================================
        // Apply exponential moving average filter to smooth out sensor noise
        // Float sensors (humidity, temperature, pH): use float EMA filter
        // Integer sensors (EC, N, P, K): use integer EMA filter (returns float for precision)
        
        humidity = applyEmaFilter(rawHumidity, emaHumidity, !emaInitialized);
        temperature = applyEmaFilter(rawTemperature, emaTemperature, !emaInitialized);
        conductivity = (uint32_t)applyEmaFilterInt(rawConductivity, emaConductivity, !emaInitialized);
        ph = applyEmaFilter(rawPh, emaPh, !emaInitialized);
        nitrogen = (uint32_t)applyEmaFilterInt(rawNitrogen, emaNitrogen, !emaInitialized);
        phosphorus = (uint32_t)applyEmaFilterInt(rawPhosphorus, emaPhosphorus, !emaInitialized);
        potassium = (uint32_t)applyEmaFilterInt(rawPotassium, emaPotassium, !emaInitialized);

        // Store filtered values back to EMA variables for next iteration
        emaHumidity = humidity;
        emaTemperature = temperature;
        emaConductivity = conductivity;
        emaPh = ph;
        emaNitrogen = nitrogen;
        emaPhosphorus = phosphorus;
        emaPotassium = potassium;
        emaInitialized = true;  // Mark all EMA filters as initialized

        // ==========================================================================================
        // PRINT SENSOR DATA TO SERIAL MONITOR
        // ==========================================================================================
        // Output all sensor readings in human-readable format for debugging
        // Shows both filtered value (after EMA) and raw value (before EMA)
        
        Serial.println("----- Soil Parameters -----");
        
        // Moisture and Temperature
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
        
        // Electrical Conductivity - indicator of dissolved salts/nutrients
        Serial.print("Conductivity: ");
        Serial.print(conductivity, 1);
        Serial.print(" µS/cm (raw: ");
        Serial.print(rawConductivity);
        Serial.println(")");
        
        // pH Level - acidity/alkalinity
        Serial.print("pH        : ");
        Serial.print(ph, 1);
        Serial.print(" (raw: ");
        Serial.print(rawPh);
        Serial.println(")");
        
        // NPK - Primary macronutrients
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

        // ==========================================================================================
        // SEND DATA TO CYNIIOT CLOUD PLATFORM
        // ==========================================================================================
        // Prepare data array with all sensor readings and system state
        // Format: [system_state, humidity, temperature, conductivity, pH, N, P, K]
        // This data is sent to the cloud for remote monitoring and historical tracking
        
        float payload[numVariables] = {onState, humidity, temperature, conductivity, ph, nitrogen, phosphorus, potassium};
        iot.update(payload);  // Transmit to CynoIOT cloud platform
    }
    else
    {
        // Modbus communication failed - log error
        // This could indicate sensor disconnection, wiring issues, or sensor failure
        Serial.println("Modbus error!");
        iot.debug("error read sensor");
    }
#endif
}

// ==========================================================================================
// OLED DISPLAY UPDATE FUNCTION
// ==========================================================================================
/**
 * @brief Update OLED display with current system status, sensor values, and notifications
 * 
 * This function manages all content displayed on the 128x64 pixel OLED screen.
 * It's called every second from the main loop to provide real-time feedback.
 * 
 * Display Priority System (highest to lowest):
 * 1. Network state changes (WiFi connection status notifications)
 * 2. CynoIOT cloud notifications (server messages)
 * 3. Popup messages (user-triggered feedback)
 * 4. Regular status display (sensor values, mode, timers)
 * 
 * Display Layout:
 * - Line 0 (top): "SmartFarm" title + WiFi status icon (top-right corner)
 * - Line 1-2: Main content (sensor values OR notification OR status text)
 * - Line 3-4: Status/timer information
 * 
 * Network State Notifications:
 * - Boot → NotConfigured: "no config stay in AP Mode"
 * - Boot → ApMode: "AP Mode for 30 sec"
 * - Connecting → ApMode: "can't connect wifi go AP Mode"
 * - Connecting → OnLine: "wifi connect success XX dBm"
 * - OnLine → OffLine: "wifi disconnect go AP Mode"
 * - ApMode → Connecting: "wifi connecting"
 * - OnLine → Connecting: "wifi disconnect reconnecting"
 * 
 * Icons Displayed:
 * - AP Mode: Router icon (9x8 pixels)
 * - Connecting: WiFi icon (8x8 pixels, blinking animation)
 * - OnLine with cloud: WiFi on icon (8x8 pixels)
 * - OnLine no cloud: WiFi with X icon (8x8 pixels)
 * - OffLine: WiFi off icon (8x8 pixels)
 * 
 * @note This function uses a state machine approach with prev_state tracking
 * @note displaytime variable controls how long notifications are shown (in seconds)
 * @see showPopupMessage() for displaying temporary popup messages
 * @see time1sec() which calls this function every second
 */
void updateDisplay()
{
    // ==========================================================================================
    // GET CURRENT NETWORK STATE
    // ==========================================================================================
    // Get the current WiFi/network connection state from IotWebConf
    // States: Boot, NotConfigured, ApMode, Connecting, OnLine, OffLine
    iotwebconf::NetworkState curr_state = iotWebConf.getState();
    
    // ==========================================================================================
    // NETWORK STATE CHANGE DETECTION
    // ==========================================================================================
    // Track state transitions to display appropriate notifications
    // Each state transition has a specific notification message and duration
    
    // BOOT state - Device just powered on
    if (curr_state == iotwebconf::Boot)
    {
        prev_state = curr_state;  // Initialize previous state
    }
    
    // NOT CONFIGURED state - No WiFi credentials saved, staying in AP mode
    else if (curr_state == iotwebconf::NotConfigured)
    {
        if (prev_state == iotwebconf::Boot)  // First time entering this state
        {
            displaytime = 5;  // Show for 5 seconds
            prev_state = curr_state;
            noti = "-State-\n\nno config\nstay in\nAP Mode";
        }
    }
    
    // AP MODE state - Configuration portal active
    // Device creates WiFi hotspot for user to connect and configure
    else if (curr_state == iotwebconf::ApMode)
    {
        if (prev_state == iotwebconf::Boot)  // Started in AP mode (first boot)
        {
            displaytime = 5;  // Show for 5 seconds
            prev_state = curr_state;
            noti = "-State-\n\nAP Mode\nfor 30 sec";
        }
        else if (prev_state == iotwebconf::Connecting)  // WiFi connection failed
        {
            displaytime = 5;  // Show for 5 seconds
            prev_state = curr_state;
            noti = "-State-\n\nX  can't\nconnect\nwifi\ngo AP Mode";
        }
    }
    
    // ONLINE state - Successfully connected to WiFi
    else if (curr_state == iotwebconf::OnLine)
    {
        if (prev_state == iotwebconf::Connecting)  // Just connected successfully
        {
            displaytime = 5;  // Show for 5 seconds
            prev_state = curr_state;
            // Show signal strength in dBm (typically -30 to -90)
            noti = "-State-\n\nwifi\nconnect\nsuccess\n" + String(WiFi.RSSI()) + " dBm";
        }
    }
    
    // OFFLINE state - WiFi connection lost
    else if (curr_state == iotwebconf::OffLine)
    {
        displaytime = 10;  // Show for 10 seconds (important notification)
        prev_state = curr_state;
        noti = "-State-\n\nX wifi\ndisconnect\ngo AP Mode";
    }
    
    // CONNECTING state - Attempting to connect to WiFi
    else if (curr_state == iotwebconf::Connecting)
    {
        if (prev_state == iotwebconf::ApMode)  // User finished config, now connecting
        {
            displaytime = 5;  // Show for 5 seconds
            prev_state = curr_state;
            noti = "-State-\n\nwifi\nconnecting";
        }
        else if (prev_state == iotwebconf::OnLine)  // Lost connection, reconnecting
        {
            displaytime = 10;  // Show for 10 seconds (important notification)
            prev_state = curr_state;
            noti = "-State-\n\nX  wifi\ndisconnect\nreconnecting";
        }
    }
    else if (curr_state == iotwebconf::OnLine)
    {
        if (prev_state == iotwebconf::Connecting)
        {
            displaytime = 5;
            prev_state = curr_state;
            noti = "-State-\n\nwifi\nconnect\nsuccess\n" + String(WiFi.RSSI()) + " dBm";
        }
    }

    
    // ==========================================================================================
    // CYNIIOT CLOUD NOTIFICATIONS
    // ==========================================================================================
    // Check for incoming notifications from CynoIOT cloud platform
    // These are server-generated messages (e.g., "New firmware available")
    // Only display if no other notification is currently being shown
    
    if (iot.noti != "" && displaytime == 0)
    {
        displaytime = 3;  // Show for 3 seconds
        noti = iot.noti;  // Get notification from cloud
        iot.noti = "";    // Clear notification after reading
    }

    // ==========================================================================================
    // DISPLAY NOTIFICATION MESSAGES
    // ==========================================================================================
    // If displaytime > 0, show the notification message
    // These are typically network state change notifications
    
    if (displaytime)
    {
        displaytime--;  // Count down display timer
        oled.clearDisplay();  // Clear screen
        oled.setTextSize(1);  // Use small text for messages
        oled.setCursor(0, 0);
        oled.print(noti);  // Display notification text
        Serial.println(noti);  // Also log to Serial Monitor
    }
    
    // ==========================================================================================
    // DISPLAY POPUP MESSAGES
    // ==========================================================================================
    // Popup messages have priority over regular display
    // These are triggered by user actions (button presses, events, etc.)
    // Examples: "Sequence Start", "Pump On", "Ch1 Off"
    
    else if (popupShowTimer)
    {
        popupShowTimer--;  // Count down popup timer
        oled.clearDisplay();  // Clear screen
        oled.setTextSize(1);  // Use small text
        oled.setCursor(0, 0);
        oled.print(popupStatus);  // Display popup message
        // Serial.println(popupStatus);  // Optional: log popup to serial
    }
    
    // ==========================================================================================
    // REGULAR STATUS DISPLAY (DEFAULT SCREEN)
    // ==========================================================================================
    // This is the normal display shown when no notifications are active
    // Shows sensor values, operating mode, and system status
    
    else
    {
        oled.clearDisplay();  // Clear screen for fresh display

        // ----------------------------------------------------------------------------------
        // MAIN DISPLAY AREA - SENSOR VALUES OR MODE
        // ----------------------------------------------------------------------------------
        // Display content depends on sensor model and configuration
        
        oled.setTextSize(2);  // Use large text for main value
        oled.setCursor(0, 15);  // Position in middle of screen

#if !defined(NOSENSOR_MODEL)
        // If sensors are present, display humidity as main value
        // This is the most important reading for irrigation control
        oled.print(humidity, 0);  // Display humidity (no decimal places)
        oled.print(" %");  // Add percent sign
#else
        // If no sensors, display operating mode instead
        oled.setTextSize(1);  // Use smaller text for mode display
        if (globalMode == OFF)
            oled.print("Off mode");  // Manual/off mode
        else if (globalMode == AUTO)
            oled.print("Auto mode");  // Automatic mode
#endif

        // ----------------------------------------------------------------------------------
        // TITLE BAR
        // ----------------------------------------------------------------------------------
        // Display "SmartFarm" title at top of screen
        // This is always visible except during notifications
        
        oled.setTextSize(1);  // Use small text for title
        oled.setCursor(0, 0);  // Top-left position
        oled.print("SmartFarm");  // Application title

        // ----------------------------------------------------------------------------------
        // STATUS BAR - TIMERS AND SYSTEM STATUS
        // ----------------------------------------------------------------------------------
        // Display irrigation status, timers, or other system information
        // Examples: "OFF", "C1:500", "P120", "D5" (delay countdown)
        
        oled.setCursor(0, 40);  // Bottom of screen
        oled.print(displayStatus);  // Status string from updateSystemState()
    }

    
    // ==========================================================================================
    // WIFI STATUS ICON DISPLAY
    // ==========================================================================================
    // Display appropriate WiFi icon in top-right corner (position 55, 0 or 56, 0)
    // These icons provide quick visual feedback about network connection status
    
    if (curr_state == iotwebconf::NotConfigured || curr_state == iotwebconf::ApMode)
        // AP Mode or Not Configured: Show router/AP icon (9x8 pixels)
        oled.drawBitmap(55, 0, wifi_ap, 9, 8, 1);
        
    else if (curr_state == iotwebconf::Connecting)
    {
        // Connecting state: Blinking WiFi icon animation
        // Toggles on/off each call to create blinking effect
        if (t_connecting == 1)
        {
            oled.drawBitmap(56, 0, wifi_on, 8, 8, 1);  // Show WiFi icon
            t_connecting = 0;
        }
        else
        {
            t_connecting = 1;  // Next time: don't show icon (off frame)
        }
    }
    else if (curr_state == iotwebconf::OnLine)
    {
        // Online state: Check cloud connection status
        if (iot.status())
        {
            // Connected to both WiFi AND CynoIOT cloud
            oled.drawBitmap(56, 0, wifi_on, 8, 8, 1);  // Show WiFi on icon
        }
        else
        {
            // Connected to WiFi but NOT to CynoIOT cloud
            // Shows user that device is online but cloud is unreachable
            oled.drawBitmap(56, 0, wifi_nointernet, 8, 8, 1);  // Show WiFi with X icon
        }
    }
    else if (curr_state == iotwebconf::OffLine)
        // Offline state: WiFi connection lost
        oled.drawBitmap(56, 0, wifi_off, 8, 8, 1);  // Show WiFi off icon

    // ==========================================================================================
    // RENDER DISPLAY TO OLED SCREEN
    // ==========================================================================================
    // All drawing operations are buffered in memory
    // This command sends the buffer to the actual OLED screen
    // Without this, nothing would appear on the display
    
    oled.display();  // Push all graphics to OLED screen
}

// ==========================================================================================
// EMERGENCY SEQUENCE STOP FUNCTION
// ==========================================================================================
/**
 * @brief Immediately stop all irrigation operations and reset to idle state
 * 
 * This function performs an emergency shutdown of the irrigation system.
 * It's called when:
 * - User manually stops irrigation via event "SQ" with value "0"
 * - Pump protection timer triggers (pump running too long without valves)
 * - System needs to abort current irrigation cycle
 * 
 * Actions performed:
 * 1. Set working mode to NO_WORKING (idle state)
 * 2. Turn off pump immediately
 * 3. Reset pump timer and delay timer to zero
 * 4. Turn off all 4 channels/valves
 * 5. Reset all channel timers to zero
 * 6. Display "Sequence Stop" popup message
 * 
 * @note This is an immediate stop - no graceful shutdown or waiting
 * @note updateHardwareOutputs() will apply these state changes to GPIO pins
 */
void offSeq()
{
    workingMode = NO_WORKING;  // Return to idle mode
    pumpState = 0;             // Turn off pump
    pumpTimer = 0;             // Reset pump timer
    pumpDelayTimer = 0;        // Reset pump delay timer
    for (uint8_t i = 0; i < 4; i++)
    {
        chState[i] = 0;        // Turn off all channels
        chTimer[i] = 0;        // Reset all channel timers
    }
    showPopupMessage("Sequence\n\nStop");  // Notify user
}

// ==========================================================================================
// START SEQUENTIAL IRRIGATION MODE
// ==========================================================================================
/**
 * @brief Initialize and start the sequential irrigation cycle
 * 
 * This function is called when irrigation is triggered via event "SQ" with value "1".
 * It sets up all timers and states for a complete irrigation cycle through all channels.
 * 
 * Sequence Operation:
 * 1. Reset humidity cutoff flag (allows cutoff to trigger during this cycle)
 * 2. Set working mode to SEQUENCE
 * 3. Set channel timers based on number of active channels
 * 4. Calculate and set pump timer
 * 5. Set pump delay if using channels
 * 6. Display "Sequence Start" popup message
 * 
 * Timer Calculations:
 * - Each channel gets 'interval' seconds of irrigation time
 * - Pump timer = (number of channels × interval) - pump delay - 3 seconds
 * - The -3 seconds accounts for overlap adjustments
 * - Pump delay ensures pump is pressurized before first valve opens
 * 
 * Example with 4 channels, 600 second interval:
 * - Each channel: 600 seconds (10 minutes)
 * - Pump delay: 5 seconds (pumpDelayConst)
 * - Pump timer: (4 × 600) - 5 - 3 = 2392 seconds
 * - Total cycle time: ~40 minutes
 * 
 * @note This function only sets up the sequence - updateSystemState() executes it
 * @see updateSystemState() for actual sequence execution logic
 */
void startSequenceMode()
{
    // Reset humidity cutoff flag to allow cutoff during this cycle
    humidityCutoffApplied = false;
    
    // Set mode to SEQUENCE - activates irrigation logic in updateSystemState()
    workingMode = SEQUENCE;
    
    // ----------------------------------------------------------------------------------
    // SETUP CHANNEL TIMERS
    // ----------------------------------------------------------------------------------
    // If channels are enabled, set timer for each active channel
    if (chUse)
    {
        for (uint8_t i = 0; i < chUse; i++)
        {
            chTimer[i] = interval;  // Each channel runs for 'interval' seconds
        }
        // iot.debug("TIMERS SET: chTimer[0]=" + String(chTimer[0]) + ", interval=" + String(interval));
    }

    // ----------------------------------------------------------------------------------
    // SETUP PUMP TIMER
    // ----------------------------------------------------------------------------------
    // Calculate pump operation time based on configuration
    if (pumpUse)
    {
        // Calculate pump time: total channel time minus delays and overlaps
        pumpTimer = chUse * interval - pumpDelayConst - 3;

        // If using channels, add pump startup delay
        // This ensures pump builds pressure before first valve opens
        if (chUse)
        {
            pumpDelayTimer = pumpDelayConst;  // Delay before pump starts (typically 5 seconds)
            
            // Ensure pump timer doesn't go negative (safety check)
            if (int32_t(chUse * interval - pumpDelayConst - 3) < 0)
                pumpTimer = 0;
            else
                pumpTimer = chUse * interval - pumpDelayConst - 3;
        }
        else
        {
            // Pump-only mode (no channels): pump runs for the full interval
            pumpTimer = interval;
        }
    }
    
    // Notify user that sequence has started
    showPopupMessage("Sequence\n\nStart");
}

// ==========================================================================================
// POPUP MESSAGE DISPLAY FUNCTION
// ==========================================================================================
/**
 * @brief Display a temporary popup message on the OLED screen
 * 
 * This function queues a message to be displayed on the OLED screen for a specified
 * duration. It overrides the normal display content temporarily.
 * 
 * How it works:
 * 1. Store the message text in popupStatus variable
 * 2. Set the display duration in popupShowTimer (in seconds)
 * 3. updateDisplay() function checks these variables and displays the popup
 * 4. After timer expires, normal display resumes
 * 
 * Usage Examples:
 * - showPopupMessage("Pump\n\nOn", 3);  // Show for 3 seconds
 * - showPopupMessage("Error", 5);       // Show for 5 seconds
 * - showPopupMessage("Set to\n\nAuto\n\nMode");  // Multi-line message
 * 
 * Display Priority:
 * - Popup messages take priority over normal status display
 * - If displaytime is set, popup waits until that completes
 * - This ensures important notifications are always seen
 * 
 * @param msg The message text to display (supports \n for newlines)
 * @param timeout Duration to display message in seconds (default: 3 if not specified)
 * 
 * @note Maximum recommended line count is 4-5 lines (screen height: 64 pixels)
 * @note Use \n\n for double spacing between lines for better readability
 */
void showPopupMessage(String msg, uint8_t timeout)
{
    popupStatus = msg;       // Store message text
    popupShowTimer = timeout;  // Set display duration in seconds
}

// ==========================================================================================
// HARDWARE OUTPUT UPDATE FUNCTION
// ==========================================================================================
/**
 * @brief Apply software state to physical GPIO pins and notify cloud platform
 * 
 * This function synchronizes the physical hardware state with the software state.
 * It's called in every iteration of the main loop to ensure outputs are correct.
 * 
 * How it works:
 * - Reads current physical state of each GPIO pin
 * - Compares with desired software state (pumpState, chState[])
 * - Only updates GPIO if state has changed (saves write operations)
 * - Notifies CynoIOT cloud platform of any state changes
 * 
 * Why read before write:
 * - Prevents unnecessary digitalWrite() calls
 * - Reduces electrical stress on relays/solenoids
 * - Improves efficiency (digitalRead is faster than digitalWrite on many platforms)
 * - Ensures we only notify cloud when actual change occurs
 * 
 * Cloud Notification:
 * - Every state change is sent to CynoIOT platform via iot.eventUpdate()
 * - This keeps cloud dashboard synchronized with actual hardware state
 * - Event codes: "P" (pump), "c1"-"c4" (channels)
 * 
 * GPIO Protection:
 * - This function is the ONLY place that writes to pump/channel pins
 * - All other code only modifies the software state variables
 * - This prevents conflicting GPIO writes from different parts of code
 * 
 * @note This function runs in every loop() iteration
 * @note Changes take effect immediately (no delay)
 * @note Debug Serial.print statements are commented out for production use
 */
void updateHardwareOutputs()
{
    // ----------------------------------------------------------------------------------
    // PUMP OUTPUT UPDATE
    // ----------------------------------------------------------------------------------
    // Check if physical pump state differs from desired state
    if (digitalRead(PUMP) != pumpState)
    {
        // Serial.println("pump change to " + String(pumpState));
        digitalWrite(PUMP, pumpState);  // Apply new state to GPIO pin
        iot.eventUpdate("P", pumpState);  // Notify cloud platform of state change
    }

    // ----------------------------------------------------------------------------------
    // CHANNEL OUTPUT UPDATES
    // ----------------------------------------------------------------------------------
    // Each channel is checked and updated independently
    // This allows individual channels to change without affecting others
    
    // Channel 1 (Valve 1)
    if (digitalRead(CH1) != chState[0])
    {
        // Serial.println("ch1 change to " + String(chState[0]));
        // iot.debug("ch1 change to " + String(chState[0]));
        digitalWrite(CH1, chState[0]);  // Apply new state to GPIO pin
        iot.eventUpdate("c1", chState[0]);  // Notify cloud platform
    }
    
    // Channel 2 (Valve 2)
    if (digitalRead(CH2) != chState[1])
    {
        // Serial.println("ch2 change to " + String(chState[1]));
        digitalWrite(CH2, chState[1]);  // Apply new state to GPIO pin
        iot.eventUpdate("c2", chState[1]);  // Notify cloud platform
    }
    
    // Channel 3 (Valve 3)
    if (digitalRead(CH3) != chState[2])
    {
        // Serial.println("ch3 change to " + String(chState[2]));
        digitalWrite(CH3, chState[2]);  // Apply new state to GPIO pin
        iot.eventUpdate("c3", chState[2]);  // Notify cloud platform
    }
    
    // Channel 4 (Valve 4)
    if (digitalRead(CH4) != chState[3])
    {
        // Serial.println("ch4 change to " + String(chState[3]));
        digitalWrite(CH4, chState[3]);  // Apply new state to GPIO pin
        iot.eventUpdate("c4", chState[3]);  // Notify cloud platform
    }
}

// ==========================================================================================
// SYSTEM STATE CALCULATION FUNCTION
// ==========================================================================================
/**
 * @brief Calculate remaining irrigation time for cloud display
 * 
 * This function calculates how much time remains in the current irrigation cycle.
 * The value is sent to the CynoIOT cloud platform to show on the dashboard.
 * 
 * Calculation Logic:
 * 1. If channels are in use (chUse > 0) and any channel timer is active:
 *    - Return sum of all remaining channel times
 *    - This represents total time until all channels complete
 * 
 * 2. Else if pump-only mode (no channels) and pump timer is active:
 *    - Return pump timer value
 *    - Shows how long pump will continue running
 * 
 * 3. Else (idle or pump protection mode):
 *    - Return pump protection timer value
 *    - Shows how long pump has been running without valves
 *    - Used for safety monitoring
 * 
 * Use Cases:
 * - Cloud dashboard displays remaining irrigation time
 * - Users can see how long until cycle completes
 * - Helps monitor pump protection timer
 * 
 * Example Values:
 * - Channel mode with 4 active channels: returns sum of chTimer[0] through chTimer[3]
 *   Example: chTimer = [500, 400, 300, 200] → returns 1400 seconds
 * - Pump-only mode: returns pumpTimer value
 *   Example: pumpTimer = 600 → returns 600 seconds
 * - Idle mode: returns pumpOnProtectionTimer
 *   Example: protectionTimer = 120 → returns 120 seconds
 * 
 * @return Total remaining time in seconds (uint32_t)
 * @note This value is sent as the first element in the data array to cloud
 * @note A value of 0 indicates system is idle with no active operations
 */
uint32_t getSystemState()
{
    uint32_t onState = 0;
    
    // Priority 1: If channels are enabled and any channel timer is active
    // Calculate total remaining time across all active channels
    if (chUse && (chTimer[0] > 0 || chTimer[1] > 0 || chTimer[2] > 0 || chTimer[3] > 0))
    {
        onState = chTimer[0] + chTimer[1] + chTimer[2] + chTimer[3];
    }
    
    // Priority 2: Pump-only mode (no channels) and pump is running
    else if (!chUse && pumpUse && pumpTimer > 0)
    {
        onState = pumpTimer;  // Return pump timer value
    }
    
    // Priority 3: Idle or protection mode
    else
    {
        onState = pumpOnProtectionTimer;  // Return protection timer (for monitoring)
    }

    return onState;  // Return calculated time in seconds
}

// ==========================================================================================
// RS485 PRE-TRANSMISSION CALLBACK
// ==========================================================================================
/**
 * @brief Configure RS485 transceiver for transmission mode before sending Modbus data
 * 
 * This function is called by the ModbusMaster library immediately before
 * transmitting data to the soil sensor over the RS485 bus.
 * 
 * RS485 Half-Duplex Communication:
 * - RS485 uses half-duplex communication (can't transmit and receive simultaneously)
 * - The MAX485 transceiver module must be switched between transmit and receive modes
 * - DE (Driver Enable) and RE (Receiver Enable) pins control the mode
 * 
 * MAX485 Module Pin Functions:
 * - DE (Driver Enable): When HIGH, enables transmitter to send data
 * - RE (Receiver Enable): When LOW, enables receiver to accept data
 * - These pins are typically connected together and controlled as a pair
 * 
 * Transmission Mode Configuration:
 * - Set both DE and RE pins as OUTPUT (Arduino controls them)
 * - Set DE HIGH: Enables the transmitter driver
 * - Set RE HIGH: Disables the receiver (note: RE is active LOW, so HIGH = disabled)
 * - Result: MAX485 is in TRANSMIT mode
 * 
 * Why the 1ms delay:
 * - Gives MAX485 time to switch to transmit mode
 * - Ensures stable transmission before data starts flowing
 * - Prevents data corruption from mode switching transients
 * 
 * @note This is a callback function registered with ModbusMaster via node.preTransmission()
 * @see postTransmission() for switching back to receive mode
 * @see setup() where this callback is registered
 */
void preTransmission()
{
    // Configure RE and DE pins as outputs (Arduino controls the transceiver mode)
    pinMode(MAX485_RE, OUTPUT);
    pinMode(MAX485_DE, OUTPUT);

    // Set both pins HIGH to enable transmission mode
    digitalWrite(MAX485_RE, 1);  // HIGH disables receiver (RE is active LOW)
    digitalWrite(MAX485_DE, 1);  // HIGH enables transmitter driver
    
    delay(1);  // Wait 1ms for MAX485 to stabilize in transmit mode before sending data
}

// ==========================================================================================
// RS485 POST-TRANSMISSION CALLBACK
// ==========================================================================================
/**
 * @brief Configure RS485 transceiver for reception mode after sending Modbus data
 * 
 * This function is called by the ModbusMaster library immediately after
 * transmitting data to the soil sensor, to prepare for receiving the response.
 * 
 * Reception Mode Configuration:
 * - Set DE LOW: Disables the transmitter driver
 * - Set RE LOW: Enables the receiver (RE is active LOW)
 * - Result: MAX485 is in RECEIVE mode
 * 
 * Why the 3ms delay BEFORE setting pins:
 * - Allows last transmitted bits to complete and propagate through the bus
 * - Prevents cutting off the end of transmission
 * - Gives time for the sensor to process the command and prepare its response
 * - Typical RS485 turnaround time is 1-4ms depending on bus length and baud rate
 * 
 * Timing Sequence:
 * 1. ModbusMaster finishes transmitting data
 * 2. postTransmission() is called immediately
 * 3. Wait 3ms (bus turnaround time)
 * 4. Set RE and DE to LOW (switch to receive mode)
 * 5. ModbusMaster waits for sensor response
 * 6. Sensor sends response data
 * 7. ModbusMaster reads and decodes response
 * 
 * @note This is a callback function registered with ModbusMaster via node.postTransmission()
 * @note The 3ms delay may need adjustment for very long RS485 bus runs
 * @see preTransmission() for switching to transmit mode
 * @see disConnect() for completely disabling the transceiver
 */
void postTransmission()
{
    delay(3);  // Wait for transmission to complete and bus to settle

    // Set both pins LOW to enable reception mode
    digitalWrite(MAX485_RE, 0);  // LOW enables receiver (RE is active LOW)
    digitalWrite(MAX485_DE, 0);  // LOW disables transmitter driver
}

// ==========================================================================================
// RS485 TRANSCEIVER DISABLE FUNCTION
// ==========================================================================================
/**
 * @brief Completely disable RS485 transceiver to save power and prevent interference
 * 
 * This function is called after Modbus communication is complete to put the
 * MAX485 transceiver into a low-power, high-impedance state.
 * 
 * Why disable the transceiver:
 * 1. Power Saving:
 *    - When pins are set to INPUT, the MAX485 control pins are in high-impedance state
 *    - Reduces power consumption when not communicating
 *    - Important for battery-powered applications
 * 
 * 2. Bus Release:
 *    - Sets DE and RE pins to high-impedance (input) mode
 *    - Effectively "releases" the RS485 bus
 *    - Prevents this device from interfering with other devices on the bus
 *    - Allows other devices to control the bus in multi-drop configurations
 * 
 * 3. Pin State Protection:
 *    - INPUT mode prevents accidental writes from affecting the transceiver
 *    - Protects against floating pins picking up noise and triggering unwanted modes
 * 
 * When to use:
 * - After receiving sensor data and processing is complete
 * - Before long idle periods between sensor reads
 * - When entering low-power sleep modes
 * 
 * When NOT to use:
 * - Don't call this before preTransmission() (will interfere with mode switching)
 * - Not needed during rapid communication cycles
 * 
 * @note This is called in readAndSendSensorData() after each Modbus read
 * @note preTransmission() will reconfigure these as OUTPUT before next communication
 * @see readAndSendSensorData() where this function is called
 */
void disConnect()
{
    // Set control pins to INPUT (high-impedance state)
    // This puts MAX485 in disabled/low-power mode
    pinMode(MAX485_RE, INPUT);
    pinMode(MAX485_DE, INPUT);
}

// ==========================================================================================
// WEB SERVER ROOT PAGE HANDLER
// ==========================================================================================
/**
 * @brief Handle requests to the main web page (/)
 * 
 * This function serves the main web interface for device configuration and monitoring.
 * It's called when a user accesses the device's IP address or hostname in a browser.
 * 
 * Functions performed:
 * 1. Check for captive portal requests (device configuration mode)
 * 2. Load HTML template from flash memory (PROGMEM)
 * 3. Replace template placeholders with actual device information
 * 4. Send completed HTML page to client browser
 * 
 * Captive Portal:
 * - If device is in AP mode (not configured), users are redirected to config page
 * - IotWebConf handles this automatically via handleCaptivePortal()
 * - Returns early if captive portal request was detected
 * 
 * Template Placeholders:
 * The HTML template contains placeholders that are replaced with dynamic values:
 * - %STATE%: Current network state (Boot, ApMode, Connecting, OnLine, OffLine)
 * - %THING_NAME%: Device name (default: "SoilSensor")
 * - %EMAIL%: User's CynoIOT account email
 * - %SSID%: WiFi network name the device is connected to
 * - %RSSI%: WiFi signal strength in dBm (typically -30 to -90)
 * - %ESP_ID%: Unique device identifier (Client ID)
 * - %VERSION%: Firmware version string
 * 
 * Use Cases:
 * - User opens http://device-ip.local in browser
 * - User accesses device via mDNS hostname
 * - Captive portal redirects unconfigured clients here
 * 
 * @note HTML template is stored in PROGMEM (flash memory) to save RAM
 * @note FPSTR() macro is used to read string from flash memory
 * @see htmlTemplate[] for the actual HTML content (defined earlier in file)
 */
void handleRoot()
{
    // -- Check for captive portal requests first
    // IotWebConf handles captive portal detection and redirection
    // If this is a captive portal request, it's handled internally and we return
    if (iotWebConf.handleCaptivePortal())
    {
        // -- Captive portal request was handled, no need to continue
        return;
    }

    // -- Load HTML template and replace placeholders with actual values
    String s = FPSTR(htmlTemplate);  // Read template from flash memory (PROGMEM)
    
    // Replace template variables with current device information
    s.replace("%STATE%", String(iotWebConf.getState()));          // Network connection state
    s.replace("%THING_NAME%", String(iotWebConf.getThingName())); // Device name
    s.replace("%EMAIL%", String(emailParamValue));                // User's email for CynoIOT
    s.replace("%SSID%", String(iotWebConf.getSSID()));            // Connected WiFi network name
    s.replace("%RSSI%", String(WiFi.RSSI()));                     // WiFi signal strength (dBm)
    s.replace("%ESP_ID%", String(iot.getClientId()));             // Unique device ID
    s.replace("%VERSION%", String(IOTVERSION));                   // Firmware version

    // Send the completed HTML page to the client browser
    // HTTP 200 OK status, content type: text/html
    server.send(200, "text/html", s);
}

// ==========================================================================================
// CONFIGURATION SAVED CALLBACK
// ==========================================================================================
/**
 * @brief Called when user saves configuration via web interface
 * 
 * This callback function is invoked by IotWebConf when the user submits the
 * configuration form and settings are successfully saved to EEPROM.
 * 
 * When this is called:
 * - User has filled in WiFi credentials (SSID, password)
 * - User may have entered email for CynoIOT login
 * - Settings have been validated and written to EEPROM
 * - Device will attempt to connect to WiFi with new credentials
 * 
 * Current behavior:
 * - Logs message to Serial Monitor for debugging
 * - Can be extended to perform additional actions:
 *   * Send notification to cloud
 *   * Update display to show "Config saved"
 *   * Trigger device reboot
 *   * Update internal configuration variables
 * 
 * Example extensions:
 * ```cpp
 * void configSaved()
 * {
 *     Serial.println("Configuration was updated.");
 *     showPopupMessage("Config\n\nSaved!\n\nRebooting...");
 *     delay(2000);
 *     ESP.restart();
 * }
 * ```
 * 
 * @note This is registered with IotWebConf via iotWebConf.setConfigSavedCallback()
 * @see setup() where this callback is registered
 * @see wifiConnected() which is called after successful WiFi connection
 */
void configSaved()
{
    Serial.println("Configuration was updated.");
}

// ==========================================================================================
// WIFI CONNECTION SUCCESS CALLBACK
// ==========================================================================================
/**
 * @brief Called when WiFi connection is successfully established
 * 
 * This callback function is invoked by IotWebConf when the device successfully
 * connects to a WiFi network using the credentials provided by the user.
 * 
 * When this is called:
 * - WiFi connection has been established
 * - Device has obtained an IP address via DHCP
 * - Network is ready for communication
 * - mDNS service should be started
 * - CynoIOT cloud connection should be initiated
 * 
 * Actions performed:
 * 1. Log connection success to Serial Monitor
 * 2. Start mDNS responder for local hostname resolution
 * 3. Advertise HTTP service on port 80 via mDNS
 * 4. Print mDNS hostname for user convenience
 * 5. If email was provided, connect to CynoIOT cloud platform
 * 
 * mDNS (Multicast DNS):
 * - Allows accessing device via hostname instead of IP address
 * - Example: http://soilsensor.local instead of http://192.168.1.100
 * - Works on most modern OS (Windows 10+, macOS, Linux, Android)
 * - Service type "http" on port 80 makes device discoverable by browsers
 * 
 * CynoIOT Cloud Connection:
 * - If user provided email address during configuration
 * - iot.connect() authenticates with CynoIOT platform
 * - Enables remote monitoring and control via cloud dashboard
 * - Device ID and credentials are managed by Cynoiot library
 * 
 * @note This is registered with IotWebConf via iotWebConf.setWifiConnectionCallback()
 * @see setup() where this callback is registered
 * @see iotSetup() for CynoIOT initialization
 */
void wifiConnected()
{
    // Log successful WiFi connection
    Serial.println("WiFi was connected.");
    
    // Start mDNS responder with device hostname
    // This allows accessing device via http://devicename.local
    MDNS.begin(iotWebConf.getThingName());
    
    // Advertise HTTP service on port 80
    // Makes device discoverable by mDNS browsers
    MDNS.addService("http", "tcp", 80);

    // Display mDNS hostname for user to access device
    Serial.printf("Ready! Open http://%s.local in your browser\n", String(iotWebConf.getThingName()));
    
    // If user provided email address, connect to CynoIOT cloud platform
    // Email serves as user identifier for cloud account
    if ((String)emailParamValue != "")
    {
        // Begin cloud connection with provided email credentials
        Serial.println("login with " + (String)emailParamValue);
        iot.connect((String)emailParamValue);  // Authenticate and connect to CynoIOT
    }
}

// ==========================================================================================
// FORM VALIDATION CALLBACK
// ==========================================================================================
/**
 * @brief Validate user input from web configuration form
 * 
 * This callback function is called by IotWebConf when the user submits the
 * configuration form, before saving settings to EEPROM.
 * 
 * Purpose:
 * - Validate user input for correctness and completeness
 * - Provide feedback if validation fails
 * - Prevent invalid configuration from being saved
 * 
 * Current implementation:
 * - Always returns true (validation passes)
 * - Logs validation attempt to Serial Monitor
 * - No actual validation is performed (all inputs accepted)
 * 
 * Example validation rules that could be added:
 * - Check email format (contains @ symbol, valid domain)
 * - Ensure password meets minimum length requirements
 * - Verify SSID is not empty
 * - Check that numeric parameters are within valid ranges
 * 
 * Example implementation with email validation:
 * ```cpp
 * bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper)
 * {
 *     Serial.println("Validating form.");
 *     bool valid = true;
 *     
 *     // Get email value from form
 *     String email = webRequestWrapper->arg(emailParam.getId());
 *     
 *     // Validate email format (basic check)
 *     if (email.length() > 0 && !email.contains("@"))
 *     {
 *         emailParam.errorMessage = "Please enter a valid email address";
 *         valid = false;
 *     }
 *     
 *     return valid;
 * }
 * ```
 * 
 * @param webRequestWrapper Wrapper object containing form data and methods
 * @return true if validation passes (save settings), false if validation fails (show errors)
 * 
 * @note This is registered with IotWebConf via iotWebConf.setFormValidator()
 * @see setup() where this callback is registered
 * @note errorMessage field of parameters is displayed to user if validation fails
 */
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper)
{
    Serial.println("Validating form.");
    bool valid = true;

    /*
    // Example validation code (commented out):
    // Get the length of a parameter value
    int l = webRequestWrapper->arg(stringParam.getId()).length();
    
    // Check if parameter meets minimum length requirement
    if (l < 3)
    {
        // Set error message to display to user
        stringParam.errorMessage = "Please provide at least 3 characters for this test!";
        valid = false;  // Validation failed
    }
    */
    
    return valid;  // Currently always returns true (no validation)
}

// ==========================================================================================
// EEPROM CLEAR FUNCTION
// ==========================================================================================
/**
 * @brief Clear all EEPROM data and reboot device (factory reset)
 * 
 * This function performs a complete factory reset by erasing all stored
 * configuration data from EEPROM and restarting the device.
 * 
 * When to use:
 * - Device configuration is corrupted or invalid
 * - User wants to start fresh with default settings
 * - Troubleshooting WiFi or configuration issues
 * - Transferring device to new owner/location
 * 
 * Data cleared:
 * - WiFi credentials (SSID, password)
 * - CynoIOT email/login
 * - Irrigation interval settings
 * - Channel/pump usage configuration
 * - Humidity cutoff thresholds
 * - Global mode settings
 * - All other custom configuration stored in EEPROM
 * 
 * Process:
 * 1. Begin EEPROM session (512 bytes)
 * 2. Write 0 to every EEPROM address (0x00 to 0x1FF)
 * 3. Commit changes and close EEPROM session
 * 4. Send response to web client
 * 5. Wait 1 second for HTTP response to complete
 * 6. Restart ESP8266/ESP32
 * 
 * After reboot:
 * - Device starts in AP mode (configuration portal)
 * - Default settings are loaded (interval=600s, channels=1, etc.)
 * - User must configure WiFi and email again
 * - All previous customizations are lost
 * 
 * Access method:
 * - Called via HTTP endpoint: http://device-ip/cleareeprom
 * - Can also be triggered by holding RSTPIN low during boot (hardware reset)
 * - Protected by web server authentication (if enabled)
 * 
 * @note This is a destructive operation - all settings are permanently lost
 * @note Make sure to backup important settings before calling this function
 * @see setup() for hardware reset functionality using RSTPIN
 * @see reboot() for simple reboot without clearing EEPROM
 */
void clearEEPROM()
{
    Serial.println("clearEEPROM() called!");

    // Begin EEPROM session with 512 bytes
    EEPROM.begin(512);

    // Write 0 to all 512 bytes of EEPROM (complete erase)
    // This clears all stored configuration data
    for (int i = 0; i < 512; i++)
    {
        EEPROM.write(i, 0);  // Write 0x00 to each address
    }

    EEPROM.end();  // Commit changes and close EEPROM session
    
    // Send response to HTTP client
    // Client sees "Clear all data" message before device reboots
    server.send(200, "text/plain", "Clear all data\nrebooting");
    
    delay(1000);  // Wait for HTTP response to complete
    ESP.restart();  // Restart device to apply factory reset
}

// ==========================================================================================
// DEVICE REBOOT FUNCTION
// ==========================================================================================
/**
 * @brief Restart the ESP8266/ESP32 device
 * 
 * This function performs a simple device reboot without clearing any configuration.
 * All settings are preserved across the reboot.
 * 
 * When to use:
 * - Apply firmware updates
 * - Resolve software glitches or hangs
 * - Restart network connection
 * - Apply certain configuration changes that require reboot
 * - User-initiated restart via web interface
 * 
 * Difference from clearEEPROM():
 * - reboot(): Preserves all settings, just restarts the device
 * - clearEEPROM(): Erases all settings AND restarts the device
 * 
 * Process:
 * 1. Send response to web client ("rebooting")
 * 2. Wait 1 second for HTTP response to complete
 * 3. Call ESP.restart() to restart the microcontroller
 * 
 * What happens during reboot:
 * - All programs stop execution
 * - Microcontroller resets its internal state
 * - setup() function runs again from the beginning
 * - WiFi connection is re-established
 * - All configuration is loaded from EEPROM (preserved)
 * - Normal operation resumes
 * 
 * Access method:
 * - Called via HTTP endpoint: http://device-ip/reboot
 * - Can be triggered from web interface button
 * - Can be called programmatically if needed
 * 
 * Typical reboot time:
 * - ESP8266: 1-3 seconds
 * - ESP32: 0.5-2 seconds
 * - Plus WiFi connection time: 2-10 seconds depending on network
 * 
 * @note This does NOT clear EEPROM - all settings are preserved
 * @note Serial Monitor connection may be reset during reboot
 * @note Any uncommitted EEPROM changes should be committed before calling this
 * @see clearEEPROM() for factory reset that erases all settings
 */
void reboot()
{
    // Send response to HTTP client
    // Client sees "rebooting" message before device restarts
    server.send(200, "text/plain", "rebooting");
    
    delay(1000);  // Wait for HTTP response to complete
    ESP.restart();  // Restart the ESP8266/ESP32 microcontroller
}

// ==========================================================================================
// STATUS JSON ENDPOINT HANDLER
// ==========================================================================================
/**
 * @brief Return current device status as JSON
 * 
 * This function provides a REST API endpoint for retrieving the current
 * state of all pumps and channels in JSON format.
 * 
 * Endpoint: /status
 * Method: GET
 * Response Format: JSON
 * Content-Type: application/json
 * 
 * JSON Response Structure:
 * ```json
 * {
 *   "pump": 0,     // 0 = OFF, 1 = ON
 *   "ch1": 1,      // 0 = OFF, 1 = ON
 *   "ch2": 0,      // 0 = OFF, 1 = ON
 *   "ch3": 0,      // 0 = OFF, 1 = ON
 *   "ch4": 0       // 0 = OFF, 1 = ON
 * }
 * ```
 * 
 * Use Cases:
 * - Mobile apps or web dashboards polling for device status
 * - Home automation systems (Home Assistant, OpenHAB, etc.)
 * - Custom monitoring scripts
 * - Integration with other IoT platforms
 * - AJAX requests from web interface for real-time updates
 * 
 * Example Usage (curl):
 * ```bash
 * curl http://soilsensor.local/status
 * # Returns: {"pump":0,"ch1":1,"ch2":0,"ch3":0,"ch4":0}
 * ```
 * 
 * Example Usage (JavaScript):
 * ```javascript
 * fetch('/status')
 *   .then(response => response.json())
 *   .then(data => {
 *     console.log('Pump:', data.pump);
 *     console.log('Channel 1:', data.ch1);
 *     // Update UI elements
 *   });
 * ```
 * 
 * Data Source:
 * - Values are read from current state variables (pumpState, chState[])
 * - These reflect the DESIRED state (what should be ON/OFF)
 * - For actual physical state, read GPIO pins directly
 * 
 * @note This endpoint is read-only - it doesn't change any device state
 * @note For controlling devices, use /gpio/pump, /gpio/ch1-4 endpoints
 * @note Response is minimal JSON for efficient transmission
 * @see server.on("/status", handleStatus) in setup() for endpoint registration
 */
void handleStatus()
{
    // Build JSON string manually (Arduino JSON library not used to save memory)
    // Manual string building is more memory-efficient than JSON library
    
    String json = "{";  // Start JSON object
    
    // Add pump state
    json += "\"pump\":" + String(pumpState) + ",";
    
    // Add channel 1-4 states
    json += "\"ch1\":" + String(chState[0]) + ",";
    json += "\"ch2\":" + String(chState[1]) + ",";
    json += "\"ch3\":" + String(chState[2]) + ",";
    json += "\"ch4\":" + String(chState[3]);
    
    json += "}";  // End JSON object
    
    // Send JSON response to client
    // HTTP 200 OK, Content-Type: application/json
    server.send(200, "application/json", json);
}
