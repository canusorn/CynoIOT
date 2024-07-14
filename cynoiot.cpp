#include "cynoiot.h"

WiFiClient net;
MQTTClient client;

Cynoiot::Cynoiot()
{

#ifdef ESP8266

    String macAdd = WiFi.macAddress();
    String macAddressWithoutColon = macAdd.substring(0, 2) + macAdd.substring(3, 5) + macAdd.substring(6, 8) + macAdd.substring(9, 11) + macAdd.substring(12, 14) + macAdd.substring(15, 17);
    String S = "ESP8266-" + String(ESP.getChipId()) + "_" + macAddressWithoutColon;
    DEBUGLN("ESP8266 Chip ID: " + String(ESP.getChipId()));

#elif defined(ESP32)

    for (int i = 0; i < 41; i = i + 8)
    {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    DEBUGLN("ESP32 Chip model = " + String(ESP.getChipModel()) + "Rev " + String(ESP.getChipRevision()));
    DEBUGLN("Chip ID: ");
    DEBUGLN(chipId, HEX);
    String S = String(ESP.getChipModel()) + '-' + String(ESP.getChipRevision()) + "_" + String(chipId, HEX);

#else

    String S = "NEW-DEVICE" + String(random(100000, 999999));

#endif
    DEBUGLN("Client ID: " + S);
    this->_client_id = S;

}

void Cynoiot::connect(const char email[]){
    connect(email, "192.168.0.101");
}
void Cynoiot::connect(const char email[], const char server[])
{
    client.begin(server, net);
    client.onMessage(messageReceived);

    uint8_t ArrayLength = this->_client_id.length() + 1; // The +1 is for the 0x00h Terminator
    char ClientID[ArrayLength];
    this->_client_id.toCharArray(ClientID, ArrayLength);

    while (!client.connect(ClientID, email, this->_secret))
    {
        DEBUG(".");
        delay(1000);
    }

    DEBUGLN("\nconnected!");
    subscribe();
}

void Cynoiot::handle()
{
    client.loop();
}

void Cynoiot::subscribe()
{
    client.subscribe("/" + this->_client_id + "/#");
    DEBUGLN("subscripted!");
}

void Cynoiot::messageReceived(String &topic, String &payload)
{
    DEBUGLN("incoming: " + topic + " - " + payload);

    // Note: Do not use the client in the callback to publish, subscribe or
    // unsubscribe as it may cause deadlocks when other things arrive while
    // sending and receiving acknowledgments. Instead, change a global variable,
    // or push to a queue and handle it in the loop after calling `client.loop()`.
}

void Cynoiot::update()
{
    client.publish("/" + this->_client_id + "/data/update", "{\"humid\":" + String(random(70, 80)) + "," + "\"temp\":" + String(random(20, 30)) + "}");
}

bool Cynoiot::status()
{
    return client.connected();
}