/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

// เลือกรุ่นที่ใช้งาน
// #define NOSENSOR_MODEL
// #define HUMID_MODEL
// #define TEMP_HUMID_MODEL
// #define TEMP_HUMID_EC_MODEL
#define ALL_7IN1_MODEL

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
#define RSTPIN D8 // Define RSTPIN for ESP8266

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

const char thingName[] = "SoilSensor";            // ชื่ออุปกรณ์
const char wifiInitialApPassword[] = "iotbundle"; // รหัสผ่านเริ่มต้นสำหรับ AP Mode

#define ADDRESS 1

#define STRING_LEN 128 // ความยาวสูงสุดของสตริง
#define NUMBER_LEN 32  // ความยาวสูงสุดของตัวเลข

ModbusMaster node;
Cynoiot iot;

// กำหนดค่าสำหรับจอ OLED
#define OLED_RESET -1 // GPIO0
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
void showPopupMessage(String msg, uint8_t timeout = 3);
// -- ฟังก์ชัน Callback
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper);

unsigned long previousMillis = 0;

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
uint8_t t_connecting = 0;
uint8_t displaytime = 0;
iotwebconf::NetworkState prev_state = iotwebconf::Boot;
uint8_t sampleUpdate = 0;
String noti = "";
uint16_t timer_nointernet = 0;
uint8_t numVariables = 0;

float humidity = 0, temperature = 0, ph = 0;
uint32_t conductivity = 0, nitrogen = 0, phosphorus = 0, potassium = 0;

// EMA filter variables with 0.1 smoothing factor
float emaHumidity = 0, emaTemperature = 0, emaPh = 0;
float emaConductivity = 0, emaNitrogen = 0, emaPhosphorus = 0, emaPotassium = 0;
const float EMA_ALPHA = 0.1; // Smoothing factor for EMA filter
bool emaInitialized = false;
uint16_t interval = 600;
uint32_t pumpTimer = 0;
uint16_t chTimer[4] = {0, 0, 0, 0};
bool pumpState = false, chState[4] = {false, false, false, false};
bool humidityCutoffApplied = false;
String displayStatus = "";
String popupStatus = "";
uint8_t popupShowTimer = 0;

bool pumpUse = false;
uint8_t chUse = 0;
const uint8_t pumpDelayConst = 5, overlapValveConst = 5;
uint8_t pumpDelayTimer = 0;
uint8_t humidLowCutoff = 10, humidHighCutoff = 40;
uint32_t pumpOnProtectionTimer = 0;

enum GlobalMode : uint8_t
{
    OFF = 1, // ไม่ได้ทำงาน
    AUTO = 2 // ทำตามเซนเซอร์
};
GlobalMode globalMode = OFF;

enum WorkingMode : uint8_t
{
    NO_WORKING = 1, // ไม่ได้ทำงาน
    SEQUENCE = 2    // เรียงทีละโซน
};
WorkingMode workingMode = NO_WORKING;

// ฟังก์ชันสำหรับรับ event จากเซิร์ฟเวอร์
void handleEvent(String event, String value)
{
    EEPROM.begin(512);
    if (event == "SQ") // ตรวจสอบว่าเป็น event ชื่อ "SQ" Sequence หรือไม่
    {
        Serial.println("Sequence: " + value);
        // iot.debug("Event SQ received with value: " + value);
        if (value == "1" && workingMode == NO_WORKING)
        {
            startSequenceMode();
            iot.debug("Sequence mode started by event SQ");
        }
        else if (value == "0")
        {
            offSeq();
            iot.debug("Sequence mode stopped by event SQ");
        }
        else
        {
            iot.debug("Invalid value: " + value + "  workingmode: " + String(workingMode));
        }
    }
    else if (event == "M")
    {
        Serial.println("Mode: " + value);
        // iot.debug("Event M received with value: " + value);
        if (value == "auto")
        {
            globalMode = AUTO;
            showPopupMessage("Set to\n\nAuto\n\nMode");
            iot.debug("Mode set to Auto by event M");
        }
        else // if (value == "off" || value == "null")
        {
            globalMode = OFF;
            showPopupMessage("Set to\n\nOff\n\nMode");
            iot.debug("Mode set to Off by event M");
        }

        // Check if value is different before writing to EEPROM
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
    else if (event == "In")
    {
        Serial.println("Interval: " + value);
        // iot.debug("Event In received with value: " + value);
        showPopupMessage(String("Interval\n\nSet to\n\n") + value + String(" s"));
        interval = (uint16_t)value.toInt(); // แปลงค่า value เป็น int แล้วเก็บไว้ใน interval
        // iot.debug("Interval set to: " + String(interval) + " seconds");

        uint16_t currentValue = (uint16_t)EEPROM.read(498) | ((uint16_t)EEPROM.read(499) << 8);
        if (currentValue != interval)
        {
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
    else if (event == "P") // Pump
    {
        // iot.debug("Event P received with value: " + value);
        if (workingMode == NO_WORKING)
        {
            if (value == "1")
            {
                pumpState = 1;
                iot.eventUpdate("P", 1);
                showPopupMessage("Pump\n\nOn");
                iot.debug("Pump turned ON from event");
            }
            else if (value == "0")
            {
                pumpTimer = 0;
                pumpState = 0;
                iot.eventUpdate("P", 0);
                showPopupMessage("Pump\n\nOff");
                iot.debug("Pump turned OFF from event");
            }
        }

        else
        {
            iot.eventUpdate("P", digitalRead(PUMP));
            iot.debug("Pump in SEQUENCE mode, not change");
        }
    }

    else if (event == "c1") // Channel 1
    {
        // iot.debug("Event c1 received with value: " + value);
        if (workingMode == NO_WORKING)
        {
            if (chUse >= 1 && value.toInt())
            {
                chState[0] = 1;
                iot.eventUpdate("c1", 1);
                showPopupMessage("Ch1 On");
                iot.debug("Channel 1 turned ON from event");
            }
            else if (value.toInt() == 0)
            {
                chTimer[0] = 0;
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
    else if (event == "c2") // Channel 2
    {
        // iot.debug("Event c2 received with value: " + value);
        if (workingMode == NO_WORKING)
        {
            if (chUse >= 2 && value.toInt())
            {
                chState[1] = 1;
                iot.eventUpdate("c2", 1);
                showPopupMessage("Ch2 On");
                iot.debug("Channel 2 turned ON from event");
            }
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
    else if (event == "c3") // Channel 3
    {
        // iot.debug("Event c3 received with value: " + value);
        if (workingMode == NO_WORKING)
        {
            if (chUse >= 3 && value.toInt())
            {
                chState[2] = 1;
                iot.eventUpdate("c3", 1);
                showPopupMessage("Ch3 On");
                iot.debug("Channel 3 turned ON from event");
            }
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
    else if (event == "c4") // Channel 4
    {
        // iot.debug("Event c4 received with value: " + value);
        if (workingMode == NO_WORKING)
        {
            if (chUse >= 4 && value.toInt())
            {
                chState[3] = 1;
                iot.eventUpdate("c4", 1);
                showPopupMessage("Ch4 On");
                iot.debug("Channel 4 turned ON");
            }
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
    else if (event == "Pu")
    {
        Serial.println("Pump use : " + value);
        pumpUse = (bool)value.toInt();
        showPopupMessage(String("Pump\n\nUse\n\n") + String(value.toInt() != 0 ? "On" : "Off"));

        // Check if value is different before writing to EEPROM
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
    else if (event == "Cu")
    {
        Serial.println("CH use : " + value + "ch");
        // iot.debug("Event C1u received with value: " + value);
        chUse = value.toInt();
        showPopupMessage(String("Ch\n\nUse\n\n") + String(value.toInt()));
        // iot.debug("Channel use set to: " + String(ch1Use ? "On" : "Off"));

        // Check if value is different before writing to EEPROM
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
    else if (event == "Hl")
    {
        Serial.println("Humid low cutoff : " + value);
        // iot.debug("Event Hl received with value: " + value);
        humidLowCutoff = (uint8_t)value.toInt();
        showPopupMessage(String("Humid Low\n\nCutoff to\n\n") + value + String("%"));
        // iot.debug("Humidity low cutoff set to: " + String(humidLowCutoff) + "%");

        // Check if value is different before writing to EEPROM
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
    else if (event == "Hh")
    {
        Serial.println("Humid high cutoff : " + value);
        // iot.debug("Event Hh received with value: " + value);
        humidHighCutoff = (uint8_t)value.toInt();
        showPopupMessage(String("Humid High\n\nCutoff to\n\n") + value + String("%"));
        // iot.debug("Humidity high cutoff set to: " + String(humidHighCutoff) + "%");

        // Check if value is different before writing to EEPROM
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

    uint8_t eepromValue = EEPROM.read(500);
    if (eepromValue == 255) // if not have eeprom data use default value
    {
        Serial.println("Load globalMode = " + String(eepromValue) + ", globalMode not found in EEPROM, using default value");
        globalMode = OFF;
    }
    else
    {
        globalMode = static_cast<GlobalMode>(eepromValue);
    }
    Serial.println("globalMode: " + String(globalMode));

    pumpUse = (uint8_t)EEPROM.read(497);
    if (pumpUse == 255) // if not have eeprom data use default value
    {
        Serial.println("Load pumpUse = " + String(pumpUse) + ", pumpUse not found in EEPROM, using default value");
        pumpUse = 0;
    }
    Serial.println("pumpUse: " + String(pumpUse));

    chUse = (uint8_t)EEPROM.read(496);
    if (chUse == 255) // if not have eeprom data use default value
    {
        Serial.println("Load chUse = " + String(chUse) + ", chUse not found in EEPROM, using default value");
        chUse = 1;
    }
    Serial.println("chUse: " + String(chUse));

    humidLowCutoff = (uint8_t)EEPROM.read(489);
    if (humidLowCutoff == 255) // if not have eeprom data use default value
    {
        Serial.println("Load humidLowCutoff = " + String(humidLowCutoff) + ", humidLowCutoff not found in EEPROM, using default value = 40%");
        humidLowCutoff = 10;
    }
    Serial.println("humidLowCutoff: " + String(humidLowCutoff));

    humidHighCutoff = (uint8_t)EEPROM.read(488);
    if (humidHighCutoff == 255) // if not have eeprom data use default value
    {
        Serial.println("Load humidHighCutoff = " + String(humidHighCutoff) + ", humidHighCutoff not found in EEPROM, using default value = 60%");
        humidHighCutoff = 40;
    }
    Serial.println("humidHighCutoff: " + String(humidHighCutoff));

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

    // ตั้งค่าขา OUTPUT ทั้งหมดให้เป็น OUTPUT mode และเริ่มต้นที่ LOW
    digitalWrite(PUMP, LOW);
    digitalWrite(CH1, LOW);
    digitalWrite(CH2, LOW);
    digitalWrite(CH3, LOW);
    digitalWrite(CH4, LOW);

    pinMode(PUMP, OUTPUT);
    pinMode(CH1, OUTPUT);
    pinMode(CH2, OUTPUT);
    pinMode(CH3, OUTPUT);
    pinMode(CH4, OUTPUT);

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
        updateSystemState();
        updateDisplay(); // อัพเดทจอ OLED

        if (sampleUpdate >= 5)
        {
            readAndSendSensorData();
            sampleUpdate = 0;
        }
    }

    updateHardwareOutputs();

    // for protection pump on without valve on
    if (digitalRead(PUMP) && !chTimer[0] && !chTimer[1] && !chTimer[2] && !chTimer[3] && !digitalRead(CH1) && !digitalRead(CH2) && !digitalRead(CH3) && !digitalRead(CH4) && chUse)
    {
        chState[0] = 1;
    }
}

void updateSystemState()
{
    displayStatus = "";

#if !defined(NOSENSOR_MODEL) // ถ้ามีเซนเซอร์
    // Handle humidity cutoff before timer decrement logic
    if ((chTimer[0] || (pumpTimer && !chUse)) && workingMode == SEQUENCE) // if on and timer or auto mode
    {
        if (humidity >= humidHighCutoff && !humidityCutoffApplied) // ถ้าค่าความชื้นสูงกว่าเกณฑ์และยังไม่เคยถูกปรับ
        {
            // Debug output
            // iot.debug("HUMIDITY CUTOFF TRIGGERED: humidity=" + String(humidity) + ", cutoff=" + String(humidHighCutoff) + ", chTimer[0]=" + String(chTimer[0]));

            // Stop valve 1 (channel 1) immediately
            uint16_t valve1Time = interval - chTimer[0];
            chTimer[0] = overlapValveConst + 1;
            humidityCutoffApplied = true; // Mark that cutoff has been applied

            // iot.debug("AFTER CUTOFF: chTimer[0]=" + String(chTimer[0]) + ", valve1Time=" + String(valve1Time));

            // Set other valves to the same time as valve 1 had
            if (chUse >= 2 && chTimer[1] > 0)
                chTimer[1] = valve1Time;
            if (chUse >= 3 && chTimer[2] > 0)
                chTimer[2] = valve1Time;
            if (chUse >= 4 && chTimer[3] > 0)
                chTimer[3] = valve1Time;

            if (pumpUse)
            {
                int32_t pumpNewTimer = chTimer[0] + chTimer[1] + chTimer[2] + chTimer[3] - pumpDelayConst - 3;
                if (pumpNewTimer < 0)
                    pumpTimer = 0;
                else
                    pumpTimer = pumpNewTimer;
            }
        }
        else if (humidity < humidHighCutoff - 3)
        {
            humidityCutoffApplied = false; // Reset flag when humidity drops below cutoff
        }
    }
    else
    {
        pumpOnProtectionTimer = 0;
        humidityCutoffApplied = false; // Reset flag when not in sequence mode
    }
#endif

    if (workingMode == SEQUENCE)
    {
        // Handle pump delay countdown
        if (pumpDelayTimer)
        {
            pumpDelayTimer--;

            if (chTimer[0] == 0 && chTimer[1] == 0 && chTimer[2] == 0 && chTimer[3] == 0)
            {
                displayStatus = "D" + String(pumpDelayTimer);
            }
            else
            {
                displayStatus = "D+";
            }
        }
        // Normal pump operation after delay
        else if (pumpTimer && !pumpDelayTimer)
        {
            pumpState = 1;

            if (chTimer[0] == 0 && chTimer[1] == 0 && chTimer[2] == 0 && chTimer[3] == 0)
                displayStatus = "P" + String(pumpTimer);
            else
                displayStatus = "P+";

            pumpTimer--;
        }
        else if (!pumpTimer)
        {
            pumpState = 0;
            if (!pumpTimer && !chTimer[0] && !chTimer[1] && !chTimer[2] && !chTimer[3])
            {
                displayStatus = "OFF";
            }
        }

        if (chTimer[0])
        {
            chState[0] = 1;
            displayStatus += "C1:" + String(chTimer[0]);
            chTimer[0]--;

            // open next valve when overlapValveConst time
            if (chTimer[0] <= overlapValveConst && chTimer[1])
            {
                chState[1] = 1;
            }

            if (chTimer[0] == 0)
            {
                // iot.debug("set chTimer[0] to off");
                chState[0] = 0;
            }
            // iot.debug("chTimer[0] is " + String(chTimer[0]));
        }
        else if (chTimer[1])
        {
            chState[0] = 0;
            chState[1] = 1;
            displayStatus += "C2:" + String(chTimer[1]);
            chTimer[1]--;

            // open next valve when overlapValveConst time
            if (chTimer[1] <= overlapValveConst && chTimer[2])
            {
                chState[2] = 1;
            }

            if (chTimer[1] == 0)
                chState[1] = 0;
        }
        else if (chTimer[2])
        {
            chState[1] = 0;
            chState[2] = 1;
            displayStatus += "C3:" + String(chTimer[2]);
            chTimer[2]--;

            // open next valve when overlapValveConst time
            if (chTimer[2] <= overlapValveConst && chTimer[3])
            {
                chState[3] = 1;
            }

            if (chTimer[2] == 0)
                chState[2] = 0;
        }
        else if (chTimer[3])
        {
            chState[2] = 0;
            chState[3] = 1;
            displayStatus += "C4:" + String(chTimer[3]);
            chTimer[3]--;

            if (chTimer[3] == 0)
                chState[3] = 0;
        }

        if (!pumpTimer && !chTimer[0] && !chTimer[1] && !chTimer[2] && !chTimer[3])
        {
            workingMode = NO_WORKING;
            iot.eventUpdate("SQ", 0);
        }
    }

    // debug serial print
    Serial.println("---------------------------------------------------------");
    Serial.println("param\tch1\tch2\tch3\tch4\tpump");
    Serial.println("time \t" + String(chTimer[0]) + "\t" + String(chTimer[1]) + "\t" + String(chTimer[2]) + "\t" + String(chTimer[3]) + "\t" + String(pumpTimer));
    // iot.debug("time \t" + String(chTimer[0]) + "\t" + String(chTimer[1]) + "\t" + String(chTimer[2]) + "\t" + String(chTimer[3]) + "\t" + String(pumpTimer));
    Serial.println("state\t" + String(digitalRead(CH1)) + "\t" + String(digitalRead(CH2)) + "\t" + String(digitalRead(CH3)) + "\t" + String(digitalRead(CH4)) + "\t" + String(digitalRead(PUMP)));
    // iot.debug("state\t" + String(digitalRead(CH1)) + "\t" + String(digitalRead(CH2)) + "\t" + String(digitalRead(CH3)) + "\t" + String(digitalRead(CH4)) + "\t" + String(digitalRead(PUMP)));
    Serial.println("---------------------------------------------------------");
}

// ------------------------------------------------------------------
// Read sensor data based on the defined sensor model
// ------------------------------------------------------------------
// EMA filter function for smoothing sensor readings
float applyEmaFilter(float currentValue, float emaValue, bool firstReading = false)
{
    if (firstReading || !emaInitialized)
    {
        return round(currentValue * 10) / 10.0; // Initialize with first reading rounded to 1 decimal
    }
    float filteredValue = EMA_ALPHA * currentValue + (1.0 - EMA_ALPHA) * emaValue;
    return round(filteredValue * 10) / 10.0; // Round to 1 decimal place
}

// EMA filter function for integer sensor readings
float applyEmaFilterInt(uint32_t currentValue, float emaValue, bool firstReading = false)
{
    if (firstReading || !emaInitialized)
    {
        return round((float)currentValue * 10) / 10.0; // Initialize with first reading rounded to 1 decimal
    }
    float filteredValue = EMA_ALPHA * (float)currentValue + (1.0 - EMA_ALPHA) * emaValue;
    return round(filteredValue * 10) / 10.0; // Round to 1 decimal place
}

void readAndSendSensorData()
{
    uint32_t onState = getSystemState();
// Determine which sensor model is defined and execute accordingly
#if defined(NOSENSOR_MODEL)
    // No sensors - only send the onState state
    float payload[numVariables] = {onState};
    iot.update(payload);
#elif defined(HUMID_MODEL)
    // Only humidity sensor
    uint8_t result = node.readHoldingRegisters(0x0000, 1); // Read only humidity register
    disConnect();
    if (result == node.ku8MBSuccess)
    {
        float rawHumidity = node.getResponseBuffer(0) / 10.0; // 0.1 %RH
        humidity = applyEmaFilter(rawHumidity, emaHumidity, !emaInitialized);
        emaHumidity = humidity;
        emaInitialized = true;

        Serial.println("----- Soil Parameters -----");
        Serial.print("Humidity  : ");
        Serial.print(humidity, 1);
        Serial.print(" %RH (raw: ");
        Serial.print(rawHumidity);
        Serial.println(")");

        float payload[numVariables] = {onState, humidity};
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
        float rawHumidity = node.getResponseBuffer(0) / 10.0;    // 0.1 %RH
        float rawTemperature = node.getResponseBuffer(1) / 10.0; // 0.1 °C
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
        iot.debug("error read temp/humidity sensor");
    }
#elif defined(TEMP_HUMID_EC_MODEL)
    // Temperature, humidity and EC (conductivity) sensors
    uint8_t result = node.readHoldingRegisters(0x0000, 3); // Read humidity, temperature, and conductivity registers
    disConnect();
    if (result == node.ku8MBSuccess)
    {
        float rawHumidity = node.getResponseBuffer(0) / 10.0;    // 0.1 %RH
        float rawTemperature = node.getResponseBuffer(1) / 10.0; // 0.1 °C
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
        iot.debug("error read temp/humidity/EC sensor");
    }
#elif defined(ALL_7IN1_MODEL)
    // All 7 sensors (humidity, temperature, EC, pH, N, P, K)
    uint8_t result = node.readHoldingRegisters(0x0000, 7);
    disConnect();
    if (result == node.ku8MBSuccess)
    {
        float rawHumidity = node.getResponseBuffer(0) / 10.0;    // 0.1 %RH
        float rawTemperature = node.getResponseBuffer(1) / 10.0; // 0.1 °C
        uint32_t rawConductivity = node.getResponseBuffer(2);    // µS/cm
        float rawPh = node.getResponseBuffer(3) / 10.0;          // 0.1 pH
        uint32_t rawNitrogen = node.getResponseBuffer(4);        // mg/kg
        uint32_t rawPhosphorus = node.getResponseBuffer(5);      // mg/kg
        uint32_t rawPotassium = node.getResponseBuffer(6);       // mg/kg

        // Apply EMA filter to all sensor readings
        humidity = applyEmaFilter(rawHumidity, emaHumidity, !emaInitialized);
        temperature = applyEmaFilter(rawTemperature, emaTemperature, !emaInitialized);
        conductivity = (uint32_t)applyEmaFilterInt(rawConductivity, emaConductivity, !emaInitialized);
        ph = applyEmaFilter(rawPh, emaPh, !emaInitialized);
        nitrogen = (uint32_t)applyEmaFilterInt(rawNitrogen, emaNitrogen, !emaInitialized);
        phosphorus = (uint32_t)applyEmaFilterInt(rawPhosphorus, emaPhosphorus, !emaInitialized);
        potassium = (uint32_t)applyEmaFilterInt(rawPotassium, emaPotassium, !emaInitialized);

        // Update EMA variables
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

        float payload[numVariables] = {onState, humidity, temperature, conductivity, ph, nitrogen, phosphorus, potassium};
        iot.update(payload);
    }
    else
    {
        Serial.println("Modbus error!");
        iot.debug("error read sensor");
    }
#endif
}

void updateDisplay()
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
    // แสดงข้อความแจ้งเตือน
    else if (popupShowTimer)
    {
        popupShowTimer--;
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print(popupStatus);
        // Serial.println(popupStatus);
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
#else
        oled.setTextSize(1);
        if (globalMode == OFF)
            oled.print("Off mode");
        else if (globalMode == AUTO)
            oled.print("Auto mode");

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

void offSeq()
{
    workingMode = NO_WORKING;
    pumpState = 0;
    pumpTimer = 0;
    pumpDelayTimer = 0;
    for (uint8_t i = 0; i < 4; i++)
    {
        chState[i] = 0;
        chTimer[i] = 0;
    }
    showPopupMessage("Sequence\n\nStop");
}

// start sequence mode
void startSequenceMode()
{
    // iot.debug("START SEQUENCE MODE: humidity=" + String(humidity) + ", cutoff=" + String(humidHighCutoff));
    humidityCutoffApplied = false;
    workingMode = SEQUENCE;
    if (chUse)
    {
        for (uint8_t i = 0; i < chUse; i++)
        {
            chTimer[i] = interval;
        }
        // iot.debug("TIMERS SET: chTimer[0]=" + String(chTimer[0]) + ", interval=" + String(interval));
    }

    if (pumpUse)
    {
        pumpTimer = chUse * interval - pumpDelayConst - 3;

        // if use zone -> delay pump
        if (chUse)
        {
            pumpDelayTimer = pumpDelayConst;
            if (int32_t(chUse * interval - pumpDelayConst - 3) < 0)
                pumpTimer = 0;
            else
                pumpTimer = chUse * interval - pumpDelayConst - 3;
        }
        else
            pumpTimer = interval;
    }
    showPopupMessage("Sequence\n\nStart");
}

void showPopupMessage(String msg, uint8_t timeout)
{
    popupStatus = msg;
    popupShowTimer = timeout;
}

// set output from flag
void updateHardwareOutputs()
{
    if (digitalRead(PUMP) != pumpState)
    {
        // Serial.println("pump change to " + String(pumpState));
        digitalWrite(PUMP, pumpState);
        iot.eventUpdate("P", pumpState);
    }

    // Check and update each channel individually
    if (digitalRead(CH1) != chState[0])
    {
        // Serial.println("ch1 change to " + String(chState[0]));
        // iot.debug("ch1 change to " + String(chState[0]));
        digitalWrite(CH1, chState[0]);
        iot.eventUpdate("c1", chState[0]);
    }
    if (digitalRead(CH2) != chState[1])
    {
        // Serial.println("ch2 change to " + String(chState[1]));
        digitalWrite(CH2, chState[1]);
        iot.eventUpdate("c2", chState[1]);
    }
    if (digitalRead(CH3) != chState[2])
    {
        // Serial.println("ch3 change to " + String(chState[2]));
        digitalWrite(CH3, chState[2]);
        iot.eventUpdate("c3", chState[2]);
    }
    if (digitalRead(CH4) != chState[3])
    {
        // Serial.println("ch4 change to " + String(chState[3]));
        digitalWrite(CH4, chState[3]);
        iot.eventUpdate("c4", chState[3]);
    }
}

// read timer to show
uint32_t getSystemState()
{
    uint32_t onState = 0;
    if (chUse && (chTimer[0] > 0 || chTimer[1] > 0 || chTimer[2] > 0 || chTimer[3] > 0))
        onState = chTimer[0] + chTimer[1] + chTimer[2] + chTimer[3];

    else if (!chUse && pumpUse && pumpTimer > 0)
        onState = pumpTimer;

    else
        onState = pumpOnProtectionTimer;

    return onState;
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