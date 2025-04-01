#include "cynoiot.h"

WiFiClientSecure net;
// WiFiClient net;
#ifdef ESP8266
MQTTClient client(512);
#elif defined(ESP32)
MQTTClient client(1024);
#endif
// net = new WiFiClientSecure;
// client = new MQTTClient(512);

Cynoiot cynoiotInstance;

uint32_t previousTime;
uint8_t pub2SubTime;

String event, value;

String needOTA = "";

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
    if (_email.length() == 0)
    {
        _email = email;
    }

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

        // ota status checking
        EEPROM.begin(512);
        int retrievedValue = EEPROM.read(511);
        // Serial.println("EEPROM: " + String(retrievedValue));
        if (retrievedValue != 0)
        {
            if (retrievedValue == 1)
            {
                publish("OTA success", "/" + getClientId() + "/ota/status");
            }
            else if (retrievedValue == 2)
            {
                publish("OTA failed", "/" + getClientId() + "/ota/status");
            }

            EEPROM.write(511, 0); // reset status
            EEPROM.commit();
        }
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
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }

    client.loop();

    if (event.length() && value.length())
    {
        triggerEvent(event, value);
        event = "";
        value = "";
    }

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

    if (needOTA.length())
    {
        client.disconnect();
        updateOTA(needOTA);
        needOTA = "";
    }

    if (!status())
    {
        connect(_email);
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
    uint32_t chipId = 0;

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
    // Use vectors for dynamic memory allocation
    // std::vector<String> pins;
    // std::vector<String> modes;
    // std::vector<String> values;

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
    pin = pins.toInt();
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
    else if (modes == "DAC")
    {
        dacWrite(pin, values.toInt());
    }
#endif
}

void Cynoiot::messageReceived(String &topic, String &payload)
{
    String _topic = cynoiotInstance.getPublishTopic();
    String _clientid = cynoiotInstance.getClientId();
    pub2SubTime = 0;

    if (topic == _topic)
    {
        DEBUGLN("Done updating");
        return;
    }
    else if (topic.startsWith("/" + _clientid + "/io"))
    {
        DEBUGLN("Control: " + payload);
        cynoiotInstance.parsePinsString(payload);
    }
    // else if (topic.startsWith("/" + _clientid + "/init"))
    // {
    //     DEBUGLN("Init: " + payload);
    //     cynoiotInstance.parsePinsString(payload);
    // }
    else if (topic.startsWith("/" + _clientid + "/ota/start"))
    {
        // DEBUGLN("OTA update to : " + payload);
        needOTA = payload;
        // cynoiotInstance.updateOTA(payload);
    }
    else if (topic.startsWith("/" + _clientid + "/event"))
    {
        if (payload.startsWith("Event:"))
        {
            int firstColon = payload.indexOf(':');
            int secondColon = payload.indexOf(':', firstColon + 1);

            if (firstColon != -1 && secondColon != -1)
            {
                event = payload.substring(firstColon + 1, secondColon);
                value = payload.substring(secondColon + 1);
                // cynoiotInstance.triggerEvent(event, value); // Trigger the callback function with event and value
                return;
            }
        }
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
void Cynoiot::setTemplate(String templateName, uint8_t version)
{
    this->_template = templateName;
    this->_templateVersion = version;
}

void Cynoiot::templatePublish()
{
    String templateVersion = this->_template + "," + String(_templateVersion);
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

void Cynoiot::updateOTA(String otafile)
{
    DEBUGLN("updateOTA: " + otafile);

    EEPROM.begin(512);

#ifdef ESP8266
    ESPhttpUpdate.onStart([]()
                          { Serial.println("OTA: HTTP update process started"); });

    ESPhttpUpdate.onEnd([]()
                        { Serial.println("OTA: HTTP update process finished");
                        EEPROM.write(511, 1);  // 1 is update success
                    EEPROM.commit(); });

    ESPhttpUpdate.onProgress([](int cur, int total)
                             { Serial.printf("OTA: HTTP update process at %d of %d bytes...\n", cur, total); });

    ESPhttpUpdate.onError([](int err)
                          { Serial.printf("OTA: HTTP update fatal error code %d\n", err); 
                          EEPROM.write(511, 2);  // 2 is failed
                            EEPROM.commit(); });

    // WiFiClientSecure clientOTA;
    // clientOTA.setInsecure();
    // t_httpUpdate_return ret = ESPhttpUpdate.update(clientOTA, "https://cynoiot.com/api/ota/" + otafile + "/update");

    std::unique_ptr<BearSSL::WiFiClientSecure> clientOTA(new BearSSL::WiFiClientSecure);
    clientOTA->setInsecure(); // Disable certificate validation
    t_httpUpdate_return ret = ESPhttpUpdate.update(*clientOTA, "https://cynoiot.com/api/ota/" + otafile + "/update");

    // WiFiClient clientOTA;
    // t_httpUpdate_return ret = ESPhttpUpdate.update(clientOTA, "http://192.168.0.101:3000/api/ota/" + otafile + "/update");

#elif defined(ESP32)
    httpUpdate.onStart([]()
                       { Serial.println("OTA: HTTP update process started"); });

    httpUpdate.onEnd([]()
                     {  Serial.println("OTA: HTTP update process finished"); 
                        EEPROM.write(511, 1);  // 1 is update success
                        EEPROM.commit(); });

    httpUpdate.onProgress([](int cur, int total)
                          { Serial.printf("OTA: HTTP update process at %d of %d bytes...\n", cur, total); });

    httpUpdate.onError([](int err)
                       {    Serial.printf("OTA: HTTP update fatal error code %d\n", err); 
                            EEPROM.write(511, 2);  // 2 is failed
                            EEPROM.commit(); });

    NetworkClientSecure client;
    client.setInsecure();
    t_httpUpdate_return ret = httpUpdate.update(client, "https://cynoiot.com/api/ota/" + otafile + "/update");
#endif

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:

#ifdef ESP8266
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());

#elif defined(ESP32)
        Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
#endif
        break;

    case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

    case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }

    // reconnect if ota failed
    ESP.restart();
}

void Cynoiot::debug(String msg)
{
    publish(msg, "/" + getClientId() + "/debug");
}

// Add these methods to your implementation file

void Cynoiot::setEventCallback(EventCallbackFunction callback)
{
    _eventCallback = callback;
}

void Cynoiot::triggerEvent(String event, String value)
{
    if (_eventCallback != NULL)
    {
        _eventCallback(event, value);
    }
}

void Cynoiot::eventUpdate(String event, int value)
{
    String eventStr = "Event:" + event + ":" + String(value);
    String topic = "/" + getClientId() + "/event";
    publish(eventStr, topic);
}