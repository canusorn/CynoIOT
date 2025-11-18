/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

// เลือกรุ่นที่ใช้งาน
#define NOSENSOR_MODEL
// #define HUMID_MODEL
// #define TEMP_HUMID_MODEL
// #define TEMP_HUMID_EC_MODEL
// #define ALL_7IN1_MODEL

// เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266
#ifdef ESP8266
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h>

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
#include <ModbusMaster.h>     // ModbusMaster by Doc Walker

#ifdef ESP8266
SoftwareSerial RS485Serial;
#elif defined(ESP32)
#define RS485Serial Serial1
#endif

// ตั้งค่า pin สำหรับต่อกับ MAX485 และขา OUTPUT
#ifdef ESP8266
#define MAX485_RO D7 // RX
#define MAX485_RE D6
#define MAX485_DE D6
#define MAX485_DI D0 // TX

#define PUMP D1
#define CH1 D2
#define CH2 D3
#define CH3 D4
#define CH4 D5

#elif defined(ESP32)
#define RSTPIN 8

#ifdef CONFIG_IDF_TARGET_ESP32S2
#define MAX485_RO 18
#define MAX485_RE 9
#define MAX485_DE 9
#define MAX485_DI 21

#else
#define MAX485_RO 23
#define MAX485_RE 9
#define MAX485_DE 9
#define MAX485_DI 26
#endif

#define PUMP 3
#define CH1 7
#define CH2 5
#define CH3 11
#define CH4 12

#endif

const char thingName[] = "SmartFarm";             // ชื่ออุปกรณ์
const char wifiInitialApPassword[] = "iotbundle"; // รหัสผ่านเริ่มต้นสำหรับ AP Mode

#define ADDRESS 1

#define STRING_LEN 128 // ความยาวสูงสุดของสตริง
#define NUMBER_LEN 32  // ความยาวสูงสุดของตัวเลข

ModbusMaster node;
Cynoiot iot;

// กำหนดค่าสำหรับจอ OLED
#define OLED_RESET 0 // GPIO0
Adafruit_SSD1306 oled(OLED_RESET);

// HTML template เก็บไว้ใน flash memory
const char htmlTemplate[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
    <title>CynoIoT config page</title>
    <script>
    if (%STATE% == 0) {
        location.href='/config';
    }
    </script>
</head>
<body>
    CynoIoT config data
    <ul>
        <li>Device name: %THING_NAME%</li>
        <li>อีเมลล์: %EMAIL%</li>
        <li>WIFI SSID: %SSID%</li>
        <li>RSSI: %RSSI% dBm</li>
        <li>ESP ID: %ESP_ID%</li>
        <li>Version: %VERSION%</li>
    </ul>
    <button style='margin-top: 10px;' type='button' onclick="location.href='/reboot';">รีบูทอุปกรณ์</button><br><br>
    <a href='/config'>configure page แก้ไขข้อมูล wifi และ user</a>
</body>
</html>
)rawliteral";

// -- ประกาศฟังก์ชัน
void handleRoot();
// -- ฟังก์ชัน Callback
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper);

unsigned long previousMillis;

// สร้าง object สำหรับ DNS Server และ Web Server
DNSServer dnsServer;
WebServer server(80);

#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;

#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif

// ตัวแปรเก็บค่าอีเมล
char emailParamValue[STRING_LEN];

// สร้าง object สำหรับการตั้งค่า WiFi ผ่านเว็บ
IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword); // version defind in iotbundle.h file
// -- You can also use namespace formats e.g.: iotwebconf::TextParameter
IotWebConfParameterGroup login = IotWebConfParameterGroup("login", "ล็อกอิน(สมัครที่เว็บก่อนนะครับ)");

// พารามิเตอร์สำหรับกรอกอีเมล
IotWebConfTextParameter emailParam = IotWebConfTextParameter("อีเมลล์", "emailParam", emailParamValue, STRING_LEN);

// รูปภาพโลโก้ขนาด 33x30 พิกเซล
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
// รูปไอคอนสถานการ WiFi ขนาด 8x8 พิกเซล
const uint8_t wifi_on[] = {0x00, 0x3c, 0x42, 0x99, 0x24, 0x00, 0x18, 0x18};                                                 // 'wifi-1', 8x8px
const uint8_t wifi_off[] = {0x01, 0x3e, 0x46, 0x99, 0x34, 0x20, 0x58, 0x98};                                                // 'wifi_nointernet-1', 8x8px
const uint8_t wifi_ap[] = {0x41, 0x00, 0x80, 0x80, 0xa2, 0x80, 0xaa, 0x80, 0xaa, 0x80, 0x88, 0x80, 0x49, 0x00, 0x08, 0x00}; // 'router-2', 9x8px
const uint8_t wifi_nointernet[] = {0x03, 0x7b, 0x87, 0x33, 0x4b, 0x00, 0x33, 0x33};

// ตัวแปรสำหรับการแสดงผลและการเชื่อมต่อ
uint8_t t_connecting;
uint8_t displaytime;
iotwebconf::NetworkState prev_state = iotwebconf::Boot;
uint8_t sampleUpdate;
String noti;
uint16_t timer_nointernet;
uint8_t numVariables;

float humidity, temperature, ph;
uint32_t conductivity, nitrogen, phosphorus, potassium;
uint8_t state; // 0-auto  1-timer  2-off
uint16_t interval;
uint16_t pumpTimer, ch1Timer, ch2Timer, ch3Timer, ch4Timer;
String displayStatus;

bool pumpState, ch1State, ch2State, ch3State, ch4State;
bool onState;
// ฟังก์ชันสำหรับรับ event จากเซิร์ฟเวอร์
void handleEvent(String event, String value)
{
    EEPROM.begin(512);
    if (event == "Start") // ตรวจสอบว่าเป็น event ชื่อ "Start" หรือไม่
    {
        Serial.println("Start: " + value);
        if (value)
        {
            onState = true;

            uint8_t chNum = 0;

            if (ch1State)
            {
                ch1Timer = interval;
                chNum++;
            }
            if (ch2State)
            {
                ch2Timer = interval;
                chNum++;
            }
            if (ch3State)
            {
                ch3Timer = interval;
                chNum++;
            }
            if (ch4State)
            {
                ch4Timer = interval;
                chNum++;
            }

            if (chNum == 0)
                chNum = 1;

            if (pumpState)
                pumpTimer = chNum * interval;
        }
        else if (value == "off")
        {
            digitalWrite(PUMP, LOW);
            digitalWrite(CH1, LOW);
            digitalWrite(CH2, LOW);
            digitalWrite(CH3, LOW);
            digitalWrite(CH4, LOW);
            pumpTimer = 0;
            ch1Timer = 0;
            ch2Timer = 0;
            ch3Timer = 0;
            ch4Timer = 0;
            onState = false;
        }
    }
    else if (event == "Mode")
    {
        Serial.println("Mode: " + value);
        if (value == "auto" || value == "null")
        {
            state = 0;
        }
        else if (value == "timer")
        {
            state = 1;
        }
        else if (value == "off")
        {
            state = 2;
        }

        EEPROM.write(500, state);
        EEPROM.commit();
    }
    else if (event == "Interval")
    {
        Serial.println("Interval: " + value);
        interval = (uint8_t)value.toInt(); // แปลงค่า value เป็น int แล้วเก็บไว้ใน interval

        uint16_t currentValue = (uint16_t)EEPROM.read(498) | ((uint16_t)EEPROM.read(499) << 8);
        if (currentValue != interval)
        {
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
    else if (event == "pump")
    {
        Serial.println("pump use : " + value);
        pumpState = (bool)value.toInt();

        // Save pumpState to EEPROM
        EEPROM.write(497, (uint8_t)pumpState);
        EEPROM.commit();
    }
    else if (event == "ch1")
    {
        Serial.println("ch1 use : " + value);
        ch1State = (bool)value.toInt();

        // Save ch1State to EEPROM
        EEPROM.write(496, (uint8_t)ch1State);
        EEPROM.commit();
    }
    else if (event == "ch2")
    {
        Serial.println("ch2 use : " + value);
        ch2State = (bool)value.toInt();

        // Save ch2State to EEPROM
        EEPROM.write(495, (uint8_t)ch2State);
        EEPROM.commit();
    }
    else if (event == "ch3")
    {
        Serial.println("ch3 use : " + value);
        ch3State = (bool)value.toInt();

        // Save ch3State to EEPROM
        EEPROM.write(494, (uint8_t)ch3State);
        EEPROM.commit();
    }
    else if (event == "ch4")
    {
        Serial.println("ch4 use : " + value);
        ch4State = (bool)value.toInt();

        // Save ch4State to EEPROM
        EEPROM.write(493, (uint8_t)ch4State);
        EEPROM.commit();
    }
    EEPROM.end();
}

// ฟังก์ชันตั้งค่าการเชื่อมต่อกับ CynoIOT
void iotSetup()
{
    // read purifierStartValue from EEPROM
    Serial.println("Loading setting from EEPROM");
    EEPROM.begin(512);
    interval = (uint16_t)EEPROM.read(498) | ((uint16_t)EEPROM.read(499) << 8);
    if (interval == 65535) // if not have eeprom data use default value
    {
        Serial.println("Load interval = " + String(interval) + ", interval not found in EEPROM, using default value");
        interval = 600;
    }
    Serial.println("interval: " + String(interval));

    state = (uint8_t)EEPROM.read(500);
    if (state == 255) // if not have eeprom data use default value
    {
        Serial.println("Load state = " + String(state) + ", state not found in EEPROM, using default value");
        state = 2;
    }
    Serial.println("state: " + String(state));

    pumpState = (uint8_t)EEPROM.read(497);
    if (pumpState == 255) // if not have eeprom data use default value
    {
        Serial.println("Load pumpState = " + String(pumpState) + ", pumpState not found in EEPROM, using default value");
        pumpState = 1;
    }
    Serial.println("pumpState: " + String(pumpState));

    ch1State = (uint8_t)EEPROM.read(496);
    if (ch1State == 255) // if not have eeprom data use default value
    {
        Serial.println("Load ch1State = " + String(ch1State) + ", ch1State not found in EEPROM, using default value");
        ch1State = 0;
    }
    Serial.println("ch1State: " + String(ch1State));

    ch2State = (uint8_t)EEPROM.read(495);
    if (ch2State == 255) // if not have eeprom data use default value
    {
        Serial.println("Load ch2State = " + String(ch2State) + ", ch2State not found in EEPROM, using default value");
        ch2State = 0;
    }
    Serial.println("ch2State: " + String(ch2State));

    ch3State = (uint8_t)EEPROM.read(494);
    if (ch3State == 255) // if not have eeprom data use default value
    {
        Serial.println("Load ch3State = " + String(ch3State) + ", ch3State not found in EEPROM, using default value");
        ch3State = 0;
    }
    Serial.println("ch3State: " + String(ch3State));

    ch4State = (uint8_t)EEPROM.read(493);
    if (ch4State == 255) // if not have eeprom data use default value
    {
        Serial.println("Load ch4State = " + String(ch4State) + ", ch4State not found in EEPROM, using default value");
        ch4State = 0;
    }
    Serial.println("ch4State: " + String(ch4State));

    EEPROM.end();

    iot.setEventCallback(handleEvent);

    const uint8_t version = 1; // เวอร์ชั่นโปรเจคนี้

// ตั้งค่าจำนวนตัวแปรตามโมเดลที่เลือก
#ifdef NOSENSOR_MODEL
    numVariables = 1;
    String keyname[numVariables] = {"on"};           // ชื่อตัวแปร
    iot.setTemplate("smart_farm_nosensor", version); // เลือกเทมเพลต
#elif defined(HUMID_MODEL)
    numVariables = 2;
    String keyname[numVariables] = {"on", "humid"}; // ชื่อตัวแปร
    iot.setTemplate("smart_farm_humid", version);   // เลือกเทมเพลต
#elif defined(TEMP_HUMID_MODEL)
    numVariables = 3;
    String keyname[numVariables] = {"on", "humid", "temp"}; // ชื่อตัวแปร
    iot.setTemplate("smart_farm_temp_humid", version);      // เลือกเทมเพลต
#elif defined(TEMP_HUMID_EC_MODEL)
    numVariables = 4;
    String keyname[numVariables] = {"on", "humid", "temp", "ec"}; // ชื่อตัวแปร
    iot.setTemplate("smart_farm_temp_humid_ec", version);         // เเลือกเทมเพลต
#else // ALL_7IN1_MODEL (default)
    numVariables = 8;
    String keyname[numVariables] = {"on", "humid", "temp", "ec", "ph", "n", "p", "k"}; // ชื่อตัวแปร
    iot.setTemplate("smart_farm_7in1", version);                                       // เลือกเทมเพลต
#endif

    iot.setkeyname(keyname, numVariables);

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

void setup()
{
    Serial.begin(115200);

#ifdef ESP8266
    RS485Serial.begin(4800, SWSERIAL_8N1, MAX485_RO, MAX485_DI); // software serial สำหรับติดต่อกับ MAX485
#elif defined(ESP32)
    RS485Serial.begin(4800, SERIAL_8N1, MAX485_RO, MAX485_DI); // serial สำหรับติดต่อกับ MAX485
#endif

    //------แสดงโลโก้เมื่อเริ่มต้น------
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C); // เริ่มต้นการทำงานของจอ OLED
    oled.clearDisplay();
    oled.drawBitmap(16, 5, logo_bmp, 33, 30, 1); // วาดโลโก้จากอาร์เรย์ข้างบน
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 40);
    oled.print("  CYNOIOT");
    oled.display();

    // สำหรับล้าง EEPROM จั้มกับ GND ค้างไว้ แล้วรีสตาร์ท
    pinMode(RSTPIN, INPUT_PULLUP);
    if (digitalRead(RSTPIN) == false)
    {
        delay(1000);
        if (digitalRead(RSTPIN) == false)
        {
            oled.clearDisplay();
            oled.setCursor(0, 0);
            oled.print("Clear All data\n rebooting");
            oled.display();
            delay(1000);
            clearEEPROM();
        }
    }

    // เพิ่มพารามิเตอร์อีเมลในหน้าตั้งค่า
    login.addItem(&emailParam);

    //  iotWebConf.setStatusPin(STATUS_PIN);
    // iotWebConf.setConfigPin(CONFIG_PIN);
    //  iotWebConf.addSystemParameter(&stringParam);
    iotWebConf.addParameterGroup(&login);
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setFormValidator(&formValidator);
    iotWebConf.getApTimeoutParameter()->visible = false;
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    // -- ตั้งค่าการจัดการ updateServer
    iotWebConf.setupUpdateServer(
        [](const char *updatePath)
        {
            httpUpdater.setup(&server, updatePath);
        },
        [](const char *userName, char *password)
        {
            httpUpdater.updateCredentials(userName, password);
        });

    // -- เริ่มต้นการตั้งค่า
    iotWebConf.init();

    // -- ตั้งค่า URL handlers บน web server
    server.on("/", handleRoot);
    server.on("/config", []
              { iotWebConf.handleConfig(); });
    server.on("/cleareeprom", clearEEPROM);
    server.on("/reboot", reboot);
    server.onNotFound([]()
                      { iotWebConf.handleNotFound(); });

    Serial.println("Ready.");

    node.preTransmission(preTransmission); // Callbacks allow us to configure the RS485 transceiver correctly
    node.postTransmission(postTransmission);
    node.begin(ADDRESS, RS485Serial);

    delay(1000);
    iotSetup();
}

void loop()
{
    iotWebConf.doLoop();   // ประมวลผลการทำงานของ IotWebConf
    server.handleClient(); // จัดการคำขอจาก web client
    iot.handle();          // จัดการการเชื่อมต่อกับ CynoIOT
#ifdef ESP8266
    MDNS.update(); // อัพเดท mDNS
#endif

    uint32_t currentMillis = millis();
    if (currentMillis - previousMillis >= 1000) /* for every x seconds, run the codes below*/
    {
        previousMillis = currentMillis; /* Set the starting point again for next counting time */
        sampleUpdate++;

        time1sec();
        onOffUpdate();
        display_update(); // อัพเดทจอ OLED
        outputControl();

        if (sampleUpdate >= 5)
        {
            readSensorData();

            sampleUpdate = 0;
        }
    }
}

void outputControl()
{
    displayStatus = "";

    if (pumpTimer)
    {
        digitalWrite(PUMP, HIGH);

        if (ch1Timer == 0 && ch2Timer == 0 && ch3Timer == 0 && ch4Timer == 0)
        {
            displayStatus = "P" + String(pumpTimer);
        }
        else
        {
            displayStatus = "P";
        }
        pumpTimer--;
    }
    else if (!ch1Timer || !ch2Timer || !ch3Timer || !ch4Timer)
    {
        digitalWrite(PUMP, LOW);
        displayStatus = "OFF";
    }

    if (ch1Timer)
    {
        digitalWrite(CH1, HIGH);
        displayStatus += "+C1:" + String(ch1Timer);
        ch1Timer--;
    }
    else if (ch2Timer)
    {
        digitalWrite(CH1, LOW);
        digitalWrite(CH2, HIGH);
        displayStatus += "+C2:" + String(ch2Timer);
        ch2Timer--;
    }
    else if (ch3Timer)
    {
        digitalWrite(CH2, LOW);
        digitalWrite(CH3, HIGH);
        displayStatus += "+C3:" + String(ch3Timer);
        ch3Timer--;
    }
    else if (ch4Timer)
    {
        digitalWrite(CH3, LOW);
        digitalWrite(CH4, HIGH);
        displayStatus += "+C4:" + String(ch4Timer);
        ch4Timer--;
    }
    else
    {
        digitalWrite(CH1, LOW);
        digitalWrite(CH2, LOW);
        digitalWrite(CH3, LOW);
        digitalWrite(CH4, LOW);
    }

    Serial.println(displayStatus);
}

void onOffUpdate()
{
    // Calculate onTimer: set to 1 if any of ch1-ch4 is on, otherwise 0
    bool thisState = (ch1Timer > 0 || ch2Timer > 0 || ch3Timer > 0 || ch4Timer > 0 || pumpTimer > 0) ? 1 : 0;

    if (thisState != onState)
    {
        iot.eventUpdate("Start", thisState); // อัพเดท event ไปยัง server
        onState = thisState;
    }
}
// ------------------------------------------------------------------
// Read sensor data based on the defined sensor model
// ------------------------------------------------------------------
void readSensorData()
{

// Determine which sensor model is defined and execute accordingly
#if defined(NOSENSOR_MODEL)
    // No sensors - only send the onState state
    float payload[numVariables] = {bool(onState)};
    iot.update(payload);
#elif defined(HUMID_MODEL)
    // Only humidity sensor
    uint8_t result = node.readHoldingRegisters(0x0000, 1); // Read only humidity register
    disConnect();
    if (result == node.ku8MBSuccess)
    {
        humidity = node.getResponseBuffer(0) / 10.0; // 0.1 %RH

        Serial.println("----- Soil Parameters -----");
        Serial.print("Humidity  : ");
        Serial.print(humidity);
        Serial.println(" %RH");

        float payload[numVariables] = {bool(onTimer), humidity};
        iot.update(payload);
    }
    else
    {
        Serial.println("Modbus error reading humidity!");
        iot.debug("error read humidity sensor");
    }
#elif defined(TEMP_HUMID_MODEL)
    // Temperature and humidity sensors
    uint8_t result = node.readHoldingRegisters(0x0000, 2); // Read humidity and temperature registers
    disConnect();
    if (result == node.ku8MBSuccess)
    {
        humidity = node.getResponseBuffer(0) / 10.0;    // 0.1 %RH
        temperature = node.getResponseBuffer(1) / 10.0; // 0.1 °C

        Serial.println("----- Soil Parameters -----");
        Serial.print("Humidity  : ");
        Serial.print(humidity);
        Serial.println(" %RH");
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.println(" °C");

        float payload[numVariables] = {bool(onTimer), humidity, temperature};
        iot.update(payload);
    }
    else
    {
        Serial.println("Modbus error reading temp/humidity!");
        iot.debug("error read temp/humidity sensor");
    }
#elif defined(TEMP_HUMID_EC_MODEL)
    // Temperature, humidity and EC (conductivity) sensors
    uint8_t result = node.readHoldingRegisters(0x0000, 3); // Read humidity, temperature, and conductivity registers
    disConnect();
    if (result == node.ku8MBSuccess)
    {
        humidity = node.getResponseBuffer(0) / 10.0;    // 0.1 %RH
        temperature = node.getResponseBuffer(1) / 10.0; // 0.1 °C
        conductivity = node.getResponseBuffer(2);       // µS/cm

        Serial.println("----- Soil Parameters -----");
        Serial.print("Humidity  : ");
        Serial.print(humidity);
        Serial.println(" %RH");
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.println(" °C");
        Serial.print("Conductivity: ");
        Serial.print(conductivity);
        Serial.println(" µS/cm");

        float payload[numVariables] = {bool(onTimer), humidity, temperature, conductivity};
        iot.update(payload);
    }
    else
    {
        Serial.println("Modbus error reading temp/humidity/EC!");
        iot.debug("error read temp/humidity/EC sensor");
    }
#elif defined(ALL_7IN1_MODEL)
    // All 7 sensors (humidity, temperature, EC, pH, N, P, K)
    uint8_t result = node.readHoldingRegisters(0x0000, 7);
    disConnect();
    if (result == node.ku8MBSuccess)
    {
        humidity = node.getResponseBuffer(0) / 10.0;    // 0.1 %RH
        temperature = node.getResponseBuffer(1) / 10.0; // 0.1 °C
        conductivity = node.getResponseBuffer(2);       // µS/cm
        ph = node.getResponseBuffer(3) / 10.0;          // 0.1 pH
        nitrogen = node.getResponseBuffer(4);           // mg/kg
        phosphorus = node.getResponseBuffer(5);         // mg/kg
        potassium = node.getResponseBuffer(6);          // mg/kg

        Serial.println("----- Soil Parameters -----");
        Serial.print("Humidity  : ");
        Serial.print(humidity);
        Serial.println(" %RH");
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.println(" °C");
        Serial.print("Conductivity: ");
        Serial.print(conductivity);
        Serial.println(" µS/cm");
        Serial.print("pH        : ");
        Serial.println(ph);
        Serial.print("Nitrogen  : ");
        Serial.print(nitrogen);
        Serial.println(" mg/kg");
        Serial.print("Phosphorus: ");
        Serial.print(phosphorus);
        Serial.println(" mg/kg");
        Serial.print("Potassium : ");
        Serial.print(potassium);
        Serial.println(" mg/kg");

        float payload[numVariables] = {bool(onTimer), humidity, temperature, conductivity, ph, nitrogen, phosphorus, potassium};
        iot.update(payload);
    }
    else
    {
        Serial.println("Modbus error!");
        iot.debug("error read sensor");
    }
#endif
}

void display_update()
{
    // แสดงสถานะการเชื่อมต่อ
    iotwebconf::NetworkState curr_state = iotWebConf.getState();
    if (curr_state == iotwebconf::Boot)
    {
        prev_state = curr_state;
    }
    else if (curr_state == iotwebconf::NotConfigured)
    {
        if (prev_state == iotwebconf::Boot)
        {
            displaytime = 5;
            prev_state = curr_state;
            noti = "-State-\n\nno config\nstay in\nAP Mode";
        }
    }
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
        else if (prev_state == iotwebconf::OnLine)
        {
            displaytime = 10;
            prev_state = curr_state;
            noti = "-State-\n\nX wifi\ndisconnect\ngo AP Mode";
        }
    }
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
    else if (curr_state == iotwebconf::OnLine)
    {
        if (prev_state == iotwebconf::Connecting)
        {
            displaytime = 5;
            prev_state = curr_state;
            noti = "-State-\n\nwifi\nconnect\nsuccess\n" + String(WiFi.RSSI()) + " dBm";
        }
    }

    // แสดงการแจ้งเตือนจาก CynoIOT
    if (iot.noti != "" && displaytime == 0)
    {
        displaytime = 3;
        noti = iot.noti;
        iot.noti = "";
    }

    // แสดงข้อความแจ้งเตือน
    if (displaytime)
    {
        displaytime--;
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print(noti);
        Serial.println(noti);
    }
    //------อัพเดทจอ OLED------
    else
    {

        // แสดงค่าจากเซ็นเซอร์ตามโมเดลที่เลือก
        oled.clearDisplay();
        oled.setTextSize(2);
        oled.setCursor(0, 15);

#if !defined(NOSENSOR_MODEL)
        oled.print(humidity, 0); // Humidity only
        oled.print(" %");
#endif

        // Title at top
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print("SmartFarm");

        oled.setCursor(0, 40);
        oled.print(displayStatus);
    }

    // แสดงไอคอนสถานการเชื่อมต่อ
    if (curr_state == iotwebconf::NotConfigured || curr_state == iotwebconf::ApMode)
        oled.drawBitmap(55, 0, wifi_ap, 9, 8, 1); // แสดงไอคอน AP Mode
    else if (curr_state == iotwebconf::Connecting)
    {
        if (t_connecting == 1)
        {
            oled.drawBitmap(56, 0, wifi_on, 8, 8, 1); // แสดงไอคอนกำลังเชื่อมต่อ
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
            oled.drawBitmap(56, 0, wifi_on, 8, 8, 1); // แสดงไอคอนเชื่อมต่อสำเร็จ
        }
        else
        {
            oled.drawBitmap(56, 0, wifi_nointernet, 8, 8,
                            1); // แสดงไอคอนไม่มีอินเทอร์เน็ต
        }
    }
    else if (curr_state == iotwebconf::OffLine)
        oled.drawBitmap(56, 0, wifi_off, 8, 8, 1); // แสดงไอคอนไม่ได้เชื่อมต่อ

    oled.display(); // แสดงผลบนจอ OLED
}

void preTransmission() /* transmission program when triggered*/
{
    pinMode(MAX485_RE, OUTPUT); /* Define RE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    pinMode(MAX485_DE, OUTPUT); /* Define DE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/

    digitalWrite(MAX485_RE, 1); /* put RE Pin to high*/
    digitalWrite(MAX485_DE, 1); /* put DE Pin to high*/
    delay(1);                   // When both RE and DE Pin are high, converter is allow to transmit communication
}

void postTransmission() /* Reception program when triggered*/
{

    delay(3);                   // When both RE and DE Pin are low, converter is allow to receive communication
    digitalWrite(MAX485_RE, 0); /* put RE Pin to low*/
    digitalWrite(MAX485_DE, 0); /* put DE Pin to low*/
}

float hexToFloat(uint32_t hex_value)
{
    union
    {
        uint32_t i;
        float f;
    } u;

    u.i = hex_value;
    return u.f;
}

void disConnect()
{
    pinMode(MAX485_RE, INPUT); /* Define RE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    pinMode(MAX485_DE, INPUT); /* Define DE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
}

// ฟังก์ชันจัดการหน้าเว็บหลัก
void handleRoot()
{
    // -- ให้ IotWebConf ทดสอบและจัดการคำขอ captive portal
    if (iotWebConf.handleCaptivePortal())
    {
        // -- คำขอ captive portal ถูกจัดการแล้ว
        return;
    }

    // สร้าง HTML จาก template และแทนที่ค่าต่างๆ
    String s = FPSTR(htmlTemplate);
    s.replace("%STATE%", String(iotWebConf.getState()));          // แทนที่สถานะ
    s.replace("%THING_NAME%", String(iotWebConf.getThingName())); // แทนที่ชื่อุปกรณ์
    s.replace("%EMAIL%", String(emailParamValue));                // แทนที่อีเมล
    s.replace("%SSID%", String(iotWebConf.getSSID()));            // แทนที่ SSID
    s.replace("%RSSI%", String(WiFi.RSSI()));                     // แทนที่ค่าความแรงสัญญาณ
    s.replace("%ESP_ID%", String(iot.getClientId()));             // แทนที่ ID ของ ESP
    s.replace("%VERSION%", String(IOTVERSION));                   // แทนที่เวอร์ชั่น

    server.send(200, "text/html", s); // ส่ง HTML กลับไปยัง client
}

// ฟังก์ชันที่ทำงานเมื่อบันทึกการตั้งค่า
void configSaved()
{
    Serial.println("Configuration was updated.");
}

// ฟังก์ชันที่ทำงานเมื่อเชื่อมต่อ WiFi สำเร็จ
void wifiConnected()
{

    Serial.println("WiFi was connected.");
    MDNS.begin(iotWebConf.getThingName());
    MDNS.addService("http", "tcp", 80);

    Serial.printf("Ready! Open http://%s.local in your browser\n", String(iotWebConf.getThingName()));
    if ((String)emailParamValue != "")
    {
        // เริ่มเชื่อมต่อ หลังจากต่อไวไฟได้
        Serial.println("login with " + (String)emailParamValue);
        iot.connect((String)emailParamValue); // เชื่อมต่อกับ CynoIOT ด้วยอีเมล
    }
}

// ฟังก์ชันตรวจสอบความถูกต้องของฟอร์ม
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper)
{
    Serial.println("Validating form.");
    bool valid = true;

    /*
        int l = webRequestWrapper->arg(stringParam.getId()).length();
        if (l < 3)
        {
          stringParam.errorMessage = "Please provide at least 3 characters for this
        test!"; valid = false;
        }
      */
    return valid;
}

// ฟังก์ชันล้างข้อมูลใน EEPROM
void clearEEPROM()
{
    Serial.println("clearEEPROM() called!");

    EEPROM.begin(512);

    // เขียนค่า 0 ลงใน EEPROM ทั้งหมด 512 ไบต์
    for (int i = 0; i < 512; i++)
    {
        EEPROM.write(i, 0);
    }

    EEPROM.end();
    server.send(200, "text/plain", "Clear all data\nrebooting");
    delay(1000);
    ESP.restart(); // รีสตาร์ท ESP
}

// ฟังก์ชันรีบูทอุปกรณ์
void reboot()
{
    server.send(200, "text/plain", "rebooting");
    delay(1000);
    ESP.restart(); // รีสตาร์ท ESP
}