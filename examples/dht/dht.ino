#include <ESP8266WiFi.h>
#include <cynoiot.h>
#include <DHT.h>

#define DHTPIN D7
// Uncomment whatever type you're using!
#define DHTTYPE DHT11 // DHT 11
// #define DHTTYPE DHT22 // DHT 22  (AM2302), AM2321
//  #define DHTTYPE DHT21   // DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE);

const char ssid[] = "G6PD";
const char pass[] = "570610193";
// const char ssid[] = "Anusorn";
// const char pass[] = "0827705969";
const char email[] = "anusorn1998@gmail.com";

Cynoiot iot;

unsigned long previousMillis = 0;

void iotSetup()
{
    uint8_t numVariables = 2;
    String keyname[numVariables];
    keyname[0] = "humid";
    keyname[1] = "temp";
    iot.setkeyname(keyname, numVariables);

    Serial.print("Connecting to server.");
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

    dht.begin();
}

void loop()
{
    iot.handle();

    if (!iot.status())
    {
        iot.connect(email);
    }
delay(100);
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 2000)
    {
        previousMillis = currentMillis;

        float humid = dht.readHumidity();
        float temp = dht.readTemperature();

        Serial.print(F("Humidity: "));
        Serial.print(humid);
        Serial.print(F("% \t Temperature: "));
        Serial.print(temp);
        Serial.println(F("°C "));

        if (isnan(humid) || isnan(temp))
        {
            Serial.println(F("Failed to read from DHT sensor!"));
            return;
        }
        float val[2];
        val[0] = humid;
        val[1] = temp;
        iot.update(val);
    }
}