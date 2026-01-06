/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
 // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>

// เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#elif defined(ESP32)
#include <WiFi.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>
#endif

#include <Wire.h>   // Wire library for I2C communication
#include <Ticker.h> // Ticker library for interrupt
#include <EEPROM.h>  // EEPROM library for storing data

#include <Adafruit_SSD1306.h>  // Adafruit SSD1306 library by Adafruit
#include <Adafruit_GFX.h>  // Adafruit GFX library by Adafruit
#include <cynoiot.h>    // CynoIOT by IoTbundle
#include <ModbusMaster.h>   // ModbusMaster by Doc Walker

// IoTWebconfrom https://github.com/canusorn/IotWebConf-iotbundle
#include <IotWebConf.h>
#include <IotWebConfUsing.h>

#ifdef ESP8266
SoftwareSerial RS485Serial;
#elif defined(ESP32)
#define RS485Serial Serial1
#endif

// ตั้งค่า pin สำหรับต่อกับ MAX485
#ifdef ESP8266
#define MAX485_RO D7 // RX
#define MAX485_RE D6
#define MAX485_DE D5
#define MAX485_DI D0 // TX

#elif defined(ESP32)
#define RSTPIN 7

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

#endif

ModbusMaster node;
Cynoiot iot;
#define OLED_RESET -1 // GPIO0
Adafruit_SSD1306 oled(OLED_RESET);

const char thingName[] = "dts6111";
const char wifiInitialApPassword[] = "iotbundle";
#define ADDRESS 1

#define STRING_LEN 128
#define NUMBER_LEN 32

// timer interrupt
Ticker timestamp;

unsigned long previousMillis;

DNSServer dnsServer;
WebServer server(80);

#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;

#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif

char emailParamValue[STRING_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword); // version defind in iotbundle.h file
// -- You can also use namespace formats e.g.: iotwebconf::TextParameter
IotWebConfParameterGroup login = IotWebConfParameterGroup("login", "ล็อกอิน(สมัครที่เว็บก่อนนะครับ)");

IotWebConfTextParameter emailParam = IotWebConfTextParameter("อีเมลล์ (ระวังห้ามใส่เว้นวรรค)", "emailParam", emailParamValue, STRING_LEN);

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
</body>
</html>
)rawliteral";

// -- Method declarations.
void handleRoot();
// -- Callback methods.
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper);

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
const uint8_t wifi_nointernet[] = {0x03, 0x7b, 0x87, 0x33, 0x4b, 0x00, 0x33, 0x33};                                         // 'wifi-off-1', 8x8px
uint8_t t_connecting;
iotwebconf::NetworkState prev_state = iotwebconf::Boot;
uint8_t displaytime;
String noti;
uint16_t timer_nointernet;
uint8_t numVariables;
uint8_t sampleUpdate, updateValue = 5;
float power[3];

void iotSetup()
{
    uint8_t numVariables = 16;
    String keyname[numVariables] = {
        "Va", "Vb", "Vc",
        "Ia", "Ib", "Ic",
        "Pa", "Pb", "Pc",
        "PFa", "PFb", "PFc", "f",
        "Et", "Ei", "Ee"};
    iot.setkeyname(keyname, numVariables);

    const uint8_t version = 1;           // เวอร์ชั่นโปรเจคนี้
    iot.setTemplate("dts6111", version); // เลือกเทมเพลตแดชบอร์ด

    Serial.println("ClinetID:" + String(iot.getClientId()));
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

#ifdef ESP8266
    RS485Serial.begin(9600, SWSERIAL_8N1, MAX485_RO, MAX485_DI); // software serial สำหรับติดต่อกับ MAX485
#elif defined(ESP32)
    RS485Serial.begin(9600, SERIAL_8N1, MAX485_RO, MAX485_DI); // serial สำหรับติดต่อกับ MAX485
#endif

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

    node.preTransmission(preTransmission); // Callbacks allow us to configure the RS485 transceiver correctly
    node.postTransmission(postTransmission);
    node.begin(ADDRESS, RS485Serial);

    // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/config", []
              { iotWebConf.handleConfig(); });
    server.on("/cleareeprom", clearEEPROM);
    server.on("/reboot", reboot);
    server.onNotFound([]()
                      { iotWebConf.handleNotFound(); });

    Serial.println("Ready.");

    iotSetup();
}

void loop()
{
    iotWebConf.doLoop();
    server.handleClient();
    iot.handle();
#ifdef ESP8266
    MDNS.update();
#endif

    // อ่านค่าจาก PZEM-017
    uint32_t currentMillisPZEM = millis();
    if (currentMillisPZEM - previousMillis >= 5000) /* for every x seconds, run the codes below*/
    {
        previousMillis = currentMillisPZEM; /* Set the starting point again for next counting time */
        readFromMeter();
        display_update();
    }
}

void readFromMeter()
{
    bool readValid = true;
    uint8_t result;
    float var[16];
    result = node.readInputRegisters(0x0000, 51);
    disConnect();
    if (result == node.ku8MBSuccess) /* If there is a response */
    {
        var[0] = node.getResponseBuffer(0) * 0.1;  // Va
        var[1] = node.getResponseBuffer(1) * 0.1;  // Vb
        var[2] = node.getResponseBuffer(2) * 0.1;  // Vc
        var[3] = node.getResponseBuffer(3) * 0.01; // Ia
        var[4] = node.getResponseBuffer(4) * 0.01; // Ib
        var[5] = node.getResponseBuffer(5) * 0.01; // Ic

        // power with signed
        int16_t p = int16_t(node.getResponseBuffer(8)); // Pa
        var[6] = p / 1000.0;                            // to kW
        p = int(node.getResponseBuffer(9));             // Pb
        var[7] = p / 1000.0;                            // to kW
        p = int(node.getResponseBuffer(10));            // Pc
        var[8] = p / 1000.0;                            // to kW

        power[0] = var[6];
        power[1] = var[7];
        power[2] = var[8];

        var[9] = node.getResponseBuffer(20) * 0.001;  // PFa
        var[10] = node.getResponseBuffer(21) * 0.001; // PFb
        var[11] = node.getResponseBuffer(22) * 0.001; // PFc

        var[12] = node.getResponseBuffer(26) * 0.01; // f

        var[13] = node.getResponseBuffer(30) / 100.0; // Et

        var[14] = node.getResponseBuffer(40) / 100.0; // Ei

        var[15] = node.getResponseBuffer(50) / 100.0; // Ee
    }
    else
    {
        // Detailed error reporting based on ModbusMaster error codes
        String errorMsg = "RS485 Error: ";
        switch (result) {
            case node.ku8MBIllegalFunction:
                errorMsg += "Illegal Function (0x01)";
                break;
            case node.ku8MBIllegalDataAddress:
                errorMsg += "Illegal Data Address (0x02)";
                break;
            case node.ku8MBIllegalDataValue:
                errorMsg += "Illegal Data Value (0x03)";
                break;
            case node.ku8MBSlaveDeviceFailure:
                errorMsg += "Slave Device Failure (0x04)";
                break;
            case node.ku8MBInvalidSlaveID:
                errorMsg += "Invalid Slave ID (0xE0)";
                break;
            case node.ku8MBInvalidFunction:
                errorMsg += "Invalid Function (0xE1)";
                break;
            case node.ku8MBResponseTimedOut:
                errorMsg += "Response Timeout (0xE2)";
                break;
            case node.ku8MBInvalidCRC:
                errorMsg += "Invalid CRC (0xE3)";
                break;
            default:
                errorMsg += "Unknown Error (0x" + String(result, HEX) + ")";
        }
        
        Serial.println(errorMsg);
        iot.debug(errorMsg);
        readValid = false;
        power[0] = NAN;
        power[1] = NAN;
        power[2] = NAN;
    }

    if (readValid)
    {
        Serial.println("Va: " + String(var[0]));
        Serial.println("Vb: " + String(var[1]));
        Serial.println("Vc: " + String(var[2]));
        Serial.println("Ia: " + String(var[3]));
        Serial.println("Ib: " + String(var[4]));
        Serial.println("Ic: " + String(var[5]));
        Serial.println("Pa: " + String(var[6]));
        Serial.println("Pb: " + String(var[7]));
        Serial.println("Pc: " + String(var[8]));
        Serial.println("PFa: " + String(var[9]));
        Serial.println("PFb: " + String(var[10]));
        Serial.println("PFc: " + String(var[11]));
        Serial.println("f: " + String(var[12]));
        Serial.println("Et: " + String(var[13]));
        Serial.println("Ei: " + String(var[14]));
        Serial.println("Ee: " + String(var[15]));
        Serial.println("----------------------");

        iot.update(var);
    }
}

// ---- ฟังก์ชันแสดงผลฝ่านจอ OLED ----
void display_update()
{
    //------Update OLED------
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.println("3P [kW]");

        oled.setCursor(0, 15);
        oled.print("Pa ");
        if(power[0]<10) oled.print(power[0],2);
        else oled.print(power[0],0);
        
        oled.setCursor(0, 26);
        oled.print("Pb ");
        if(power[1]<10) oled.print(power[1],2);
        else oled.print(power[1],0);
        
        oled.setCursor(0, 37);
        oled.print("Pc ");
        if(power[2]<10) oled.print(power[2],2);
        else oled.print(power[2],0);

    // on error
    if (isnan(power[0]))
    {
        oled.clearDisplay();
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