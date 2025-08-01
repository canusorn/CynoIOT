/*
  -PZEM004T-
  5V - 5V
  GND - GND
  D3 - TX
  D4 - RX
*/

 // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>

#include <Wire.h>   // Wire library for I2C communication
#include <Ticker.h> // Ticker library for interrupt
#include <EEPROM.h>  // EEPROM library for storing data

#include <PZEM004Tv30.h>     // PZEM004Tv30 by Jakub
#include <Adafruit_SSD1306.h>  // Adafruit SSD1306 library by Adafruit
#include <Adafruit_GFX.h>  // Adafruit GFX library by Adafruit
#include <cynoiot.h>    // CynoIOT by IoTbundle

// IoTWebconfrom https://github.com/canusorn/IotWebConf-iotbundle
#include <IotWebConf.h>
#include <IotWebConfUsing.h>

// สร้าง object ชื่อ iot
Cynoiot iot;

const char thingName[] = "pzem004t";
const char wifiInitialApPassword[] = "iotbundle";

#define STRING_LEN 128
#define NUMBER_LEN 32

// timer interrupt
Ticker timestamp;

// Static HTML stored in flash memory
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

// -- Method declarations.
void handleRoot();
// -- Callback methods.
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper);

DNSServer dnsServer;
WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

char emailParamValue[STRING_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword); // version defind in iotbundle.h file
// -- You can also use namespace formats e.g.: iotwebconf::TextParameter
IotWebConfParameterGroup login = IotWebConfParameterGroup("login", "ล็อกอิน(สมัครที่เว็บก่อนนะครับ)");

IotWebConfTextParameter emailParam = IotWebConfTextParameter("อีเมลล์ (ระวังห้ามใส่เว้นวรรค)", "emailParam", emailParamValue, STRING_LEN);

#define OLED_RESET 0 // GPIO0
Adafruit_SSD1306 oled(OLED_RESET);
PZEM004Tv30 pzem(D3, D4); // rx,tx pin

unsigned long previousMillis = 0;
float voltage, current, power, energy, frequency, pf;

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
const uint8_t wifi_on[] = {0x00, 0x3c, 0x42, 0x99, 0x24, 0x00, 0x18, 0x18};                                                 // 'wifi-1', 8x8px
const uint8_t wifi_off[] = {0x01, 0x3e, 0x46, 0x99, 0x34, 0x20, 0x58, 0x98};                                                // 'wifi_nointernet-1', 8x8px
const uint8_t wifi_ap[] = {0x41, 0x00, 0x80, 0x80, 0xa2, 0x80, 0xaa, 0x80, 0xaa, 0x80, 0x88, 0x80, 0x49, 0x00, 0x08, 0x00}; // 'router-2', 9x8px
const uint8_t wifi_nointernet[] = {0x03, 0x7b, 0x87, 0x33, 0x4b, 0x00, 0x33, 0x33};
uint8_t t_connecting;
iotwebconf::NetworkState prev_state = iotwebconf::Boot;
uint8_t displaytime;
String noti;
uint16_t timer_nointernet;
uint8_t numVariables;
uint8_t sampleUpdate, updateValue = 5;

void iotSetup()
{
    // ตั้งค่าตัวแปรที่จะส่งขึ้นเว็บ
    numVariables = 6;                                               // จำนวนตัวแปร
    String keyname[numVariables] = {"v", "i", "p", "e", "f", "pf"}; // ชื่อตัวแปร
    iot.setkeyname(keyname, numVariables);

    const uint8_t version = 1;            // เวอร์ชั่นโปรเจคนี้
    iot.setTemplate("pzem004t", version); // เลือกเทมเพลตแดชบอร์ด
}

// timer interrupt every 1 second
void time1sec()
{

    // if can't connect to network
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

    // reconnect wifi if can't connect server
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

    // timer interrupt every 1 sec
    timestamp.attach(1, time1sec);

    //------Display LOGO at start------
    oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    oled.clearDisplay();
    oled.drawBitmap(16, 5, logo_bmp, 33, 30, 1); // call the drawBitmap function and pass it the array from above
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 40);
    oled.print("  CYNOIOT");
    oled.display();

    // for clear eeprom jump D5 to GND
    pinMode(D5, INPUT_PULLUP);
    if (digitalRead(D5) == false)
    {
        delay(1000);
        if (digitalRead(D5) == false)
        {
            oled.clearDisplay();
            oled.setCursor(0, 0);
            oled.print("Clear All data\n rebooting");
            oled.display();
            delay(1000);
            clearEEPROM();
        }
    }

    login.addItem(&emailParam);

    //  iotWebConf.setStatusPin(STATUS_PIN);
    // iotWebConf.setConfigPin(CONFIG_PIN);
    //  iotWebConf.addSystemParameter(&stringParam);
    iotWebConf.addParameterGroup(&login);
    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setFormValidator(&formValidator);
    iotWebConf.getApTimeoutParameter()->visible = false;
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    // -- Define how to handle updateServer calls.
    iotWebConf.setupUpdateServer(
        [](const char *updatePath)
        {
            httpUpdater.setup(&server, updatePath);
        },
        [](const char *userName, char *password)
        {
            httpUpdater.updateCredentials(userName, password);
        });

    // -- Initializing the configuration.
    iotWebConf.init();

    // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/config", []
              { iotWebConf.handleConfig(); });
    server.on("/cleareeprom", clearEEPROM);
    server.on("/reboot", reboot);
    server.onNotFound([]()
                      { iotWebConf.handleNotFound(); });

    Serial.println("ESPID: " + String(ESP.getChipId()));
    Serial.println("Ready.");

    //  pzem.resetEnergy(); //reset energy
    // Serial.println(ESP.getChipId());

    iotSetup();
}

void loop()
{
    iotWebConf.doLoop();
    server.handleClient();
    MDNS.update();

    // คอยจัดการการเชื่อมต่อ
    if ((String)emailParamValue != "")
    {
        iot.handle();
        if (!iot.status())
        {
            iot.connect((String)emailParamValue);
        }
    }

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 1000)
    { // run every 1 second
        previousMillis = currentMillis;
        displayValue(); // update OLED

        sampleUpdate++;
        if (sampleUpdate >= updateValue)
        {
            sampleUpdate = 0;

            if (isnan(voltage))
                return;

            //  อัพเดทค่าใหม่ในรูปแบบ array
            float val[numVariables] = {voltage, current, power, energy, frequency, pf};
            iot.update(val);
        }
    }
}

void displayValue()
{
    //------read data------
    voltage = pzem.voltage();
    if (!isnan(voltage))
    { // ถ้าอ่านค่าได้
        current = pzem.current();
        power = pzem.power();
        energy = pzem.energy();
        frequency = pzem.frequency();
        pf = pzem.pf();
    }
    else
    { // ถ้าอ่านค่าไม่ได้ให้ใส่ค่า NAN(not a number)
        current = NAN;
        power = NAN;
        energy = NAN;
        frequency = NAN;
        pf = NAN;
    }

    //------Update OLED display------
    oled.clearDisplay();
    oled.setTextSize(1);

    // display voltage
    oled.setCursor(3, 0);
    oled.print(voltage, 1);
    oled.setCursor(42, 0);
    oled.println("V");

    // display current
    if (current < 10)
        oled.setCursor(9, 12);
    else
        oled.setCursor(3, 12);
    oled.print(current, 2);
    oled.setCursor(42, 12);
    oled.println("A");

    // display power
    if (power < 10)
        oled.setCursor(26, 24);
    else if (power < 100)
        oled.setCursor(20, 24);
    else if (power < 1000)
        oled.setCursor(14, 24);
    else if (power < 10000)
        oled.setCursor(8, 24);
    else
        oled.setCursor(2, 24);
    oled.print(power, 0);
    oled.setCursor(42, 24);
    oled.println("W");

    // display energy
    oled.setCursor(3, 36);
    if (energy < 10)
        oled.print(energy, 3);
    else if (energy < 100)
        oled.print(energy, 2);
    else if (energy < 1000)
        oled.print(energy, 1);
    else
    {
        oled.setCursor(8, 36);
        oled.print(energy, 0);
    }
    oled.setCursor(42, 36);
    oled.println("kWh");

    // on error
    if (isnan(voltage))
    {
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.printf("-Sensor-\n\nno sensor\ndetect!");
    }

    // display status
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
            noti = "-State-\n\nX  wifi\ndisconnect\ngo AP Mode";
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

    if (iot.noti != "" && displaytime == 0)
    {
        displaytime = 3;
        noti = iot.noti;
        iot.noti = "";
    }

    if (displaytime)
    {
        displaytime--;
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.print(noti);
        Serial.println(noti);
    }

    // display state
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

    oled.display();

    //------Serial display------
    if (!isnan(voltage))
    {
        Serial.print("Voltage: ");
        Serial.print(voltage);
        Serial.println("V");
        Serial.print("Current: ");
        Serial.print(current);
        Serial.println("A");
        Serial.print("Power: ");
        Serial.print(power);
        Serial.println("W");
        Serial.print("Energy: ");
        Serial.print(energy, 3);
        Serial.println("kWh");
        Serial.print("Frequency: ");
        Serial.print(frequency, 1);
        Serial.println("Hz");
        Serial.print("PF: ");
        Serial.println(pf);
    }
    else
    {
        Serial.println("No sensor detect");
    }
}

void handleRoot()
{
    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal())
    {
        // -- Captive portal request were already served.
        return;
    }

    String s = FPSTR(htmlTemplate);
    s.replace("%STATE%", String(iotWebConf.getState()));          // Replace state placeholder
    s.replace("%THING_NAME%", String(iotWebConf.getThingName())); // Replace device name
    s.replace("%EMAIL%", String(emailParamValue));                // Replace email
    s.replace("%SSID%", String(iotWebConf.getSSID()));            // Replace SSID
    s.replace("%RSSI%", String(WiFi.RSSI()));                     // Replace RSSI
    s.replace("%ESP_ID%", String(iot.getClientId()));             // Replace ESP ID
    s.replace("%VERSION%", String(IOTVERSION));                   // Replace version

    server.send(200, "text/html", s);
}

void configSaved()
{
    Serial.println("Configuration was updated.");
}

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
        iot.connect((String)emailParamValue);
    }
}

bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper)
{
    Serial.println("Validating form.");
    bool valid = true;

    // if (l < 3)
    // {
    //   emailParam.errorMessage = "Please provide at least 3 characters for this test!";
    //   valid = false;
    // }

    return valid;
}

void clearEEPROM()
{
    EEPROM.begin(512);
    // write a 0 to all 512 bytes of the EEPROM
    for (int i = 0; i < 512; i++)
    {
        EEPROM.write(i, 0);
    }

    EEPROM.end();
    server.send(200, "text/plain", "Clear all data\nrebooting");
    delay(1000);
    ESP.restart();
}

void reboot()
{
    server.send(200, "text/plain", "rebooting");
    delay(1000);
    ESP.restart();
}