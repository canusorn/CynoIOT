#include "cynoiot.h"

WiFiClientSecure net;
// WiFiClient net;
MQTTClient client(512);

Cynoiot cynoiotInstance;

uint32_t previousTime;
uint8_t pub2SubTime;

Cynoiot::Cynoiot()
{
}

bool Cynoiot::connect(String email)
{
    uint8_t ArrayLength = email.length() + 1; // The +1 is for the 0x00h Terminator
    char email_c[ArrayLength];
    email.toCharArray(email_c, ArrayLength);

    return connect(email_c, DEFAULT_SERVER);
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

    // client.begin(server, net);
    client.begin(server, PORT, net);
    client.onMessage(messageReceived);
    // client.setCleanSession(false);

    uint8_t ArrayLength = getClientId().length() + 1; // The +1 is for the 0x00h Terminator
    char ClientID[ArrayLength];
    getClientId().toCharArray(ClientID, ArrayLength);

    uint32_t currentMillis = millis();
    if (currentMillis - _lastReConnect > RECONNECT_SERVER_TIME || _lastReConnect == 0)
    {
        net.setInsecure();
        DEBUGLN("\nConnecting to " + String(server));
        this->_lastReConnect = currentMillis;
        client.connect(ClientID, email, this->_secret);
    }

    if (!status() && this->_connected)
    {
        DEBUGLN("Server disconnect ,reconnecting.");
        this->_connected = false;
        this->_Subscribed = false;
    }
    // else if (!status() && !this->_connected)
    // {
    // DEBUG(".");
    // }
    else if (status() && !this->_connected)
    {
        this->_connected = true;
        DEBUGLN("\nServer Connected!");
        this->_Subscribed = false;

        if (this->_template != "")
            templatePublish();
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

    // if subscriped flag
    if (!this->_Subscribed && status() && this->_connected)
    {
        this->_Subscribed = subscribe();
    }

    // run every 1 second
    uint32_t currentTime = millis();
    if (currentTime - previousTime >= 1000)
    {
        previousTime = currentTime;

        checkSubscription();
    }
}

bool Cynoiot::subscribe()
{
    bool isSubscribed = client.subscribe("/" + getClientId() + "/#");
    // client.subscribe("/ESP8266-2802776_ECFABC2AC458/#");
    if (isSubscribed)
    {
        DEBUGLN("subscripted! to /" + getClientId() + "/#");
        pub2SubTime = 0;
        return true;
    }
    else
    {
        DEBUGLN("not subscripted!");
        return false;
    }
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

        payload += "\"" + this->_var[i] + "\":" + String(val[i], 3);
    }
    payload += "}";

    DEBUGLN("Payload:" + payload);

    if (millis() - this->_lastPublish < MAX_PUBLISH_TIME - 100 && this->_lastPublish)
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

    pub2SubTime++;
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

#ifdef ESP8266
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
#endif

void Cynoiot::parsePinsString(const String &input)
{
    // Define the maximum number of elements
#ifdef ESP8266
    const int maxElements = 9;
#elif defined(ESP32)
    const int maxElements = 30;
#endif

    // Arrays to hold the parsed values
    String pins[maxElements];
    String modes[maxElements];
    String values[maxElements];

    int pinIndex = 0;
    int modeIndex = 0;
    int valueIndex = 0;

    // Split the input string by commas
    int startPos = 0;
    int endPos = input.indexOf(',');

    while (endPos != -1)
    {
        String segment = input.substring(startPos, endPos);
        int firstColon = segment.indexOf(':');
        int secondColon = segment.indexOf(':', firstColon + 1);

        if (firstColon != -1 && secondColon != -1)
        {
            // pins[pinIndex++] = segment.substring(0, firstColon);
            // modes[modeIndex++] = segment.substring(firstColon + 1, secondColon);
            // values[valueIndex++] = segment.substring(secondColon + 1);

            pinHandle(segment.substring(0, firstColon), segment.substring(firstColon + 1, secondColon), segment.substring(secondColon + 1));
        }

        startPos = endPos + 1;
        endPos = input.indexOf(',', startPos);
    }

    // Handle the last segment
    String segment = input.substring(startPos);
    int firstColon = segment.indexOf(':');
    int secondColon = segment.indexOf(':', firstColon + 1);

    if (firstColon != -1 && secondColon != -1)
    {
        // pins[pinIndex++] = segment.substring(0, firstColon);
        // modes[modeIndex++] = segment.substring(firstColon + 1, secondColon);
        // values[valueIndex++] = segment.substring(secondColon + 1);

        pinHandle(segment.substring(0, firstColon), segment.substring(firstColon + 1, secondColon), segment.substring(secondColon + 1));
    }
}

void Cynoiot::pinHandle(const String &pins, const String &modes, const String &values)
{
    DEBUGLN("saved control gpio : " + pins + " " + modes + " " + values);

    int pin;
#ifdef ESP8266
    uint8_t str_len = pins.length() + 1;
    char buff[str_len];
    pins.toCharArray(buff, str_len);

    if (buff[0] == 'D')
    {
        pin = getPinNumber(pins);
    }
    else
    {
        pin = pins.toInt();
    }
#else
    int pin = pins.toInt();
#endif

    // digit pwm dac get
    if (modes == "digit")
    {
        if (values == "high" || values == "on" || values == "1")
        {
            digitalWrite(pin, HIGH);
        }
        else
        {
            digitalWrite(pin, LOW);
        }
        pinMode(pin, OUTPUT);
    }
    else if (modes == "pwm")
    {
        analogWrite(pin, values.toInt());
    }
#ifdef ESP32
    else if (modes == "dac")
    {
        dacWrite(pin, value.toInt());
    }
#endif
}

void Cynoiot::messageReceived(String &topic, String &payload)
{
    String _topic = cynoiotInstance.getPublishTopic();
    String _clientid = cynoiotInstance.getClientId();

    if (topic == _topic)
    {
        pub2SubTime = 0;
        DEBUGLN("Done publishing");
        return;
    }
    else if (topic.startsWith("/" + _clientid + "/io"))
    {
        DEBUGLN("Control:  - " + payload);
        cynoiotInstance.parsePinsString(payload);
    }
    else if (topic.startsWith("/" + _clientid + "/init"))
    {
        DEBUGLN("Init:  - " + payload);
        cynoiotInstance.parsePinsString(payload);
    }
    else
    {
        DEBUGLN("incoming: " + topic + " - " + payload);
    }

    // Note: Do not use the client in the callback to publish, subscribe or
    // unsubscribe as it may cause deadlocks when other things arrive while
    // sending and receiving acknowledgments. Instead, change a global variable,
    // or push to a queue and handle it in the loop after calling `client.loop()`.
}

void Cynoiot::checkSubscription()
{
    // check received my publish message
    if (pub2SubTime)
    {
        pub2SubTime++;

        if (pub2SubTime >= this->_noSubTime - 1)
            DEBUGLN("pub to Sub Time:" + String(pub2SubTime) + " s");

        if (pub2SubTime >= this->_noSubTime)
        {
            this->_Subscribed = false;
            DEBUGLN("No Subscripe for : " + String(pub2SubTime) + " s");
            DEBUGLN("Resubscripe ....");
            pub2SubTime = 0;
        }
    }
}

void Cynoiot::setTemplate(String templateName)
{
    this->_template = templateName;
}

void Cynoiot::templatePublish()
{
    uint8_t ArrayLength = this->_template.length() + 1; // The +1 is for the 0x00h Terminator
    char payload_c[ArrayLength];
    this->_template.toCharArray(payload_c, ArrayLength);

    String topic = "/" + getClientId() + "/template/name";
    ArrayLength = topic.length() + 1; // The +1 is for the 0x00h Terminator
    char topic_c[ArrayLength];
    topic.toCharArray(topic_c, ArrayLength);

    client.publish(topic_c, payload_c);
}

void Cynoiot::interrupt1sec()
{
    // today timestamp update
    this->daytimestamp++;
}