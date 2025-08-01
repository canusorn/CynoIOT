#ifdef ESP8266
#include <ESP8266WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP8266

#elif defined(ESP32)
#include <WiFi.h> // เรียกใช้ไลบรารี WiFi สำหรับบอร์ด ESP32
#endif

#include <cynoiot.h> // CynoIOT by IoTbundle

const char ssid[] = "G6PD";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

Cynoiot iot;

unsigned long previousMillis = 0;

void iotSetup()
{
    uint8_t numVariables = 2;
    String keyname[numVariables] = {"humid", "temp"};
    iot.setkeyname(keyname, numVariables);
    // iot.setTemplate("testtemplate", 1);
    iot.connect(email);
}

void setup()
{
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
        iot.debug("humid:" + String(val[0]) + " \ttemp" + String(val[1]));
        // iot.update(val);
    }
}