/*
   -PMS7003-
   5V - VCC
   GND - GND
   D4  - TX
   D3  - RX(not use in this code)
*/
// เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266
#ifdef ESP8266
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <SoftwareSerial.h> // SoftwareSerial for ESP8266

// เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#elif defined(ESP32)
#include <WiFi.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
#endif

#include <Ticker.h> // Ticker library for interrupt
#include <Wire.h>   // Wire library for I2C communication
#include <EEPROM.h> // EEPROM library for storing data

// IoTWebconfrom https://github.com/canusorn/IotWebConf-iotbundle
#include <IotWebConf.h>
#include <IotWebConfUsing.h>

#include <Adafruit_SSD1306.h> // Adafruit SSD1306 library by Adafruit
#include <Adafruit_GFX.h>     // Adafruit GFX library by Adafruit
#include <PMS.h>              // PMS Library by Mariusz
#include <cynoiot.h>          // CynoIOT by IoTbundle

// สร้าง object ชื่อ iot
Cynoiot iot;

const char thingName[] = "pms7003";               // ชื่ออุปกรณ์
const char wifiInitialApPassword[] = "iotbundle"; // รหัสผ่านเริ่มต้นสำหรับ AP Mode

#define STRING_LEN 128 // ความยาวสูงสุดของสตริง
#define NUMBER_LEN 32  // ความยาวสูงสุดของตัวเลข

// ตัวจับเวลาสำหรับการทำงานแบบ interrupt
Ticker timestamp;

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

// กำหนดขาที่ใช้เชื่อมต่อกับเซ็นเซอร์ PMS7003
#ifdef ESP8266
SoftwareSerial pmsSerial;
#define PURIFIER D8
#elif defined(ESP32)
#define pmsSerial Serial1
#define PURIFIER 12
#endif
PMS pms(pmsSerial);
PMS::DATA data;

// กำหนดค่าสำหรับจอ OLED
#define OLED_RESET 0 // GPIO0
Adafruit_SSD1306 oled(OLED_RESET);

#define RESET_PIN 7

// ตัวแปรเก็บเวลาล่าสุดที่อัพเดทข้อมูล
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
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xc0, 0x01, 0xf8, 0x00, 0x1f,
    0xff, 0xff, 0xfc, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x1f, 0xf8,
    0x0f, 0xfe, 0x00, 0x3f, 0xf0, 0x07, 0xfe, 0x00, 0x3f, 0xf0, 0x07, 0xfe, 0x00, 0x3f, 0xe0, 0x03,
    0xfe, 0x00, 0x3f, 0xc0, 0x01, 0xfe, 0x00, 0x3f, 0x80, 0x00, 0xfe, 0x00, 0x7f, 0x00, 0x00, 0x7f,
    0x00, 0x7f, 0x00, 0x00, 0x7f, 0x00, 0x7e, 0x00, 0x00, 0x3f, 0x00, 0x7e, 0x00, 0x00, 0x3f, 0x00,
    0x7e, 0x38, 0x0e, 0x3f, 0x00, 0x3e, 0x38, 0x0e, 0x3e, 0x00, 0x0e, 0x10, 0x04, 0x38, 0x00, 0x0e,
    0x00, 0x00, 0x38, 0x00, 0x0e, 0x00, 0x00, 0x38, 0x00, 0x0e, 0x03, 0x20, 0x38, 0x00, 0x0e, 0x07,
    0xf0, 0x38, 0x00, 0x0e, 0x03, 0xe0, 0x30, 0x00, 0x06, 0x01, 0xc0, 0x30, 0x00, 0x07, 0x01, 0xc0,
    0x70, 0x00, 0x03, 0xff, 0xff, 0xe0, 0x00, 0x01, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x7f, 0xff, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
// รูปไอคอนสถานการ WiFi ขนาด 8x8 พิกเซล
const uint8_t wifi_on[] = {0x00, 0x3c, 0x42, 0x99, 0x24, 0x00, 0x18, 0x18};                                                 // 'wifi-1', 8x8px
const uint8_t wifi_off[] = {0x01, 0x3e, 0x46, 0x99, 0x34, 0x20, 0x58, 0x98};                                                // 'wifi_nointernet-1', 8x8px
const uint8_t wifi_ap[] = {0x41, 0x00, 0x80, 0x80, 0xa2, 0x80, 0xaa, 0x80, 0xaa, 0x80, 0x88, 0x80, 0x49, 0x00, 0x08, 0x00}; // 'router-2', 9x8px
const uint8_t wifi_nointernet[] = {0x03, 0x7b, 0x87, 0x33, 0x4b, 0x00, 0x33, 0x33};

// ตัวแปรสำหรับการแสดงผลและการเชื่อมต่อ
uint8_t t_connecting;
iotwebconf::NetworkState prev_state = iotwebconf::Boot;
uint8_t displaytime;
String noti;
uint16_t timer_nointernet;
uint8_t numVariables;
uint8_t sampleUpdate, updateValue = 5, oledstate;
uint8_t sensorNotDetect = updateValue;
uint8_t purifierOnValue = 40;

// ฟังก์ชันสำหรับรับ event จากเซิร์ฟเวอร์
void handleEvent(String event, String value)
{
    if (event == "purifierOnValue") // ตรวจสอบว่าเป็น event ชื่อ "purifierOnValue" หรือไม่
    {
        purifierOnValue = (uint8_t)value.toInt(); // แปลงค่า value เป็น int แล้วเก็บไว้ใน purifierOnValue

        Serial.println("set purifierOnValue: " + String((uint8_t)value.toInt()));
        EEPROM.begin(512);
        EEPROM.write(499, purifierOnValue);
        EEPROM.commit();
    }
}

// ฟังก์ชันตั้งค่าการเชื่อมต่อกับ CynoIOT
void iotSetup()
{
    // read purifierOnValue from EEPROM
    EEPROM.begin(512);
    purifierOnValue = (uint8_t)EEPROM.read(499);
    if (purifierOnValue == 255) // if not have eeprom data use default value
    {
        purifierOnValue = 40;
    }

    iot.setEventCallback(handleEvent);

    // ตั้งค่าตัวแปรที่จะส่งขึ้นเว็บ
    numVariables = 4;                                              // จำนวนตัวแปร
    String keyname[numVariables] = {"pm1", "pm2", "pm10", "puri"}; // ชื่อตัวแปร
    iot.setkeyname(keyname, numVariables);

    const uint8_t version = 1; // เวอร์ชั่นโปรเจคนี้
    // iot.setTemplate("pms7003_airpurifier", version); // เลือกเทมเพลตแดชบอร์ด

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

// ฟังก์ชัน setup ทำงานครั้งแรกเมื่อเริ่มต้นโปรแกรม
void setup()
{
    Serial.begin(115200); // เริ่มการสื่อสารผ่าน Serial ที่ความเร็ว 115200 bps

    // เริ่มการสื่อสารกับเซ็นเซอร์ PMS7003 ที่ความเร็ว 9600 bps
#ifdef ESP8266
    pmsSerial.begin(9600, SWSERIAL_8N1, D4, D3); // software serial สำหรับติดต่อกับ MAX485
#elif defined(ESP32)
    pmsSerial.begin(9600, SERIAL_8N1, 16, 18); // serial สำหรับติดต่อกับ MAX485
#endif

    pinMode(PURIFIER, OUTPUT);
    digitalWrite(PURIFIER, LOW);

    //------แสดงโลโก้เมื่อเริ่มต้น------
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C); // เริ่มต้นการทำงานของจอ OLED
    oled.clearDisplay();
    oled.drawBitmap(16, 5, logo_bmp, 33, 30, 1); // วาดโลโก้จากอาร์เรย์ข้างบน
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 40);
    oled.print("  CYNOIOT");
    oled.display();

    // สำหรับล้าง EEPROM ให้กดปุ่ม D5 ค้างไว้
    pinMode(RESET_PIN, INPUT_PULLUP);
    if (digitalRead(RESET_PIN) == false)
    {
        delay(1000);
        if (digitalRead(RESET_PIN) == false)
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

    iotSetup(); // เรียกใช้ฟังก์ชันตั้งค่า CynoIOT
}

// ฟังก์ชัน loop ทำงานวนซ้ำตลอดเวลา
void loop()
{
    iotWebConf.doLoop();   // ประมวลผลการทำงานของ IotWebConf
    server.handleClient(); // จัดการคำขอจาก web client
    iot.handle();          // จัดการการเชื่อมต่อกับ CynoIOT
#ifdef ESP8266
    MDNS.update(); // อัพเดท mDNS
#endif

    //------อ่านข้อมูลจากเซ็นเซอร์ PMS7003------
    if (pms.read(data))
    {
        sensorNotDetect = 0; // รีเซ็ตตัวนับเมื่ออ่านข้อมูลได้

        display_update(); // อัพเดทจอ OLED

        // check purifier On Value
        if (digitalRead(PURIFIER) == LOW) // if off
        {
            if (data.PM_AE_UG_2_5 >= purifierOnValue && data.PM_AE_UG_2_5 > 5)
            {
                digitalWrite(PURIFIER, HIGH); // on purifier (active low)
            }
        }
        else // if on
        {
            if (data.PM_AE_UG_2_5 <= purifierOnValue - 10 || data.PM_AE_UG_2_5 <= 5)
            {
                digitalWrite(PURIFIER, LOW); // off purifier (active low)
            }
        }
    }

    // ทำงานทุก 1 วินาที
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 1000)
    {
        previousMillis = currentMillis;

        time1sec();

        if (sensorNotDetect < updateValue)
            sensorNotDetect++;
        else
        {
            display_update(); // update OLED
        }

        // ส่งข้อมูลขึ้นเซิร์ฟเวอร์ทุก updateValue วินาที
        sampleUpdate++;
        if (sampleUpdate >= updateValue && sensorNotDetect < updateValue)
        {
            sampleUpdate = 0;

            if (isnan(data.PM_AE_UG_1_0))
                return;

            //  อัพเดทค่าใหม่ในรูปแบบ array
            float val[numVariables] = {data.PM_AE_UG_1_0, data.PM_AE_UG_2_5, data.PM_AE_UG_10_0, digitalRead(PURIFIER) == HIGH ? 1 : 0};

            iot.update(val); // ส่งข้อมูลไปยังเซิร์ฟเวอร์
        }
    }
}

// ฟังก์ชันอัพเดทการแสดงผลบนจอ OLED
void display_update()
{
    oledstate++;
    if (oledstate > 10)
    {
        oledstate = 0;
    }

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
    else if (sensorNotDetect < updateValue)
    {
        if (oledstate <= 7)
        {
            // แสดงค่า PM จากเซ็นเซอร์ - PM2.5 เป็นตัวใหญ่ PM1.0 และ PM10.0 เป็นตัวเล็ก
            oled.clearDisplay();

            // PM2.5 as big text in the center
            oled.setTextSize(3);
            if (data.PM_AE_UG_2_5 < 9)
                oled.setCursor(20, 15);
            else if (data.PM_AE_UG_2_5 < 99)
                oled.setCursor(15, 15);
            else if (data.PM_AE_UG_2_5 < 999)
                oled.setCursor(10, 15);
            else
            {
                oled.setTextSize(2);
                oled.setCursor(0, 15);
            }

            oled.print(data.PM_AE_UG_2_5);

            // Title at top
            oled.setTextSize(1);
            oled.setCursor(14, 0);
            oled.print("PM2.5");

            oled.setCursor(18, 40);
            oled.print("ug/m3");
        }
        else
        {
            // แสดงค่า PM จากเซ็นเซอร์
            oled.clearDisplay();
            oled.setTextSize(1);
            oled.setCursor(0, 0);
            oled.println("PM(ug/m3)");
            oled.setCursor(0, 15);
            oled.print(" 1.0 : ");
            oled.print(data.PM_AE_UG_1_0);
            oled.setCursor(0, 26);
            oled.print(" 2.5 : ");
            oled.print(data.PM_AE_UG_2_5);
            oled.setCursor(0, 37);
            oled.print("10.0 : ");
            oled.print(data.PM_AE_UG_10_0);
        }
    }
    // แสดงข้อความเมื่อไม่พบเซ็นเซอร์
    else
    {
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.printf("-Sensor-\n\nno sensor\ndetect!");
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
            oled.drawBitmap(56, 0, wifi_nointernet, 8, 8, 1); // แสดงไอคอนไม่มีอินเทอร์เน็ต
        }
    }
    else if (curr_state == iotwebconf::OffLine)
        oled.drawBitmap(56, 0, wifi_off, 8, 8, 1); // แสดงไอคอนไม่ได้เชื่อมต่อ

    oled.display(); // แสดงผลบนจอ OLED

    //------แสดงข้อมูลบน Serial Monitor------
    if (sensorNotDetect < updateValue)
    {
        Serial.print("PM 1.0 (ug/m3): ");
        Serial.println(data.PM_AE_UG_1_0);
        Serial.print("PM 2.5 (ug/m3): ");
        Serial.println(data.PM_AE_UG_2_5);
        Serial.print("PM 10.0 (ug/m3): ");
        Serial.println(data.PM_AE_UG_10_0);
    }
    else
    {
        Serial.println("no sensor detect");
    }
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
        Serial.println("login");
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
        stringParam.errorMessage = "Please provide at least 3 characters for this test!";
        valid = false;
      }
    */
    return valid;
}

// ฟังก์ชันล้างข้อมูลใน EEPROM
void clearEEPROM()
{
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