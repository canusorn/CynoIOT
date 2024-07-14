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

void Cynoiot::connect(const char email[])
{
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

// void Cynoiot::update()
// {
//     client.publish("/" + this->_client_id + "/data/update", "{\"humid\":" + String(random(70, 80)) + "," + "\"temp\":" + String(random(20, 30)) + "}");
// }

bool Cynoiot::status()
{
    return client.connected();
}

void Cynoiot::setVar(const char v1[])
{
    setVar(v1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[])
{
    setVar(v1, v2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[])
{
    setVar(v1, v2, v3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[])
{
    setVar(v1, v2, v3, v4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[])
{
    setVar(v1, v2, v3, v4, v5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[])
{
    setVar(v1, v2, v3, v4, v5, v6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[])
{
    setVar(v1, v2, v3, v4, v5, v6, v7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[])
{
    setVar(v1, v2, v3, v4, v5, v6, v7, v8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[])
{
    setVar(v1, v2, v3, v4, v5, v6, v7, v8, v9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[])
{
    setVar(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[])
{
    setVar(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, 0x00, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[], const char v12[])
{
    setVar(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, 0x00, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[], const char v12[], const char v13[])
{
    setVar(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, 0x00, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[], const char v12[], const char v13[], const char v14[])
{
    setVar(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, 0x00, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[], const char v12[], const char v13[], const char v14[], const char v15[])
{
    setVar(v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, 0x00);
    this->varNum = 1;
}
void Cynoiot::setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[], const char v12[], const char v13[], const char v14[], const char v15[], const char v16[])
{
   this->varNum = 16;
    this->var[0] = v1;
    this->var[1] = v2;
    this->var[2] = v3;
    this->var[3] = v4;
    this->var[4] = v5;
    this->var[5] = v6;
    this->var[6] = v7;
    this->var[7] = v8;
    this->var[8] = v9;
    this->var[9] = v10;
    this->var[10] = v11;
    this->var[11] = v12;
    this->var[12] = v13;
    this->var[13] = v14;
    this->var[14] = v15;
    this->var[15] = v16;
}

void Cynoiot::update(float v1[]) {}
void Cynoiot::update(float v1[], float v2[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[], float v12[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[], float v12[], float v13[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[], float v12[], float v13[], float v14[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[], float v12[], float v13[], float v14[], float v15[]) {}
void Cynoiot::update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[], float v12[], float v13[], float v14[], float v15[], float v16[]) {
    for(uint8_t i = 0; i < this->varNum; i++) {
        
    }
}
