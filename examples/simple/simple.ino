#include <ESP8266WiFi.h>
#include <cynoiot.h>

const char ssid[] = "G6PD";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

Cynoiot iot;

void iotSetup()
{
    uint8_t numVariables = 2;
    String keyname[numVariables];
    keyname[0] = "humid";
    keyname[1] = "temp";
    iot.setkeyname(keyname, numVariables);

    iot.connect(email);
}

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

    iotSetup();
}

void loop()
{
    iot.handle();

    if (!iot.status())
    {
        iot.connect(email);
    }
    delay(5000);
    float val[2] = {random(70, 80), random(20, 30)};
    iot.update(val);
}