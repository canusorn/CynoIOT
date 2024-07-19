#include "cynoiot.h"

WiFiClient net;
MQTTClient client;
Cynoiot cynoiotInstance;

Cynoiot::Cynoiot()
{
}

bool Cynoiot::connect(const char email[])
{
    return connect(email, DEFAULT_SERVER);
}
bool Cynoiot::connect(const char email[], const char server[])
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    client.begin(server, net);
    client.onMessage(messageReceived);

    uint8_t ArrayLength = getClientId().length() + 1; // The +1 is for the 0x00h Terminator
    char ClientID[ArrayLength];
    getClientId().toCharArray(ClientID, ArrayLength);

    uint32_t currentMillis = millis();

    if (currentMillis - _lastReConnect > RECONNECT_SERVER_TIME || _lastReConnect == 0)
    {
        _lastReConnect = currentMillis;
        client.connect(ClientID, email, this->_secret);
    }

    if (!status() && this->_connected)
    {
        DEBUGLN("Server disconnect ,reconnecting.");
        this->_connected = false;
    }
    // else if (!status() && !this->_connected)
    // {
    // DEBUG(".");
    // }
    else if (status() && !this->_connected)
    {
        this->_connected = true;
        DEBUGLN("\nServer Connected!");
        subscribe();
    }

#ifdef CYNOIOT_DEBUG
    uint32_t timetoconnect = millis() - currentMillis;
    if (timetoconnect > 1000)
    {
        DEBUGLN("Reconnecting time : " + String(timetoconnect) + " ms");
    }
#endif

    return this->_connected;
}

void Cynoiot::handle()
{
    client.loop();
}

void Cynoiot::subscribe()
{
    client.subscribe("/" + getClientId() + "/#");
    DEBUGLN("subscripted!");
}

void Cynoiot::messageReceived(String &topic, String &payload)
{
    String _topic = cynoiotInstance.getPublishTopic();
    String _clientid = cynoiotInstance.getClientId();

    if (topic == _topic)
        return;

    else if (topic.startsWith("/" + _clientid + "/"))
    {
        String pinStr = topic.substring(topic.lastIndexOf('/') + 1);
        // Serial.println(pinStr);
        int pin = cynoiotInstance.getPinNumber(pinStr);

        if (pin < 0)
            return;

        if (topic.startsWith("/" + _clientid + "/digit/"))
        {
            DEBUGLN("Control pin : " + String(pin) + " - " + payload);
            if (payload == "high" || payload == "1" || payload == "HIGH" || payload == "on")
            {
                digitalWrite(pin, HIGH);
            }
            else
            {
                digitalWrite(pin, LOW);
            }
            pinMode(pin, OUTPUT);
        }

        else if (topic.startsWith("/" + _clientid + "/pwm/"))
        {
        }
        else if (topic.startsWith("/" + _clientid + "/getStatus/"))
        {
        }
    }

    DEBUGLN("incoming: " + topic + " - " + payload);

    // Note: Do not use the client in the callback to publish, subscribe or
    // unsubscribe as it may cause deadlocks when other things arrive while
    // sending and receiving acknowledgments. Instead, change a global variable,
    // or push to a queue and handle it in the loop after calling `client.loop()`.
}

bool Cynoiot::status()
{
    return client.connected();
}

void Cynoiot::setkeyname(String keyname[], uint8_t numElements)
{

    this->_numElements = numElements;

    for (uint8_t i = 0; i < this->_numElements; i++)
    {
        this->_var[i] = keyname[i];
    }
}

void Cynoiot::update(float val[])
{
    String payload = "{";
    for (uint8_t i = 0; i < this->_numElements; i++)
    {
        if (i)
            payload += ",";

        payload += "\"" + this->_var[i] + "\":" + String(val[i]);
    }
    payload += "}";

    DEBUGLN("Payload:" + payload);

    if (millis() - this->_lastPublish < MAX_PUBLISH_TIME && this->_lastPublish)
    {
        Serial.println(F("Update too fast"));
        return;
    }
    this->_lastPublish = millis();
    publish(payload);
}

void Cynoiot::publish(String payload)
{
    publish(payload, getPublishTopic());
}
void Cynoiot::publish(String payload, String topic)
{
    uint8_t ArrayLength = payload.length() + 1; // The +1 is for the 0x00h Terminator
    char payload_c[ArrayLength];
    payload.toCharArray(payload_c, ArrayLength);

    ArrayLength = topic.length() + 1; // The +1 is for the 0x00h Terminator
    char topic_c[ArrayLength];
    topic.toCharArray(topic_c, ArrayLength);

    client.publish(topic_c, payload_c);
}

String Cynoiot::getPublishTopic()
{
    return String("/" + getClientId() + "/data/update");
}

String Cynoiot::getClientId()
{

#ifdef ESP8266

    String macAdd = WiFi.macAddress();
    String macAddressWithoutColon = macAdd.substring(0, 2) + macAdd.substring(3, 5) + macAdd.substring(6, 8) + macAdd.substring(9, 11) + macAdd.substring(12, 14) + macAdd.substring(15, 17);
    String S = "ESP8266-" + String(ESP.getChipId()) + "_" + macAddressWithoutColon;
    // DEBUGLN("ESP8266 Chip ID: " + String(ESP.getChipId()));

#elif defined(ESP32)

    for (int i = 0; i < 41; i = i + 8)
    {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    // DEBUGLN("ESP32 Chip model = " + String(ESP.getChipModel()) + "Rev " + String(ESP.getChipRevision()));
    // DEBUGLN("Chip ID: ");
    // DEBUGLN(chipId, HEX);
    String S = String(ESP.getChipModel()) + '-' + String(ESP.getChipRevision()) + "_" + String(chipId, HEX);

#else

    String S = "NEW-DEVICE" + String(random(100000, 999999));

#endif

    return S;
}

int Cynoiot::getPinNumber(String pinId)
{
    if (pinId == "D0")
        return D0; // GPIO 16
    if (pinId == "D1")
        return D1; // GPIO 5
    if (pinId == "D2")
        return D2; // GPIO 4
    if (pinId == "D3")
        return D3; // GPIO 0
    if (pinId == "D4")
        return D4; // GPIO 2
    if (pinId == "D5")
        return D5; // GPIO 14
    if (pinId == "D6")
        return D6; // GPIO 12
    if (pinId == "D7")
        return D7; // GPIO 13
    if (pinId == "D8")
        return D8; // GPIO 15
    return -1;     // Invalid pin identifier
}