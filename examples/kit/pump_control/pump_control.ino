/* Pump Control
 * Controls a water pump via relay with timer-based operation
 * Web config, OLED display, CynoIOT cloud integration
 * ESP8266/ESP32
 */
/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

// ==========================================================================================
// LIBRARY INCLUDES
// ==========================================================================================

// WiFi and network libraries for ESP8266
#ifdef ESP8266
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

// WiFi and network libraries for ESP32
#elif defined(ESP32)
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <WiFi.h>
#endif

#include <EEPROM.h>  // EEPROM library for storing configuration data
#include <Wire.h>    // Wire library for I2C communication with OLED display

// CynoIOT and Web Configuration libraries
// IoTWebConf from https://github.com/canusorn/IotWebConf-iotbundle
#include <IotWebConf.h>
#include <IotWebConfUsing.h>

// Display libraries
#include <Adafruit_GFX.h>      // Adafruit GFX library for graphics primitives
#include <Adafruit_SSD1306.h>  // Adafruit SSD1306 library for OLED display control

// IoT library
#include <cynoiot.h>  // CynoIOT library for cloud platform integration

// ==========================================================================================
// PIN DEFINITIONS - ESP8266
// ==========================================================================================
#ifdef ESP8266
#define PUMP D1    // Water pump control pin
#define RSTPIN D8  // Reset pin for EEPROM clear function

// ==========================================================================================
// PIN DEFINITIONS - ESP32
// ==========================================================================================
#elif defined(ESP32)
#define RSTPIN 8

// ESP32-S2 specific pin configuration
#ifdef CONFIG_IDF_TARGET_ESP32S2
#define PUMP 3  // Water pump control pin

// Standard ESP32 pin configuration
#else
#define PUMP 3  // Water pump control pin
#endif

#endif

// ==========================================================================================
// SYSTEM CONFIGURATION
// ==========================================================================================

// WiFi and Device Configuration
const char thingName[] = "PumpControl";            // Device name for WiFi AP mode
const char wifiInitialApPassword[] = "iotbundle";  // Default password for AP mode

#define STRING_LEN 128  // Maximum length for string parameters (e.g., email, SSID)
#define NUMBER_LEN 32   // Maximum length for numeric parameters

// Communication Objects
Cynoiot iot;  // CynoIOT object for cloud platform integration

// Display Configuration
#define OLED_RESET -1               // Reset pin for OLED display (-1 = use Arduino reset)
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
void handleRoot();                                       // Main web page handler
void showPopupMessage(String msg, uint8_t timeout = 3);  // Display popup message on OLED
void handleStatus();                                     // HTTP handler for status JSON

// Callback functions for IotWebConf
void wifiConnected();                                                  // Called when WiFi connection established
void configSaved();                                                    // Called when configuration is saved
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper);  // Form validation handler

// ==========================================================================================
// TIMING VARIABLES
// ==========================================================================================
unsigned long previousMillis = 0;  // Timer for 1-second interval tasks

// ==========================================================================================
// NETWORK SERVER OBJECTS
// ==========================================================================================
DNSServer dnsServer;   // DNS server for captive portal
WebServer server(80);  // Web server on port 80 for configuration interface

// ==========================================================================================
// OTA (OVER-THE-AIR) UPDATE SERVER
// ==========================================================================================
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
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword);

// Parameter group for login credentials
IotWebConfParameterGroup login = IotWebConfParameterGroup("login", "ล็อกอิน(สมัครที่เว็บก่อนนะครับ)");

// Email parameter for CynoIOT account login
IotWebConfTextParameter emailParam = IotWebConfTextParameter("อีเมลล์", "emailParam", emailParamValue, STRING_LEN);

// ==========================================================================================
// DISPLAY BITMAPS
// ==========================================================================================
// Logo bitmap for startup screen (33x30 pixels)
const uint8_t logo_bmp[] = {  // 'cyno', 33x30px
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
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// WiFi status icons (8x8 pixels) for status display
const uint8_t wifi_on[] = { 0x00, 0x3c, 0x42, 0x99, 0x24, 0x00, 0x18, 0x18 };
const uint8_t wifi_off[] = { 0x01, 0x3e, 0x46, 0x99, 0x34, 0x20, 0x58, 0x98 };
const uint8_t wifi_ap[] = { 0x41, 0x00, 0x80, 0x80, 0xa2, 0x80, 0xaa, 0x80, 0xaa, 0x80, 0x88, 0x80, 0x49, 0x00, 0x08, 0x00 };
const uint8_t wifi_nointernet[] = { 0x03, 0x7b, 0x87, 0x33, 0x4b, 0x00, 0x33, 0x33 };

// ==========================================================================================
// GLOBAL VARIABLES - DISPLAY AND NETWORK STATUS
// ==========================================================================================
uint8_t t_connecting = 0;                                // Counter for connecting animation
uint8_t displaytime = 0;                                 // Counter for display time management
iotwebconf::NetworkState prev_state = iotwebconf::Boot;  // Previous network state
uint8_t sampleUpdate = 0;                                // Counter for data update interval
String noti = "";                                        // Notification message string
uint16_t timer_nointernet = 0;                           // Counter for no-internet detection
uint8_t numVariables = 0;                                // Number of variables to send to cloud

// ==========================================================================================
// GLOBAL VARIABLES - PUMP CONTROL
// ==========================================================================================
uint16_t interval = 600;             // Pump run interval in seconds (default: 10 minutes)
uint32_t pumpTimer = 0;              // Pump operation timer countdown (seconds)
bool pumpState = false;              // Pump ON/OFF state
String displayStatus = "";           // Status string for display
String popupStatus = "";             // Popup message for display
uint8_t popupShowTimer = 0;          // Timer for popup message display
uint32_t pumpOnProtectionTimer = 0;  // Safety timer for pump runaway protection

// ==========================================================================================
// EVENT HANDLER
// ==========================================================================================
void handleEvent(String event, String value) {

  // EVENT: St - Timed Pump Run Control
  if (event == "St") {
    Serial.println("Start: " + value);

    // Start timed pump run
    if (value == "1") {
      pumpTimer = interval;  // Set pump timer to interval duration
      pumpState = 1;         // Turn on pump
      showPopupMessage("Pump\n\nStart\n\n" + String(interval) + "s");
    }
    // Stop timed pump run immediately
    else if (value == "0") {
      pumpTimer = 0;
      pumpState = 0;
      showPopupMessage("Pump\n\nStop");
    } else if ((uint16_t)value.toInt() > 1) {
      pumpTimer = (uint16_t)value.toInt();   // Set pump timer to interval duration
      pumpState = 1;  // Turn on pump
      showPopupMessage("Pump\n\nStart\n\n" + String(interval) + "s");
    }
  }

  // EVENT: In - Pump Run Interval Setting
  else if (event == "In") {
    EEPROM.begin(512);
    Serial.println("Interval: " + value);
    showPopupMessage(String("Interval\n\nSet to\n\n") + value + String(" s"));
    interval = (uint16_t)value.toInt();

    // Persist to EEPROM (only if changed)
    uint16_t currentValue = (uint16_t)EEPROM.read(498) | ((uint16_t)EEPROM.read(499) << 8);
    if (currentValue != interval) {
      EEPROM.write(498, interval & 0xFF);         // low byte
      EEPROM.write(499, (interval >> 8) & 0xFF);  // high byte
      EEPROM.commit();
      Serial.println("Verified written value: " + String((uint16_t)EEPROM.read(498) | ((uint16_t)EEPROM.read(499) << 8)));
    }
    EEPROM.end();
  }

  // EVENT: P - Pump Manual Control
  else if (event == "P") {
    // Only allow manual control when timer is not active
    if (pumpTimer == 0) {
      // Turn pump ON
      if (value == "1") {
        pumpState = 1;
        iot.eventUpdate("P", 1);
        showPopupMessage("Pump\n\nOn");
      }
      // Turn pump OFF
      else if (value == "0") {
        pumpState = 0;
        iot.eventUpdate("P", 0);
        showPopupMessage("Pump\n\nOff");
      }
    }
    // Timer is active - return current pump state
    else {
      iot.eventUpdate("P", !digitalRead(PUMP));
    }
  }
}

// ==========================================================================================
// IOT SETUP FUNCTION
// ==========================================================================================
void iotSetup() {
  // Begin EEPROM session with 512 bytes
  Serial.println("Loading setting from EEPROM");
  EEPROM.begin(512);

  // Load pump run interval from EEPROM (16-bit value: low byte + high byte)
  interval = (uint16_t)EEPROM.read(498) | ((uint16_t)EEPROM.read(499) << 8);
  if (interval == 65535)  // 0xFFFF indicates uninitialized EEPROM
  {
    Serial.println("Load interval = " + String(interval) + ", interval not found in EEPROM, using default value");
    interval = 600;  // Default: 10 minutes (600 seconds)
  }
  Serial.println("interval: " + String(interval));

  EEPROM.end();  // Close EEPROM session

  // Register callback function to handle events from CynoIOT server
  iot.setEventCallback(handleEvent);

  const uint8_t version = 1;  // Project version number for template selection

  // No sensors - only send on/off state
  numVariables = 1;
  String keyname[numVariables] = { "on" };
  iot.setTemplate("pumpcontrol", version);

  // Register variable names with CynoIOT
  iot.setkeyname(keyname, numVariables);

  // Display client ID for debugging
  Serial.println("ClientID:" + String(iot.getClientId()));
}

// ==========================================================================================
// 1-SECOND TIMER FUNCTION
// ==========================================================================================
void time1sec() {
  // Check server connection status
  if (iotWebConf.getState() == iotwebconf::OnLine) {
    if (iot.status()) {
      timer_nointernet = 0;
    } else {
      timer_nointernet++;
      if (timer_nointernet > 30)
        Serial.println("No connection time : " + String(timer_nointernet));
    }
  }

  // Reconnect WiFi if unable to connect to server
  if (timer_nointernet == 60) {
    Serial.println("Can't connect to server -> Restart wifi");
    iotWebConf.goOffLine();
    timer_nointernet++;
  } else if (timer_nointernet >= 65) {
    timer_nointernet = 0;
    iotWebConf.goOnLine(false);
  } else if (timer_nointernet >= 61)
    timer_nointernet++;
}

// ==========================================================================================
// SETUP FUNCTION - One-Time Initialization
// ==========================================================================================
void setup() {
  // Initialize Serial Monitor for debugging (115200 baud)
  Serial.begin(115200);

  // OLED DISPLAY INITIALIZATION
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  oled.drawBitmap(16, 5, logo_bmp, 33, 30, 1);  // Display CynoIOT logo
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 40);
  oled.print("  CYNOIOT");
  oled.display();

  // EEPROM CLEAR FUNCTION
  pinMode(RSTPIN, INPUT_PULLUP);
  if (digitalRead(RSTPIN) == false) {
    delay(1000);                       // Debounce - wait 1 second
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
  pinMode(PUMP, INPUT);      // Start in INPUT mode (pump OFF)
  digitalWrite(PUMP, HIGH);  // active low

  // IOT WEBCONF CONFIGURATION
  login.addItem(&emailParam);

  // Optional: Set status LED pin for ESP32-S2
#ifdef CONFIG_IDF_TARGET_ESP32S2
  iotWebConf.setStatusPin(15);
#endif

  // Configure IotWebConf callbacks and parameters
  iotWebConf.addParameterGroup(&login);                  // Add login parameter group
  iotWebConf.setConfigSavedCallback(&configSaved);       // Called when config is saved
  iotWebConf.setFormValidator(&formValidator);           // Custom form validation
  iotWebConf.getApTimeoutParameter()->visible = false;   // Hide AP timeout setting
  iotWebConf.setWifiConnectionCallback(&wifiConnected);  // Called when WiFi connects

  // OTA UPDATE SERVER SETUP
  iotWebConf.setupUpdateServer(
    [](const char *updatePath) {
      httpUpdater.setup(&server, updatePath);
    },
    [](const char *userName, char *password) {
      httpUpdater.updateCredentials(userName, password);
    });

  // INITIALIZE IOT WEBCONF
  iotWebConf.init();  // Start WiFi configuration portal

  // WEB SERVER URL HANDLERS
  server.on("/", handleRoot);
  server.on("/config", [] {
    iotWebConf.handleConfig();
  });
  server.on("/cleareeprom", clearEEPROM);
  server.on("/reboot", reboot);

  // Manual pump control via HTTP
  server.on("/gpio/pump", []() {
    String state = server.arg("state");
    if (state == "1")
      pumpState = 1;
    else if (state == "0") {
      pumpState = 0;
      pumpTimer = 0;  // Cancel timer when manually turned off
    }
    updateHardwareOutputs();
    server.send(200, "text/plain", "OK");
  });

  // Status endpoint - returns JSON with current pump state
  server.on("/status", handleStatus);

  // 404 handler
  server.onNotFound([]() {
    iotWebConf.handleNotFound();
  });

  Serial.println("Ready.");

  // Wait for system to stabilize
  delay(1000);

  // Initialize CynoIOT connection and load configuration
  iotSetup();
}

// ==========================================================================================
// MAIN LOOP FUNCTION
// ==========================================================================================
void loop() {
  // Background tasks
  iotWebConf.doLoop();
  server.handleClient();
  iot.handle();
#ifdef ESP8266
  MDNS.update();
#endif

  // Timed tasks (every 1 second)
  uint32_t currentMillis = millis();
  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;
    sampleUpdate++;

    time1sec();           // Check network status
    updateSystemState();  // Update pump timer and states
    updateDisplay();      // Refresh OLED display

    // Data upload (every 5 seconds)
    if (sampleUpdate >= 5) {
      readAndSendData();
      sampleUpdate = 0;
    }
  }

  // Hardware output update
  updateHardwareOutputs();
}

// ==========================================================================================
// SYSTEM STATE UPDATE FUNCTION
// ==========================================================================================
void updateSystemState() {
  displayStatus = "";  // Clear display status string

  // TIMED PUMP OPERATION - countdown active timer
  if (pumpTimer > 0) {
    pumpState = 1;                            // Ensure pump is on during timer
    displayStatus = "P" + String(pumpTimer);  // Show remaining time
    pumpTimer--;                              // Decrement timer each second

    // Timer reached zero - turn off pump
    if (pumpTimer == 0) {
      pumpState = 0;
      iot.eventUpdate("St", 0);  // Notify cloud that timed run is complete
    }
  }

  // SAFETY PROTECTION - Pump Runaway Detection
  // If pump is on without an active timer, track how long it runs
  if (!digitalRead(PUMP) && pumpTimer == 0) {
    pumpOnProtectionTimer++;

    // If pump has been running too long (5x interval), shut it down
    if (pumpOnProtectionTimer >= interval * 5) {
      pumpOnProtectionTimer = 0;
      pumpState = 0;
      pumpTimer = 0;
      showPopupMessage("Pump\n\nProtection\n\nStop");
    }
  } else {
    pumpOnProtectionTimer = 0;
  }
}

// ==========================================================================================
// DATA SEND FUNCTION
// ==========================================================================================
void readAndSendData() {
  // Calculate current system state and send to cloud
  uint32_t onState = 0;

  if (pumpTimer > 0)
    onState = pumpTimer;  // Remaining timer value
  else
    onState = pumpOnProtectionTimer;  // Manual run duration for safety tracking

  float payload[numVariables] = { onState };
  iot.update(payload);
}

// ==========================================================================================
// OLED DISPLAY UPDATE FUNCTION
// ==========================================================================================
void updateDisplay() {
  // Get current network state
  iotwebconf::NetworkState curr_state = iotWebConf.getState();

  // Network state change detection
  if (curr_state == iotwebconf::Boot) {
    prev_state = curr_state;
  } else if (curr_state == iotwebconf::NotConfigured) {
    if (prev_state == iotwebconf::Boot) {
      displaytime = 5;
      prev_state = curr_state;
      noti = "-State-\n\nno config\nstay in\nAP Mode";
    }
  } else if (curr_state == iotwebconf::ApMode) {
    if (prev_state == iotwebconf::Boot) {
      displaytime = 5;
      prev_state = curr_state;
      noti = "-State-\n\nAP Mode\nfor 30 sec";
    } else if (prev_state == iotwebconf::Connecting) {
      displaytime = 5;
      prev_state = curr_state;
      noti = "-State-\n\nX  can't\nconnect\nwifi\ngo AP Mode";
    }
  } else if (curr_state == iotwebconf::OnLine) {
    if (prev_state == iotwebconf::Connecting) {
      displaytime = 5;
      prev_state = curr_state;
      noti = "-State-\n\nwifi\nconnect\nsuccess\n" + String(WiFi.RSSI()) + " dBm";
    }
  } else if (curr_state == iotwebconf::OffLine) {
    displaytime = 10;
    prev_state = curr_state;
    noti = "-State-\n\nX wifi\ndisconnect\ngo AP Mode";
  } else if (curr_state == iotwebconf::Connecting) {
    if (prev_state == iotwebconf::ApMode) {
      displaytime = 5;
      prev_state = curr_state;
      noti = "-State-\n\nwifi\nconnecting";
    } else if (prev_state == iotwebconf::OnLine) {
      displaytime = 10;
      prev_state = curr_state;
      noti = "-State-\n\nX  wifi\ndisconnect\nreconnecting";
    }
  } else if (curr_state == iotwebconf::OnLine) {
    if (prev_state == iotwebconf::Connecting) {
      displaytime = 5;
      prev_state = curr_state;
      noti = "-State-\n\nwifi\nconnect\nsuccess\n" + String(WiFi.RSSI()) + " dBm";
    }
  }

  // CynoIOT cloud notifications
  if (iot.noti != "" && displaytime == 0) {
    displaytime = 3;
    noti = iot.noti;
    iot.noti = "";
  }

  // Display notification messages
  if (displaytime) {
    displaytime--;
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print(noti);
    Serial.println(noti);
  }

  // Display popup messages
  else if (popupShowTimer) {
    popupShowTimer--;
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print(popupStatus);
  }

  // Regular status display (default screen)
  else {
    oled.clearDisplay();

    // Main display area - pump state
    oled.setTextSize(2);
    oled.setCursor(0, 15);

    if (pumpState) {
      oled.print("PUMP ON");

      // Status bar - timer countdown
      oled.setCursor(40, 32);
      oled.setTextSize(1);
      oled.print(displayStatus);
    } else {
      oled.print("PUMP OFF");
    }


    // Title bar
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("Pump Ctrl");
  }

  // WiFi status icon display
  if (curr_state == iotwebconf::NotConfigured || curr_state == iotwebconf::ApMode)
    oled.drawBitmap(55, 0, wifi_ap, 9, 8, 1);
  else if (curr_state == iotwebconf::Connecting) {
    if (t_connecting == 1) {
      oled.drawBitmap(56, 0, wifi_on, 8, 8, 1);
      t_connecting = 0;
    } else {
      t_connecting = 1;
    }
  } else if (curr_state == iotwebconf::OnLine) {
    if (iot.status())
      oled.drawBitmap(56, 0, wifi_on, 8, 8, 1);
    else
      oled.drawBitmap(56, 0, wifi_nointernet, 8, 8, 1);
  } else if (curr_state == iotwebconf::OffLine)
    oled.drawBitmap(56, 0, wifi_off, 8, 8, 1);

  // Render display to OLED screen
  oled.display();
}

// ==========================================================================================
// POPUP MESSAGE DISPLAY FUNCTION
// ==========================================================================================
void showPopupMessage(String msg, uint8_t timeout) {
  popupStatus = msg;
  popupShowTimer = timeout;
}

// ==========================================================================================
// HARDWARE OUTPUT UPDATE FUNCTION
// ==========================================================================================
void updateHardwareOutputs() {
  // Check if pump state has changed
  bool lastPumpState = (digitalRead(PUMP) == LOW);
  if (pumpState != lastPumpState) {
    if (pumpState) {
      // Pump ON: Set to OUTPUT mode and drive LOW (active low)
      pinMode(PUMP, OUTPUT);
      digitalWrite(PUMP, LOW);
    } else {
      // Pump OFF: Set to INPUT mode (high impedance)
      pinMode(PUMP, INPUT);
      digitalWrite(PUMP, HIGH);
    }
    iot.eventUpdate("P", pumpState);
  }
}

// ==========================================================================================
// WEB SERVER ROOT PAGE HANDLER
// ==========================================================================================
void handleRoot() {
  if (iotWebConf.handleCaptivePortal()) {
    return;
  }

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
void configSaved() {
  Serial.println("Configuration was updated.");
}

// ==========================================================================================
// WIFI CONNECTION SUCCESS CALLBACK
// ==========================================================================================
void wifiConnected() {
  Serial.println("WiFi was connected.");

  MDNS.begin(iotWebConf.getThingName());
  MDNS.addService("http", "tcp", 80);

  Serial.printf("Ready! Open http://%s.local in your browser\n", String(iotWebConf.getThingName()));

  if ((String)emailParamValue != "") {
    Serial.println("login with " + (String)emailParamValue);
    iot.connect((String)emailParamValue);
  }
}

// ==========================================================================================
// FORM VALIDATION CALLBACK
// ==========================================================================================
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper) {
  Serial.println("Validating form.");
  bool valid = true;
  return valid;
}

// ==========================================================================================
// EEPROM CLEAR FUNCTION
// ==========================================================================================
void clearEEPROM() {
  Serial.println("clearEEPROM() called!");

  EEPROM.begin(512);
  for (int i = 0; i < 512; i++) {
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
void reboot() {
  server.send(200, "text/plain", "rebooting");
  delay(1000);
  ESP.restart();
}

// ==========================================================================================
// STATUS JSON ENDPOINT HANDLER
// ==========================================================================================
void handleStatus() {
  String json = "{";
  json += "\"pump\":" + String(pumpState);
  json += "}";

  server.send(200, "application/json", json);
}
