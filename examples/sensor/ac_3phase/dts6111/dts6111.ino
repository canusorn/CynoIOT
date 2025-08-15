/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

#ifdef ESP8266
#include <ESP8266WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266
#include <SoftwareSerial.h> // เรียกใช้ไลบรารี SoftwareSerial สำหรับบอร์ด ESP8266

#elif defined(ESP32)
#include <WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#endif

#include <cynoiot.h>    // CynoIOT by IoTbundle
#include <ModbusMaster.h>   // ModbusMaster by Doc Walker

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

const char ssid[] = "G6PD_2.4G";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

#define ADDRESS 1

unsigned long previousMillis;

void setup()
{
    Serial.begin(115200);

#ifdef ESP8266
    RS485Serial.begin(9600, SWSERIAL_8N1, MAX485_RO, MAX485_DI); // software serial สำหรับติดต่อกับ MAX485
#elif defined(ESP32)
    RS485Serial.begin(9600, SERIAL_8N1, MAX485_RO, MAX485_DI); // serial สำหรับติดต่อกับ MAX485
#endif

    Serial.println();
    Serial.print("Wifi connecting to ");
    Serial.print(ssid);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(1000);
    }
    Serial.print("\nWiFi connected, IP address: ");
    Serial.println(WiFi.localIP());

    pinMode(MAX485_RE, OUTPUT); /* Define RE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    pinMode(MAX485_DE, OUTPUT); /* Define DE Pin as Signal Output for RS485 converter. Output pin means Arduino command the pin signal to go high or low so that signal is received by the converter*/
    digitalWrite(MAX485_RE, 0); /* Arduino create output signal for pin RE as LOW (no output)*/
    digitalWrite(MAX485_DE, 0); /* Arduino create output signal for pin DE as LOW (no output)*/

    node.preTransmission(preTransmission); // Callbacks allow us to configure the RS485 transceiver correctly
    node.postTransmission(postTransmission);
    node.begin(ADDRESS, RS485Serial);

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

    Serial.print("Connecting to server.");
    iot.connect(email);
}

void loop()
{
    iot.handle();

    // อ่านค่าจาก PZEM-017
    uint32_t currentMillisPZEM = millis();
    if (currentMillisPZEM - previousMillis >= 5000) /* for every x seconds, run the codes below*/
    {
        previousMillis = currentMillisPZEM; /* Set the starting point again for next counting time */
        readFromMeter();
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
        int16_t power = int16_t(node.getResponseBuffer(8)); // Pa
        var[6] = power / 1000.0;                            // to kW
        power = int(node.getResponseBuffer(9));             // Pb
        var[7] = power / 1000.0;                            // to kW
        power = int(node.getResponseBuffer(10));            // Pc
        var[8] = power / 1000.0;                            // to kW

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
        Serial.println("error read sensor");
        iot.debug("error read sensor");
        readValid = false;
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