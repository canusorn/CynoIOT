// เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266
#ifdef ESP8266
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

// เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#elif defined(ESP32)
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <WiFi.h>
#endif

#include <EEPROM.h> // EEPROM library for storing data
#include <Wire.h>   // Wire library for I2C communication

// IoTWebconfrom https://github.com/canusorn/IotWebConf-iotbundle
#include <IotWebConf.h>
#include <IotWebConfUsing.h>

#include <Adafruit_GFX.h>     // Adafruit GFX library by Adafruit
#include <Adafruit_SSD1306.h> // Adafruit SSD1306 library by Adafruit
#include <cynoiot.h>          // CynoIOT by IoTbundle

// DS18B20 Temperature Sensor pin (required primary sensor)
#define TEMP_PIN 9

#include "OneWireESP32.h"
float temperature = NAN;

// ถ้าต้องการใช้ Ultrasonic Distance Sensor ก็ต้องกำหนด DISTANCE_PIN
// #define DISTANCE_PIN

// สร้าง object ชื่อ iot
Cynoiot iot;

const char thingName[] = "hydro";
const char wifiInitialApPassword[] = "iotbundle";

#define STRING_LEN 128
#define NUMBER_LEN 32

// Static HTML stored in flash memory
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
        .sensor-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
            margin: 15px 0;
        }
        .sensor-card {
            background: #f9f9f9;
            border-radius: 8px;
            padding: 15px;
            text-align: center;
            border: 1px solid #eee;
        }
        .sensor-label {
            font-size: 12px;
            color: #888;
            text-transform: uppercase;
            margin-bottom: 5px;
        }
        .sensor-value {
            font-size: 22px;
            font-weight: bold;
            color: #333;
        }
        .sensor-unit {
            font-size: 12px;
            color: #888;
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

    function updateSensors() {
        fetch('/sensor')
            .then(response => response.json())
            .then(data => {
                document.getElementById('tds_value').textContent = data.tds.toFixed(1);
                document.getElementById('ec_value').textContent = data.ec.toFixed(1);
                document.getElementById('temp_value').textContent = data.temperature.toFixed(1);
                var wl = document.getElementById('wl_value');
                if (wl) wl.textContent = data.water_level.toFixed(1);
            })
            .catch(error => console.error('Sensor error:', error));
    }

    window.onload = function() {
        updateStatus();
        updateSensors();
    };
    setInterval(updateStatus, 10000);
    setInterval(updateSensors, 5000);
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
        <h3>Sensor Data</h3>
        <div class="sensor-grid">
            <div class="sensor-card">
                <div class="sensor-label">TDS</div>
                <div class="sensor-value"><span id="tds_value">--</span></div>
                <div class="sensor-unit">ppm</div>
            </div>
            <div class="sensor-card">
                <div class="sensor-label">EC</div>
                <div class="sensor-value"><span id="ec_value">--</span></div>
                <div class="sensor-unit">&micro;S/cm</div>
            </div>
            <div class="sensor-card">
                <div class="sensor-label">Temperature</div>
                <div class="sensor-value"><span id="temp_value">--</span></div>
                <div class="sensor-unit">&deg;C</div>
            </div>
            <div class="sensor-card">
                <div class="sensor-label">Water Level</div>
                <div class="sensor-value"><span id="wl_value">--</span></div>
                <div class="sensor-unit">cm</div>
            </div>
        </div>
    </div>
<br>
    <div class="container">
        <h3>Pump Control</h3>
        <div class="btn-group">
            <strong>PUMP:</strong> <span id="pump_status" class="status">Loading...</span><br>
            <button class="btn btn-on" onclick="togglePin('pump', '1')">PUMP ON</button>
            <button class="btn btn-off" onclick="togglePin('pump', '0')">PUMP OFF</button>
        </div>
    </div>
</body>
</html>
)rawliteral";

// -- Method declarations.
void handleRoot();
void handleStatus();
void handleSensor();
#ifdef DISTANCE_PIN
float handleWaterLevelSpike(float new_distance);
#endif
// -- Callback methods.
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper);

// ตั้งค่า pin สำหรับเซ็นเซอร์และขา OUTPUT
#ifdef ESP8266
#ifdef DISTANCE_PIN
#define TRIG_PIN D5 // D5 for ESP8266
#define ECHO_PIN D6 // D6 for ESP8266
#endif
#define TDS_PIN A0 // A0 for ESP8266

#define RSTPIN D8 // Define RSTPIN for ESP8266

#elif defined(ESP32)
#define RSTPIN 8
#define PUMP 37

#ifdef CONFIG_IDF_TARGET_ESP32S2
// #define TRIG_PIN 18
// #define ECHO_PIN 16
// #define TDS_PIN 5 // ADC1_CH0 for ESP32S2
#ifdef DISTANCE_PIN
#define TRIG_PIN 11 // ✅ Safe GPIO pin   RX
#define ECHO_PIN 12 // ✅ Safe GPIO pin   TX
#endif
#define TDS_PIN 5 // ✅ ADC1_CH3 (safe ADC pin)   // old 4

#else
#ifdef DISTANCE_PIN
#define TRIG_PIN 4 // GPIO4 for ESP32
#define ECHO_PIN 2 // GPIO2 for ESP32
#endif
#define TDS_PIN 34 // ADC1_CH6 for ESP32 (only ADC1 pins available)
#endif

#endif

// TDS sensor calibration
#define TDS_CALIBRATION_VOLTAGE 3.3
float tds_calibration_coefficient = 10.0;

unsigned long previousMillis = 0;
#ifdef DISTANCE_PIN
float water_level, previous_distance = 0;
uint8_t consecutive_changes = 0;
#endif
float tds_value, ec_value;
bool pumpState = true; // Track pump state, default ON

#define OLED_RESET -1 // GPIO0
Adafruit_SSD1306 oled(OLED_RESET);

// สร้าง object สำหรับ DNS Server และ Web Server
DNSServer dnsServer;
WebServer server(80);

#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;

#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif

char emailParamValue[STRING_LEN];

IotWebConf
    iotWebConf(thingName, &dnsServer, &server,
               wifiInitialApPassword); // version defind in iotbundle.h file
// -- You can also use namespace formats e.g.: iotwebconf::TextParameter
IotWebConfParameterGroup login =
    IotWebConfParameterGroup("login", "ล็อกอิน(สมัครที่เว็บก่อนนะครับ)");

IotWebConfTextParameter emailParam =
    IotWebConfTextParameter("อีเมลล์", "emailParam", emailParamValue, STRING_LEN);

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
const uint8_t wifi_on[] = {0x00, 0x3c, 0x42, 0x99,
                           0x24, 0x00, 0x18, 0x18}; // 'wifi-1', 8x8px
const uint8_t wifi_off[] = {0x01, 0x3e, 0x46, 0x99, 0x34,
                            0x20, 0x58, 0x98}; // 'wifi_nointernet-1', 8x8px
const uint8_t wifi_ap[] = {
    0x41, 0x00, 0x80, 0x80, 0xa2, 0x80, 0xaa, 0x80,
    0xaa, 0x80, 0x88, 0x80, 0x49, 0x00, 0x08, 0x00}; // 'router-2', 9x8px
const uint8_t wifi_nointernet[] = {0x03, 0x7b, 0x87, 0x33,
                                   0x4b, 0x00, 0x33, 0x33};
uint8_t t_connecting;
iotwebconf::NetworkState prev_state = iotwebconf::Boot;
uint8_t displaytime;
String noti;
bool ota_updated = false;
uint16_t timer_nointernet;
uint8_t numVariables;
uint8_t sampleUpdate, updateValue = 10;

void handleEvent(String event, String value) {

  // EVENT: SQ - Sequence Mode Control
  if (event == "PUMP") {
    Serial.println("PUMP: " + value);
    // iot.debug("Event PUMP received with value: " + value);

    // Turn pump on or off
    if (value == "1") {
      digitalWrite(PUMP, HIGH); // Turn PUMP ON
      Serial.println("PUMP turned ON");
      iot.debug("PUMP turned ON");
    } else if (value == "0") {
      digitalWrite(PUMP, LOW); // Turn PUMP OFF
      Serial.println("PUMP turned OFF");
      iot.debug("PUMP turned OFF");
    }
  }

  // EVENT: M - Global Mode Selection
  else if (event == "CAL") {

    EEPROM.begin(512);

    Serial.println("calibration: " + value);
    // iot.debug("Event M received with value: " + value);

    // Convert string value to float and update calibration coefficient
    float newCalibration = value.toFloat();
    if (newCalibration > 0) {
      // Persist calibration coefficient to EEPROM (only if changed to reduce
      // write cycles)
      float storedCalibration;
      EEPROM.get(500, storedCalibration);
      if (storedCalibration != newCalibration) {
        tds_calibration_coefficient = newCalibration;
        EEPROM.put(500, tds_calibration_coefficient);
        EEPROM.commit();
        Serial.println("TDS Calibration coefficient saved to EEPROM: " +
                       String(tds_calibration_coefficient));
        iot.debug("TDS Calibration coefficient = " +
                  String(tds_calibration_coefficient) + " saved to EEPROM");
      }
    }

    EEPROM.end(); // Close EEPROM session

    // Reset sensor values after calibration
    tds_value = 0;
  }
}

void iotSetup() {
  // ตั้งค่าตัวแปรที่จะส่งขึ้นเว็บ
#ifdef DISTANCE_PIN
  numVariables = 4;                                              // จำนวนตัวแปร
  String keyname[numVariables] = {"tds", "ec", "temp", "level"}; // ชื่อตัวแปร
#else
  numVariables = 3;                                     // จำนวนตัวแปร
  String keyname[numVariables] = {"tds", "ec", "temp"}; // ชื่อตัวแปร
#endif
  iot.setkeyname(keyname, numVariables);

  iot.setEventCallback(handleEvent);

  const uint8_t version = 1;              // เวอร์ชั่นโปรเจคนี้
  iot.setTemplate("hydroponic", version); // เลือกเทมเพลตแดชบอร์ด

  Serial.println("ClinetID:" + String(iot.getClientId()));
}

void time1sec() {
  // if can't connect to network
  if (iotWebConf.getState() == iotwebconf::OnLine) {
    if (iot.status()) {
      timer_nointernet = 0;
    } else {
      timer_nointernet++;
      if (timer_nointernet > 30)
        Serial.println("No connection time : " + String(timer_nointernet));
    }
  }

  // reconnect wifi if can't connect server
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

void setup() {
  Serial.begin(115200);

  // Initialize EEPROM and read TDS calibration coefficient
  EEPROM.begin(512);
  EEPROM.get(500, tds_calibration_coefficient);

  // Check if EEPROM value is valid (not NaN or zero)
  if (isnan(tds_calibration_coefficient) ||
      tds_calibration_coefficient == 0.0) {
    tds_calibration_coefficient = 10.0; // Set default value
    Serial.println("TDS Calibration coefficient set to default: 10.0");
  } else {
    Serial.println("TDS Calibration coefficient loaded from EEPROM: " +
                   String(tds_calibration_coefficient));
  }
  EEPROM.end();

#ifdef DISTANCE_PIN
  // Initialize SR04 Ultrasonic Sensor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
#endif

  // Initialize TDS Sensor
  pinMode(TDS_PIN, ANALOG);

  // Initialize PUMP
  pinMode(PUMP, OUTPUT);
  digitalWrite(PUMP, HIGH); // ON for default

#ifdef ESP32
  analogReadResolution(12);
#endif

  //------Display LOGO at start------
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  oled.clearDisplay();
  oled.drawBitmap(
      16, 5, logo_bmp, 33, 30,
      1); // call the drawBitmap function and pass it the array from above
  oled.setTextSize(1);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 40);
  oled.print("  CYNOIOT");
  oled.display();

  // for clear eeprom jump D4 to GND
  pinMode(RSTPIN, INPUT_PULLUP);
  if (digitalRead(RSTPIN) == false) {
    delay(1000);
    if (digitalRead(RSTPIN) == false) {
      oled.clearDisplay();
      oled.setCursor(0, 0);
      oled.print("Clear All data\n rebooting");
      oled.display();
      delay(1000);
      clearEEPROM();
    }
  }

  login.addItem(&emailParam);

#ifdef CONFIG_IDF_TARGET_ESP32S2
  iotWebConf.setStatusPin(15);
#endif

  // iotWebConf.setConfigPin(CONFIG_PIN);
  //  iotWebConf.addSystemParameter(&stringParam);
  iotWebConf.addParameterGroup(&login);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.getApTimeoutParameter()->visible = false;
  iotWebConf.setWifiConnectionCallback(&wifiConnected);

  // -- Define how to handle updateServer calls.
  iotWebConf.setupUpdateServer(
      [](const char *updatePath) { httpUpdater.setup(&server, updatePath); },
      [](const char *userName, char *password) {
        httpUpdater.updateCredentials(userName, password);
      });

  // -- Initializing the configuration.
  iotWebConf.init();

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", [] { iotWebConf.handleConfig(); });
  server.on("/cleareeprom", clearEEPROM);
  server.on("/reboot", reboot);

  // Manual pump control via HTTP
  server.on("/gpio/pump", []() {
    String state = server.arg("state");
    if (state == "1") {
      pumpState = true;
      digitalWrite(PUMP, HIGH); // ON
    } else if (state == "0") {
      pumpState = false;
      digitalWrite(PUMP, LOW); // OFF
    }
    server.send(200, "text/plain", "OK");
  });

  // Status endpoint - returns JSON with current pump state
  server.on("/status", handleStatus);

  // Sensor data endpoint - returns JSON with current sensor readings
  server.on("/sensor", handleSensor);

  server.onNotFound([]() { iotWebConf.handleNotFound(); });

  Serial.println("Ready.");

  iotSetup();
}

void loop() {
  iot.handle();
  iotWebConf.doLoop();
  server.handleClient();
#ifdef ESP8266
  MDNS.update();
#endif

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 1000) { // run every 1 second
    previousMillis = currentMillis;

    display_update();
    time1sec();
    sampleUpdate++;

    if (sampleUpdate >= updateValue) {
      sampleUpdate = 0;

      //------get data from TDS Sensor------
      readTDS();

      //------get data from DS18B20 Temperature Sensor------
      readTemperature();

#ifdef DISTANCE_PIN
      //------get data from SR04 Ultrasonic Sensor------
      readWaterLevel();
#endif

      // display data in serialmonitor
      Serial.println("TDS: " + String(tds_value) +
                     "ppm  EC: " + String(ec_value) +
                     "μS/cm  Temp: " + String(temperature, 1) + "°C"
#ifdef DISTANCE_PIN
                     + "  Water Level: " + String(water_level) + "cm"
#endif
      );

      if (isnan(ec_value) || isnan(temperature))
        return;

      //  อัพเดทค่าใหม่ในรูปแบบ array
#ifdef DISTANCE_PIN
      float val[numVariables] = {tds_value, ec_value, temperature, water_level};
#else
      float val[numVariables] = {tds_value, ec_value, temperature};
#endif
      iot.update(val);
    }
  }
}

#ifdef DISTANCE_PIN
float handleWaterLevelSpike(float new_distance) {
  const float SPIKE_THRESHOLD = 1.0;

  if (previous_distance == 0) {
    previous_distance = new_distance;
    consecutive_changes = 0;
    return new_distance;
  }

  float change = abs(new_distance - previous_distance);

  if (change < SPIKE_THRESHOLD) {
    consecutive_changes = 0;
    previous_distance = new_distance;
    return new_distance;
  } else {
    consecutive_changes++;
    if (consecutive_changes >= 2) {
      consecutive_changes = 0;
      previous_distance = new_distance;
      return new_distance;
    } else {
      return previous_distance;
    }
  }
}
#endif

#ifdef DISTANCE_PIN
void readWaterLevel() {
  // Send 10us pulse to trigger
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read echo pulse
  long duration = pulseIn(ECHO_PIN, HIGH);

  // Calculate distance in cm (speed of sound = 343 m/s = 0.0343 cm/μs)
  float distance = duration * 0.0343 / 2;

  float filtered_distance = handleWaterLevelSpike(distance);

  // Inverse water level to negative value (water depth from sensor)
  filtered_distance = -filtered_distance;

  if (water_level == 0)
    water_level = filtered_distance;
  else
    water_level = (0.3 * filtered_distance) + (0.7 * water_level);
}
#endif
void readTDS() {
  // Read analog value from TDS sensor
  int rawValue = analogRead(TDS_PIN);

  // Convert to voltage (ESP8266: 10-bit ADC 0-1023, ESP32: 12-bit ADC 0-4095)
#ifdef ESP32
  float voltage = rawValue * (TDS_CALIBRATION_VOLTAGE / 4096.0);
#else
  float voltage = rawValue * (TDS_CALIBRATION_VOLTAGE / 1024.0);
#endif

  // Convert voltage to TDS value (ppm)
  // This is a simplified conversion - actual calibration may be needed
  float current_tds = (133.42 * voltage * voltage * voltage -
                       255.86 * voltage * voltage + 857.39 * voltage) *
                      tds_calibration_coefficient * 0.1;

  // Apply EMA filter (coefficient 0.1)
  if (tds_value == 0)
    tds_value = current_tds; // Initialize with first reading
  else
    tds_value = (0.1 * current_tds) + (0.9 * tds_value);

  // Convert TDS to EC (Electrical Conductivity)
  // EC (μS/cm) = TDS (ppm) * 2 (approximate conversion factor)
  ec_value = tds_value * 2.0;

  // Ensure values are within reasonable range
  if (ec_value < 0)
    ec_value = 0;
  if (ec_value > 5000)
    ec_value = 5000; // Max 5000 μS/cm for hydroponic systems
}

void readTemperature() {
    const uint8_t MaxDevs = 1;

    float currTemp[MaxDevs];

    OneWire32 ds(TEMP_PIN); //gpio pin

	uint64_t addr[MaxDevs];

	//uint64_t addr[] = {
	//	0x183c01f09506f428,
	//	0xf33c01e07683de28,
	//};

	//to find addresses
	uint8_t devices = ds.search(addr, MaxDevs);
	for (uint8_t i = 0; i < devices; i += 1) {
		Serial.printf("%d: 0x%llx,\n", i, addr[i]);
		//char buf[20]; snprintf( buf, 20, "0x%llx,", addr[i] ); Serial.println(buf);
	}
	//end

	for(;;){
		ds.request();
		vTaskDelay(750 / portTICK_PERIOD_MS);
		for(byte i = 0; i < MaxDevs; i++){
			uint8_t err = ds.getTemp(addr[i], currTemp[i]);
			if(err){
				const char *errt[] = {"", "CRC", "BAD","DC","DRV"};
				Serial.print(i); Serial.print(": "); Serial.println(errt[err]);
			}else{
				Serial.print(i); Serial.print(": "); Serial.println(currTemp[i]);
			}
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

  // Valid reading
  // Apply simple EMA filter for temperature (alpha = 0.2)
  if (isnan(temperature))
    temperature = currTemp[1];
  else
    temperature = (0.2 * currTemp[1]) + (0.8 * temperature);
}

void display_update() {

  //------Update OLED------
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("--Hydro--");

  oled.setCursor(0, 12);
  oled.println("TDS " + String(tds_value, 0));
  oled.setCursor(0, 24);
  oled.println("EC " + String(ec_value, 0));
  oled.setCursor(0, 36);
  oled.println("Temp " + String(temperature, 1));

  // display status
  iotwebconf::NetworkState curr_state = iotWebConf.getState();
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
    } else if (prev_state == iotwebconf::OnLine) {
      displaytime = 10;
      prev_state = curr_state;
      noti = "-State-\n\nX  wifi\ndisconnect\ngo AP Mode";
    }
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
      noti =
          "-State-\n\nwifi\nconnect\nsuccess\n" + String(WiFi.RSSI()) + " dBm";
    }
  }

  if (iot.noti != "" && displaytime == 0) {
    displaytime = 3;
    noti = iot.noti;
    iot.noti = "";
  }

  if (displaytime) {
    displaytime--;
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print(noti);
    Serial.println(noti);
  }

  // display state
  if (curr_state == iotwebconf::NotConfigured ||
      curr_state == iotwebconf::ApMode)
    oled.drawBitmap(55, 0, wifi_ap, 9, 8, 1);
  else if (curr_state == iotwebconf::Connecting) {
    if (t_connecting == 1) {
      oled.drawBitmap(56, 0, wifi_on, 8, 8, 1);
      t_connecting = 0;
    } else {
      t_connecting = 1;
    }
  } else if (curr_state == iotwebconf::OnLine) {
    if (iot.status()) {
      oled.drawBitmap(56, 0, wifi_on, 8, 8, 1);
    } else {
      oled.drawBitmap(56, 0, wifi_nointernet, 8, 8, 1);
    }
  } else if (curr_state == iotwebconf::OffLine)
    oled.drawBitmap(56, 0, wifi_off, 8, 8, 1);

  oled.display();
}

void handleRoot() {
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal()) {
    // -- Captive portal request were already served.
    return;
  }

  String s = FPSTR(htmlTemplate);
  s.replace("%STATE%",
            String(iotWebConf.getState())); // Replace state placeholder
  s.replace("%THING_NAME%",
            String(iotWebConf.getThingName()));      // Replace device name
  s.replace("%EMAIL%", String(emailParamValue));     // Replace email
  s.replace("%SSID%", String(iotWebConf.getSSID())); // Replace SSID
  s.replace("%RSSI%", String(WiFi.RSSI()));          // Replace RSSI
  s.replace("%ESP_ID%", String(iot.getClientId()));  // Replace ESP ID
  s.replace("%VERSION%", String(IOTVERSION));        // Replace version

  server.send(200, "text/html", s);
}

void configSaved() { Serial.println("Configuration was updated."); }

void wifiConnected() {
  Serial.println("WiFi was connected.");
  MDNS.begin(iotWebConf.getThingName());
  MDNS.addService("http", "tcp", 80);

  Serial.printf("Ready! Open http://%s.local in your browser\n",
                String(iotWebConf.getThingName()));
  if ((String)emailParamValue != "") {
    // เริ่มเชื่อมต่อ หลังจากต่อไวไฟได้
    Serial.println("login");
    iot.connect((String)emailParamValue);
  }
}

bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper) {
  Serial.println("Validating form.");
  bool valid = true;

  /*
      int l = webRequestWrapper->arg(stringParam.getId()).length();
      if (l < 3)
      {
        stringParam.errorMessage = "Please provide at least 3 characters for
     this test!"; valid = false;
      }
    */
  return valid;
}

void clearEEPROM() {
  EEPROM.begin(512);
  // write a 0 to all 512 bytes of the EEPROM
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }

  EEPROM.end();
  server.send(200, "text/plain", "Clear all data\nrebooting");
  delay(1000);
  ESP.restart();
}

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

// ==========================================================================================
// SENSOR DATA JSON ENDPOINT HANDLER
// ==========================================================================================
void handleSensor() {
  String json = "{";
  json += "\"tds\":" + String(tds_value, 2) + ",";
  json += "\"ec\":" + String(ec_value, 2) + ",";
  json += "\"temperature\":" + String(temperature, 2) + ",";
#ifdef DISTANCE_PIN
  json += "\"water_level\":" + String(water_level, 2);
#else
  json += "\"water_level\":0";
#endif
  json += "}";
  server.send(200, "application/json", json);
}
