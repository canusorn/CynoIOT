#include <ESP8266WiFi.h>
#include <cynoiot.h>

const char ssid[] = "G6PD";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

Cynoiot iot;

void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(1000);
    }

    iot.setVar("humid","temp");
    iot.connect(email);
}

void loop()
{
    iot.handle();
    delay(5000);
    // iot.update();

    if (!iot.status())
    {
        iot.connect(email);
    }
}