#ifdef ESP8266
#include <ESP8266WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266

#elif defined(ESP32)
#include <WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#endif

#include <cynoiot.h> // CynoIOT by IoTbundle

#define LED D4

const char ssid[] = "G6PD_2.4G";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

Cynoiot iot;

unsigned long previousMillis = 0;
bool ledState = 0;

void iotSetup()
{
    uint8_t numVariables = 2;
    String keyname[numVariables] = {"humid", "temp"};
    iot.setkeyname(keyname, numVariables);
    
    iot.connect(email);
}

void setup()
{
    pinMode(LED, OUTPUT);

    Serial.begin(115200);
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

    Serial.print("Connecting to server.");
    iotSetup();
}

void loop()
{
    iot.handle();

    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 5000)
    {
        previousMillis = currentMillis;

        float val[2] = {random(70, 80), random(20, 30)};
        iot.update(val);

        ledState = !digitalRead(LED);          // สลับสถานะ LED
        digitalWrite(LED, ledState);           // เปิด/ปิด LED
        iot.gpioUpdate(LED, ledState);         // อัพเดท pin ไปยัง server
    }
}