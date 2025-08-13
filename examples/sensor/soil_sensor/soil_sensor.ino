/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

#ifdef ESP8266
#include <ESP8266WiFi.h>    // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266
#include <SoftwareSerial.h> // เรียกใช้ไลบรารี SoftwareSerial สำหรับบอร์ด ESP8266

#elif defined(ESP32)
#include <WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#endif

#include <cynoiot.h>      // CynoIOT by IoTbundle
#include <ModbusMaster.h> // ModbusMaster by Doc Walker

#ifdef ESP8266
SoftwareSerial RS485Serial;
#elif defined(ESP32)
#define RS485Serial Serial1
#endif

// ตั้งค่า pin สำหรับต่อกับ MAX485
#ifdef ESP8266
#define MAX485_RO D7 // RX
#define MAX485_RE D6
#define MAX485_DE D6
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

float humidity, temperature, ph;
uint32_t conductivity, nitrogen, phosphorus, potassium;

void setup()
{
    Serial.begin(115200);

#ifdef ESP8266
    RS485Serial.begin(4800, SWSERIAL_8N1, MAX485_RO, MAX485_DI); // software serial สำหรับติดต่อกับ MAX485
#elif defined(ESP32)
    RS485Serial.begin(4800, SERIAL_8N1, MAX485_RO, MAX485_DI); // serial สำหรับติดต่อกับ MAX485
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

    node.preTransmission(preTransmission); // Callbacks allow us to configure the RS485 transceiver correctly
    node.postTransmission(postTransmission);
    node.begin(ADDRESS, RS485Serial);

    uint8_t numVariables = 7;
    String keyname[numVariables] = {"humid", "temp", "ec", "ph", "n", "p", "k"};
    iot.setkeyname(keyname, numVariables);

    // const uint8_t version = 1;           // เวอร์ชั่นโปรเจคนี้
    // iot.setTemplate("soilsensor", version); // เลือกเทมเพลตแดชบอร์ด

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

        readSensorData();
    }
}

// ------------------------------------------------------------------
// Read all 7 registers (0x0000 … 0x0006)
// ------------------------------------------------------------------
void readSensorData()
{
    uint8_t result = node.readHoldingRegisters(0x0000, 7);
    disConnect();
    if (result == node.ku8MBSuccess)
    {
        humidity = node.getResponseBuffer(0) / 10.0;    // 0.1 %RH
        temperature = node.getResponseBuffer(1) / 10.0; // 0.1 °C
        conductivity = node.getResponseBuffer(2);     // µS/cm
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

        float payload[7] = {humidity, temperature, conductivity, ph, nitrogen, phosphorus, potassium};
        iot.update(payload);
    }
    else
    {
        Serial.println("Modbus error!");
        iot.debug("error read sensor");
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