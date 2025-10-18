#include "cynoiot.h"
#include <Ticker.h>

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
Ticker everySecond; // Add Ticker object for 1-second interval

uint32_t previousTime;
uint8_t pub2SubTime;

#ifdef ESP8266
const uint8_t bufferIO = 20*2;
#elif defined(ESP32)
const uint8_t bufferIO = 40*2;
#endif
String event[bufferIO], value[bufferIO], gpio[bufferIO];
String automationList;

String needOTA = "";
String lastMsgPublish = "";

#ifdef ESP8266
#define MAXTIMER 20
#elif defined(ESP32)
#define MAXTIMER 40
#endif
String timerStr, timerList[MAXTIMER];
// uint8_t _numTimer;

uint32_t weektimestamp;
uint16_t nexttimeupdate;

// global
uint32_t getDaytimestamps() { return weektimestamp % 86400; }
uint8_t getDayofWeek() { return (weektimestamp / 86400) % 7; }
uint8_t getHour() { return (weektimestamp % 86400) / 3600; }
uint8_t getMinute() { return ((weektimestamp % 86400) % 3600) / 60; }
uint8_t getSecond() { return ((weektimestamp % 86400) % 3600) % 60; }

void checkTimers() {

  // bool timerReadData = 1;

  if (timerStr.length()) {
    // Reset all timer list entries
    for (int i = 0; i < MAXTIMER; i++) {
      timerList[i] = "";
    }

    // Split timer string by commas
    int timerIndex = 0;
    int startPos = 0;
    int endPos = timerStr.indexOf(',');

    // Process all timer entries except the last one
    while (endPos != -1 && timerIndex < MAXTIMER) {
      timerList[timerIndex++] = timerStr.substring(startPos, endPos);
      startPos = endPos + 1;
      endPos = timerStr.indexOf(',', startPos);
    }

    // Process the last timer entry if there's space
    if (timerIndex < MAXTIMER && startPos < timerStr.length()) {
      timerList[timerIndex] = timerStr.substring(startPos);
    }

    // Clear the original timer string after processing
    timerStr = "";
    // _numTimer = 0;
  }

  // if (_numTimer == -1)
  //     return;

  uint8_t eventIndex, gpioIndex;

  for (uint _numTimer = 0; _numTimer < MAXTIMER; _numTimer++) {
    if (timerList[_numTimer].length()) {

      while (event[eventIndex].length() != 0) {
        eventIndex++;
      }

      while (gpio[gpioIndex].length() != 0) {
        gpioIndex++;
      }

      //  timestamp : repeat : action type : io/event : mode : value : days of
      //  week
      // Extract timer components using colons as delimiters
      int firstColon = timerList[_numTimer].indexOf(':');
      int secondColon = timerList[_numTimer].indexOf(':', firstColon + 1);
      int thirdColon = timerList[_numTimer].indexOf(':', secondColon + 1);
      int fourthColon = timerList[_numTimer].indexOf(':', thirdColon + 1);
      int fifthColon = timerList[_numTimer].indexOf(':', fourthColon + 1);
      int sixthColon = timerList[_numTimer].indexOf(':', fifthColon + 1);

      if (firstColon != -1 && secondColon != -1 && thirdColon != -1 &&
          fourthColon != -1 && fifthColon != -1) {

        String timestamp = timerList[_numTimer].substring(0, firstColon);
        String repeat =
            timerList[_numTimer].substring(firstColon + 1, secondColon);
        String actionType =
            timerList[_numTimer].substring(secondColon + 1, thirdColon);
        String target =
            timerList[_numTimer].substring(thirdColon + 1, fourthColon);
        String mode =
            timerList[_numTimer].substring(fourthColon + 1, fifthColon);
        String value =
            timerList[_numTimer].substring(fifthColon + 1, sixthColon);
        String daysOfWeek = timerList[_numTimer].substring(sixthColon + 1);

        // DEBUGLN("check timer " + timestamp + " compare to " +
        // String(getDaytimestamps())); Check if this timer matches current
        // timestamp
        if (timestamp.toInt() == getDaytimestamps()) {
          if (repeat == "w" && daysOfWeek.length()) {
            DEBUGLN("timer trig weekly");
            String thisdayOfWeek = String(cynoiotInstance.getDayofWeek());
            if (daysOfWeek.indexOf(thisdayOfWeek) != -1) {
              if (actionType == "g") {
                if (mode == "d") {
                  if (gpio[gpioIndex].length() == 0) {
                    gpio[gpioIndex] = String(target + "digit" + value);
                  }
                  // else
                  // {
                  //     timerReadData = 0;
                  // }
                } else if (mode == "p") {
                  if (gpio[gpioIndex].length() == 0) {
                    gpio[gpioIndex] = String(target + "pwm" + value);
                  }
                  // else
                  // {
                  //     timerReadData = 0;
                  // }
                }
#ifdef ESP32
                else if (mode == "a") {
                  if (gpio[gpioIndex].length() == 0) {
                    gpio[gpioIndex] = String(target + "DAC" + value);
                  }
                  // else
                  // {
                  //     timerReadData = 0;
                  // }
                }
#endif
              } else if (actionType == "e") {
                DEBUGLN("Timer(Weekly) trig Event " + target + " value " +
                        value);
                event[eventIndex] = target;
                ::value[eventIndex] = value;
              }
            }
          } else if (repeat == "e") {

            if (actionType == "g") {
              if (mode == "d") {
                DEBUGLN("Timer trig digital Pin " + target + " value " + value);
                if (gpio[gpioIndex].length() == 0) {
                  gpio[gpioIndex] = String(target + ":digit:" + value);
                }
                // else
                // {
                //     timerReadData = 0;
                // }
              } else if (mode == "p") {
                DEBUGLN("Timer trig pwm Pin " + target + " value " + value);
                if (gpio[gpioIndex].length() == 0) {
                  gpio[gpioIndex] = String(target + ":pwm:" + value);
                }
                // else
                // {
                //     timerReadData = 0;
                // }
              }
#ifdef ESP32
              else if (mode == "a") {
                DEBUGLN("Timer trig DAC Pin " + target + " value " + value);
                if (gpio[gpioIndex].length() == 0) {
                  gpio[gpioIndex] = String(target + ":DAC:" + value);
                }
                // else
                // {
                //     timerReadData = 0;
                // }
              }
#endif
            } else if (actionType == "e") {
              DEBUGLN("Timer trig Event " + target + " value " + value);
              event[eventIndex] = target;
              ::value[eventIndex] = value;
            }
          } else if (repeat == "o") {
            if (actionType == "g") {
              if (mode == "d") {
                DEBUGLN("Timer trig digital Pin " + target + " value " + value);
                if (gpio[gpioIndex].length() == 0) {
                  gpio[gpioIndex] = String(target + ":digit:" + value);
                }
                // else
                // {
                //     timerReadData = 0;
                // }
              } else if (mode == "p") {
                DEBUGLN("Timer trig pwm Pin " + target + " value " + value);
                if (gpio[gpioIndex].length() == 0) {
                  gpio[gpioIndex] = String(target + ":pwm:" + value);
                }
                // else
                // {
                //     timerReadData = 0;
                // }
              }
#ifdef ESP32
              else if (mode == "a") {
                DEBUGLN("Timer trig DAC Pin " + target + " value " + value);
                if (gpio[gpioIndex].length() == 0) {
                  gpio[gpioIndex] = String(target + ":DAC:" + value);
                }
                // else
                // {
                //     timerReadData = 0;
                // }
              }
#endif
            } else if (actionType == "e") {
              DEBUGLN("Timer trig Event " + target + " value " + value);
              event[eventIndex] = target;
              ::value[eventIndex] = value;
            }

            // if (timerReadData)
            // {
            // timerReadData = 0;
            timerList[_numTimer] = "";

            // shift timer list
            for (int i = _numTimer; i < MAXTIMER - 1; i++) {
              timerList[i] = timerList[i + 1];
            }
            // }
          }
        }
      }
    }
  }
  // else
  // {
  //     _numTimer = -1;
  // }

  // if (_numTimer >= MAXTIMER)
  //     _numTimer = -1;
  // else if (_numTimer >= 0 && timerReadData)
  //     _numTimer++;
}

// Callback function for the ticker
void everySecondCallback() {
  weektimestamp++;
  // _numTimer = 0; // timer ready to read

  if (weektimestamp >= 604801) {
    weektimestamp = 0;
  }

  checkTimers();
  // cynoiotInstance.printTimeDetails();
  // DEBUGLN("Timer now : " + String(weektimestamp));
}

Cynoiot::Cynoiot() {}

bool Cynoiot::connect(String email) {
  uint8_t ArrayLength =
      email.length() + 1; // The +1 is for the 0x00h Terminator
  char email_c[ArrayLength];
  email.toCharArray(email_c, ArrayLength);

  return connect(email_c, DEFAULT_SERVER);
}
bool Cynoiot::connect(const char email[]) {
  return connect(email, DEFAULT_SERVER);
}
bool Cynoiot::connect(const char email[], const char server[]) {
  if (_email.length() == 0) {
    _email = email;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  // client.begin(server, net);
  client.begin(server, PORT, net);
  client.onMessage(messageReceived);
  // client.setCleanSession(false);

  uint8_t ArrayLength =
      getClientId().length() + 1; // The +1 is for the 0x00h Terminator
  char ClientID[ArrayLength];
  getClientId().toCharArray(ClientID, ArrayLength);

  uint32_t currentMillis = millis();
  if (currentMillis - _lastReConnect > RECONNECT_SERVER_TIME ||
      _lastReConnect == 0) {
    net.setInsecure();
    DEBUGLN("\nConnecting to " + String(server));
    this->_lastReConnect = currentMillis;
    client.connect(ClientID, email, this->_secret);
  }

  if (!status() && this->_connected) {
    DEBUGLN("Server disconnect ,reconnecting.");
    this->_connected = false;
    this->_Subscribed = false;
  }
  // else if (!status() && !this->_connected)
  // {
  // DEBUG(".");
  // }
  else if (status() && !this->_connected) {
    this->_connected = true;
    DEBUGLN("\nServer Connected!");
    this->_Subscribed = false;

    // if (this->_template != "")
    templatePublish();

    // ota status checking
    EEPROM.begin(512);

    int retrievedValue = EEPROM.read(511);
    // Serial.println("EEPROM: " + String(retrievedValue));
    if (retrievedValue != 0) {
      if (retrievedValue == 1) {
        publish("OTA success", "/" + getClientId() + "/ota/status");
      } else if (retrievedValue == 2) {
        publish("OTA failed", "/" + getClientId() + "/ota/status");
      }

      EEPROM.write(511, 0); // reset status
      EEPROM.commit();
    }
    EEPROM.end();
  }

#ifdef CYNOIOT_DEBUG
  uint32_t timetoconnect = millis() - currentMillis;
  if (timetoconnect > 1000) {
    DEBUGLN("Reconnecting time : " + String(timetoconnect) + " ms");
  }
#endif

  return this->_connected;
}

void Cynoiot::handle() {

  for (uint8_t i = 0; i < bufferIO; i++) {
    if (event[i].length() && value[i].length()) {
      // DEBUGLN("Event flag found " + event[i] + " value " + value[i]);
      triggerEvent(event[i], value[i]);
      eventUpdate(event[i], value[i]);
      event[i] = "";
      value[i] = "";
    }

    if (gpio[i].length()) {
      parsePinsString(gpio[i]);

      gpio[i] = "";
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  client.loop();

  // run every 1 second
  uint32_t currentTime = millis();
  if (currentTime - previousTime >= 1000) {
    previousTime = currentTime;

    checkSubscription();
    checkUpdateTimestamps();

    handleTimestamp();
    checkAutomationTimeouts(); // Check for automation timeouts

    // printTimeDetails();
    // DEBUGLN((String)((weektimestamp % 86400) / 3600) + ":" +
    // (String)(((weektimestamp % 86400) % 3600) / 60) + ":" +
    // (String)(((weektimestamp % 86400) % 3600) % 60));
  }

  // if subscriped flag
  if (!this->_Subscribed && status() && this->_connected) {
    this->_Subscribed = subscribe();
  }

  if (needOTA.length()) {
    client.disconnect();
    updateOTA(needOTA);
    needOTA = "";
  }

  if (!status()) {
    connect(_email);
  }
}

void Cynoiot::checkUpdateTimestamps() {
  if (nexttimeupdate % 40 == 0 && nexttimeupdate <= 200) {
    DEBUGLN("request update timestamps");
    String payload = "";
    String topic = "/" + getClientId() + "/gettimestamps";
    publish(payload, topic);
  }
}

bool Cynoiot::subscribe() {
  bool isSubscribed = client.subscribe("/" + getClientId() + "/#");
  // client.subscribe("/ESP8266-2802776_ECFABC2AC458/#");
  if (isSubscribed) {
    DEBUGLN("subscripted! to /" + getClientId() + "/#");
    pub2SubTime = 0;
    return true;
  } else {
    DEBUGLN("not subscripted!");
    return false;
  }
}

bool Cynoiot::status() { return client.connected(); }

void Cynoiot::setkeyname(String keyname[], uint8_t numElements) {

  this->_numElements = numElements;

  for (uint8_t i = 0; i < this->_numElements; i++) {
    this->_var[i] = keyname[i];
  }
}

int getDecimalPlacesForDisplay(float value) {
  String s = String(value, 3); // Convert to string with high precision
  int dotIndex = s.indexOf('.');
  if (dotIndex == -1) {
    return 0; // No decimal point, it's an integer
  }

  int decimalCount = 0;
  // Iterate from the end of the string backwards to find the last non-zero
  // digit
  for (int i = s.length() - 1; i > dotIndex; i--) {
    if (s.charAt(i) != '0') {
      decimalCount = i - dotIndex;
      break;
    }
  }
  return decimalCount;
}

void Cynoiot::update(float val[]) {
  String payload = "{";
  for (uint8_t i = 0; i < this->_numElements; i++) {
    if (i)
      payload += ",";

    payload += "\"" + this->_var[i] +
               "\":" + String(val[i], getDecimalPlacesForDisplay(val[i]));

    handleAutomation(this->_var[i], val[i]);
  }
  payload += "}";

  DEBUGLN("Payload:" + payload);

  if (millis() - this->_lastPublish < MAX_PUBLISH_TIME - 100 &&
      this->_lastPublish) {
    Serial.println(F("Update too fast"));
    return;
  }
  this->_lastPublish = millis();
  publish(payload);
}

void Cynoiot::publish(String payload) { publish(payload, getPublishTopic()); }
void Cynoiot::publish(String payload, String topic) {
  uint8_t ArrayLength =
      payload.length() + 1; // The +1 is for the 0x00h Terminator
  char payload_c[ArrayLength];
  payload.toCharArray(payload_c, ArrayLength);

  ArrayLength = topic.length() + 1; // The +1 is for the 0x00h Terminator
  char topic_c[ArrayLength];
  topic.toCharArray(topic_c, ArrayLength);

  if (topic != getPublishTopic()) {
    lastMsgPublish = payload;
  }

  client.publish(topic_c, payload_c);

  pub2SubTime++;
}

String Cynoiot::getPublishTopic() {
  return String("/" + getClientId() + "/data/update");
}

String Cynoiot::getClientId() {

#ifdef ESP8266

  String macAdd = WiFi.macAddress();
  String macAddressWithoutColon =
      macAdd.substring(0, 2) + macAdd.substring(3, 5) + macAdd.substring(6, 8) +
      macAdd.substring(9, 11) + macAdd.substring(12, 14) +
      macAdd.substring(15, 17);
  String S =
      "ESP8266-" + String(ESP.getChipId()) + "_" + macAddressWithoutColon;
  // DEBUGLN("ESP8266 Chip ID: " + String(ESP.getChipId()));

#elif defined(ESP32)
  uint32_t chipId = 0;

  for (int i = 0; i < 41; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  // DEBUGLN("ESP32 Chip model = " + String(ESP.getChipModel()) + "Rev " +
  // String(ESP.getChipRevision())); DEBUGLN("Chip ID: "); DEBUGLN(chipId, HEX);
  String S = String(ESP.getChipModel()) + '-' + String(ESP.getChipRevision()) +
             "_" + String(chipId, HEX);

#else

  String S = "NEW-DEVICE" + String(random(100000, 999999));

#endif

  return S;
}

#ifdef ESP8266
int Cynoiot::getPinNumber(String pinId) {
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
  return -1;   // Invalid pin identifier
}

// Convert GPIO number to D pin format
String Cynoiot::getDpin(int pin) {
  String pinStr;
  switch (pin) {
  case 16:
    pinStr = "D0";
    break;
  case 5:
    pinStr = "D1";
    break;
  case 4:
    pinStr = "D2";
    break;
  case 0:
    pinStr = "D3";
    break;
  case 2:
    pinStr = "D4";
    break;
  case 14:
    pinStr = "D5";
    break;
  case 12:
    pinStr = "D6";
    break;
  case 13:
    pinStr = "D7";
    break;
  case 15:
    pinStr = "D8";
    break;
  default:
    pinStr = String(pin);
  }
  return pinStr;
}
#endif

void Cynoiot::parsePinsString(const String &input) {
  // Split the input string by commas
  int startPos = 0;
  int endPos = input.indexOf(',');

  while (endPos != -1) {
    String segment = input.substring(startPos, endPos);
    int firstColon = segment.indexOf(':');
    int secondColon = segment.indexOf(':', firstColon + 1);
    int thirdColon = segment.indexOf(':', secondColon + 1);

    if (firstColon != -1 && secondColon != -1) {
      if (segment.substring(0, firstColon) != "Event") {
        pinHandle(segment.substring(0, firstColon),
                  segment.substring(firstColon + 1, secondColon),
                  segment.substring(secondColon + 1, thirdColon));
#ifdef ESP8266
        gpioUpdate(getPinNumber(segment.substring(0, firstColon)),
                   segment.substring(secondColon + 1, thirdColon).toInt());
#else
        gpioUpdate(segment.substring(0, firstColon).toInt(),
                   segment.substring(secondColon + 1, thirdColon).toInt());
#endif
      } else if (segment.substring(0, firstColon) == "Event") {
        // DEBUGLN("Event run" + segment.substring(thirdColon + 1) + " " +
        // segment.substring(secondColon + 1, thirdColon));
        triggerEvent(segment.substring(thirdColon + 1),
                     segment.substring(secondColon + 1, thirdColon));
      }
    }

    startPos = endPos + 1;
    endPos = input.indexOf(',', startPos);
  }

  // Handle the last segment
  String segment = input.substring(startPos);
  int firstColon = segment.indexOf(':');
  int secondColon = segment.indexOf(':', firstColon + 1);
  int thirdColon = segment.indexOf(':', secondColon + 1);

  if (firstColon != -1 && secondColon != -1) {
    if (segment.substring(0, firstColon) != "Event") {
      pinHandle(segment.substring(0, firstColon),
                segment.substring(firstColon + 1, secondColon),
                segment.substring(secondColon + 1, thirdColon));
#ifdef ESP8266
      gpioUpdate(getPinNumber(segment.substring(0, firstColon)),
                 segment.substring(secondColon + 1, thirdColon).toInt());
#else
      gpioUpdate(segment.substring(0, firstColon).toInt(),
                 segment.substring(secondColon + 1, thirdColon).toInt());
#endif
    } else if (segment.substring(0, firstColon) == "Event") {
      // DEBUGLN("Event run" + segment.substring(thirdColon + 1) + " " +
      // segment.substring(secondColon + 1, thirdColon));
      triggerEvent(segment.substring(thirdColon + 1),
                   segment.substring(secondColon + 1, thirdColon));
    }
  }
}

void Cynoiot::pinHandle(const String &pins, const String &modes,
                        const String &values) {
  // DEBUGLN("Control gpio : " + pins);
  // DEBUGLN("pin int : " + String(pins.toInt()));

  int pin;
#ifdef ESP8266
  uint8_t str_len = pins.length() + 1;
  char buff[str_len];
  pins.toCharArray(buff, str_len);

  if (buff[0] == 'D') {
    pin = getPinNumber(pins);
  } else {
    pin = pins.toInt();
  }
#else
  pin = pins.toInt();
#endif

  // digit pwm dac get
  if (modes == "digit") {
    if (values == "high" || values == "on" || values == "1") {
      digitalWrite(pin, HIGH);
    } else {
      digitalWrite(pin, LOW);
    }
    pinMode(pin, OUTPUT);
  } else if (modes == "pwm") {
    analogWrite(pin, values.toInt());
  }
#if defined(ESP32) && !defined(ESP32S2) && !defined(ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C3)
  else if (modes == "DAC") {
    dacWrite(pin, values.toInt());
  }
#endif
}

void Cynoiot::messageReceived(String &topic, String &payload) {
  String _topic = cynoiotInstance.getPublishTopic();
  String _clientid = cynoiotInstance.getClientId();
  pub2SubTime = 0;

  // DEBUG("Received topic: " + topic);
  // DEBUGLN("\tReceived payload: " + payload);

  if (topic == _topic) {
    DEBUGLN("Done updating");
    return;
  }

  if (lastMsgPublish == payload) {
    lastMsgPublish = "";
    return;
  }

  if (topic == "/" + _clientid + "/io") {
    uint8_t gpioIndex;
    while (gpio[gpioIndex].length() != 0) {
      gpioIndex++;
    }

    // DEBUGLN("Control: " + payload);
    gpio[gpioIndex] = payload;
  }
  // else if (topic.startsWith("/" + _clientid + "/init"))
  // {
  //     DEBUGLN("Init: " + payload);
  //     cynoiotInstance.parsePinsString(payload);
  // }
  else if (topic.startsWith("/" + _clientid + "/ota/start")) {
    // DEBUGLN("OTA update to : " + payload);
    needOTA = payload;
    // cynoiotInstance.updateOTA(payload);
  } else if (topic.startsWith("/" + _clientid + "/timestamps")) {
    weektimestamp = payload.toInt();
    DEBUGLN("Timestamps: " + String(weektimestamp));

    // Detach any existing timer before attaching a new one to prevent multiple
    // callbacks
    everySecond.detach();
    // Initialize ticker to call everySecondCallback every 1 second
    everySecond.attach(1, everySecondCallback);

    nexttimeupdate = random(3600, 7200);

    // _numTimer = 0;
  } else if (topic == "/" + _clientid + "/event") {
    if (payload.startsWith("Event:")) {
      int firstColon = payload.indexOf(':');
      int secondColon = payload.indexOf(':', firstColon + 1);

      if (firstColon != -1 && secondColon != -1) {
        uint8_t eventIndex;
        while (event[eventIndex].length() != 0) {
          eventIndex++;
        }

        event[eventIndex] = payload.substring(firstColon + 1, secondColon);
        value[eventIndex] = payload.substring(secondColon + 1);
        // cynoiotInstance.triggerEvent(event, value); // Trigger the callback
        // function with event and value
        return;
      }
    }
  } else if (topic == "/" + _clientid + "/automation") {

    automationList = payload;
  } else if (topic.startsWith("/" + _clientid + "/timer")) {
    DEBUGLN("Update new timer : " + payload);
    timerStr = payload;
  } else {
    DEBUGLN("incoming: " + topic + " - " + payload);
  }

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}

void Cynoiot::checkSubscription() {
  // check received my publish message
  if (pub2SubTime) {
    pub2SubTime++;

    if (pub2SubTime >= this->_noSubTime - 1)
      DEBUGLN("pub to Sub Time:" + String(pub2SubTime) + " s");

    if (pub2SubTime >= this->_noSubTime) {
      this->_Subscribed = false;
      DEBUGLN("No Subscripe for : " + String(pub2SubTime) + " s");
      DEBUGLN("Resubscripe ....");
      pub2SubTime = 0;
    }
  }
}

void Cynoiot::setTemplate(String templateName) {
  this->_template = templateName;
}
void Cynoiot::setTemplate(String templateName, uint8_t version) {
  this->_template = templateName;
  this->_templateVersion = version;
}

void Cynoiot::templatePublish() {
  String topic = "/" + getClientId() + "/template/name";
  uint8_t ArrayLength =
      topic.length() + 1; // The +1 is for the 0x00h Terminator
  char topic_c[ArrayLength];
  topic.toCharArray(topic_c, ArrayLength);

  if (this->_template != "") {
    String templateVersion = this->_template + "," + String(_templateVersion);
    ArrayLength =
        templateVersion.length() + 1; // The +1 is for the 0x00h Terminator
    char payload_c[ArrayLength];
    templateVersion.toCharArray(payload_c, ArrayLength);
    client.publish(topic_c, payload_c);
  } else {
    const char payload[2] = "0";
    client.publish(topic_c, payload);
  }
}

void Cynoiot::updateOTA(String otafile) {
  DEBUGLN("updateOTA: " + otafile);

  EEPROM.begin(512);

#ifdef ESP8266
  ESPhttpUpdate.onStart([]() {
    Serial.println("OTA: HTTP update process started");
    everySecond.detach();
  });

  ESPhttpUpdate.onEnd([]() {
    Serial.println("OTA: HTTP update process finished");
    EEPROM.write(511, 1); // 1 is update success
    EEPROM.commit();
    EEPROM.end();
  });

  ESPhttpUpdate.onProgress([](int cur, int total) {
    Serial.printf("OTA: HTTP update process at %d of %d bytes...\n", cur,
                  total);
  });

  ESPhttpUpdate.onError([](int err) {
    Serial.printf("OTA: HTTP update fatal error code %d\n", err);
    EEPROM.write(511, 2); // 2 is failed
    EEPROM.commit();
    EEPROM.end();
  });

  // WiFiClientSecure clientOTA;
  // clientOTA.setInsecure();
  // t_httpUpdate_return ret = ESPhttpUpdate.update(clientOTA,
  // "https://cynoiot.com/api/ota/" + otafile + "/update");

  std::unique_ptr<BearSSL::WiFiClientSecure> clientOTA(
      new BearSSL::WiFiClientSecure);
  clientOTA->setInsecure(); // Disable certificate validation
  t_httpUpdate_return ret = ESPhttpUpdate.update(
      *clientOTA, "https://cynoiot.com/api/ota/" + otafile + "/update");

  // WiFiClient clientOTA;
  // t_httpUpdate_return ret = ESPhttpUpdate.update(clientOTA,
  // "http://192.168.0.101:3000/api/ota/" + otafile + "/update");

#elif defined(ESP32)
  httpUpdate.onStart([]() {
    Serial.println("OTA: HTTP update process started");
    everySecond.detach();
  });

  httpUpdate.onEnd([]() {
    Serial.println("OTA: HTTP update process finished");
    EEPROM.write(511, 1); // 1 is update success
    EEPROM.commit();
  });

  httpUpdate.onProgress([](int cur, int total) {
    Serial.printf("OTA: HTTP update process at %d of %d bytes...\n", cur,
                  total);
  });

  httpUpdate.onError([](int err) {
    Serial.printf("OTA: HTTP update fatal error code %d\n", err);
    EEPROM.write(511, 2); // 2 is failed
    EEPROM.commit();
  });

  NetworkClientSecure client;
  client.setInsecure();
  t_httpUpdate_return ret = httpUpdate.update(
      client, "https://cynoiot.com/api/ota/" + otafile + "/update");
#endif

  switch (ret) {
  case HTTP_UPDATE_FAILED:

#ifdef ESP8266
    Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n",
                  ESPhttpUpdate.getLastError(),
                  ESPhttpUpdate.getLastErrorString().c_str());

#elif defined(ESP32)
    Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
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

void Cynoiot::debug(String msg) {
  publish(msg, "/" + getClientId() + "/debug");
}

void Cynoiot::setEventCallback(EventCallbackFunction callback) {
  _eventCallback = callback;
}

void Cynoiot::triggerEvent(String event, String value) {
  if (_eventCallback != NULL) {
    _eventCallback(event, value);
  }
}

void Cynoiot::eventUpdate(String event, String value) {

  String eventStr = "Event:" + event + ":" + value;
  String topic = "/" + getClientId() + "/eventact";
  publish(eventStr, topic);
}

void Cynoiot::eventUpdate(String event, int value) {
  String eventStr = "Event:" + event + ":" + String(value);
  String topic = "/" + getClientId() + "/eventact";
  publish(eventStr, topic);
}

void Cynoiot::gpioUpdate(int pin, String value) {
  // DEBUGLN("gpioUpdate: " + String(pin) + " " + value);
  int value_int = value.toInt();
  if (value == "on" || value == "ON" || value == "HIGH" || value == "high" ||
      value == "1") {
    value_int = 1;
  } else if (value == "off" || value == "OFF" || value == "LOW" ||
             value == "low" || value == "0") {
    value_int = 0;
  }

  gpioUpdate(pin, value_int);
}

void Cynoiot::gpioUpdate(int pin, int value) {
#ifdef ESP8266
  String pinStr = getDpin(pin);
  String payload = pinStr + ":act:" + String(value);
#else
  String payload = String(pin) + ":act:" + String(value);
#endif

  String topic = "/" + getClientId() + "/ioact";
  publish(payload, topic);
}

void Cynoiot::gpioUpdate(String pin, int value) {
  String payload = pin + ":act:" + String(value);
  String topic = "/" + getClientId() + "/ioact";
  publish(payload, topic);
}

uint32_t Cynoiot::getTime() { return weektimestamp; }

void Cynoiot::printTimeDetails() {
  // Calculate days since Sunday (0-6)
  uint32_t daysSinceSunday = ::getDayofWeek();

  if (daysSinceSunday == 0) {
    return;
  }

  // Calculate hours, minutes, seconds
  uint8_t hours = ::getHour();
  uint8_t minutes = ::getMinute();
  uint8_t seconds = ::getSecond();

  // Array of day names
  const char *days[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                        "Thursday", "Friday", "Saturday"};

  // Print formatted time details
  Serial.print("Day: ");
  Serial.print(days[daysSinceSunday]);
  Serial.print(" | Time: ");
  if (hours < 10)
    Serial.print("0");
  Serial.print(hours);
  Serial.print(":");
  if (minutes < 10)
    Serial.print("0");
  Serial.print(minutes);
  Serial.print(":");
  if (seconds < 10)
    Serial.print("0");
  Serial.println(seconds);
}

// in cynoiot class
uint32_t Cynoiot::getDaytimestamps() { return ::getDaytimestamps(); }
uint8_t Cynoiot::getDayofWeek() { return ::getDayofWeek(); }
uint8_t Cynoiot::getHour() { return ::getHour(); }
uint8_t Cynoiot::getMinute() { return ::getMinute(); }
uint8_t Cynoiot::getSecond() { return ::getSecond(); }

void Cynoiot::handleTimestamp() {
  // handle update timestamp
  if (nexttimeupdate) {
    nexttimeupdate--;
  } else {
    nexttimeupdate = random(3600, 7200);
  }
}

// Structure to track time-limited automation actions
struct AutomationTimeout {
  String actionType;
  String target;
  String mode;
  String timeoutValue;
  unsigned long startTime;
  unsigned long timeoutDuration;
  bool active;
};

// Array to track active time-limited automations
static AutomationTimeout timeoutTracking[10];
static int timeoutCount = 0;

void Cynoiot::handleAutomation(String variable, float value) {
  // Format: event,gpio : var : operator : value_th : event_name,gpio : mode :
  // value [: timeLimit] [: timeLimit value]
  // Components: [0]actionType
  // [1]variable [2]operator [3]threshold [4]target [5]mode [6]value
  // [7]timeLimit [8]timeoutValue

  if (automationList.length() == 0) {
    return;
  }

  // Process each automation rule separated by commas
  int startPos = 0;
  int endPos = automationList.indexOf(',');

  while (endPos != -1 || startPos < automationList.length()) {
    String rule;
    if (endPos != -1) {
      rule = automationList.substring(startPos, endPos);
      startPos = endPos + 1;
      endPos = automationList.indexOf(',', startPos);
    } else {
      rule = automationList.substring(startPos);
      startPos = automationList.length();
    }

    if (rule.length() == 0) {
      continue;
    }

    // Parse rule components
    String components[9];
    int componentCount = 0;
    int colonPos = 0;
    int lastPos = 0;

    // Split by colons
    while (colonPos != -1 && componentCount < 9) {
      colonPos = rule.indexOf(':', lastPos);
      if (colonPos != -1) {
        components[componentCount] = rule.substring(lastPos, colonPos);
        lastPos = colonPos + 1;
      } else {
        components[componentCount] = rule.substring(lastPos);
      }
      componentCount++;
    }

    // Minimum 7 components required (actionType, variable, operator, threshold,
    // target, mode, value)
    if (componentCount < 7) {
      DEBUGLN("Automation rule invalid - insufficient components: " + rule);
      continue;
    }

    String actionType = components[0];
    actionType.trim();
    String ruleVariable = components[1];
    ruleVariable.trim();
    String operatorStr = components[2];
    operatorStr.trim();
    String thresholdStr = components[3];
    thresholdStr.trim();
    float threshold = thresholdStr.toFloat();
    String target = components[4];
    target.trim();
    String mode = components[5];
    mode.trim();
    String actionValue = components[6];
    actionValue.trim();
    String timeLimit = (componentCount > 7) ? components[7] : "";
    timeLimit.trim();
    String timeoutValue = (componentCount > 8) ? components[8] : "";
    timeoutValue.trim();

    // Check if this rule applies to the current variable
    if (ruleVariable != variable) {
      continue;
    }

    // Evaluate condition
    bool conditionMet = false;
    if (operatorStr == ">") {
      conditionMet = (value > threshold);
    } else if (operatorStr == "<") {
      conditionMet = (value < threshold);
    } else if (operatorStr == ">=") {
      conditionMet = (value >= threshold);
    } else if (operatorStr == "<=") {
      conditionMet = (value <= threshold);
    } else if (operatorStr == "==") {
      conditionMet = (value == threshold);
    } else {
      DEBUGLN("Automation rule invalid operator: " + operatorStr);
      continue;
    }

    if (conditionMet) {
      DEBUGLN("Automation condition met: " + variable + " " + operatorStr +
              " " + String(threshold) + " (value: " + String(value) + ")");

      // Execute action
      executeAutomationAction(actionType, target, mode, actionValue);

      // Handle time limit if specified
      if (timeLimit.length() > 0 && timeoutValue.length() > 0) {
        int timeLimitSeconds = timeLimit.toInt();
        if (timeLimitSeconds > 0 && timeoutCount < 10) {
          // Add to timeout tracking
          timeoutTracking[timeoutCount].actionType = actionType;
          timeoutTracking[timeoutCount].target = target;
          timeoutTracking[timeoutCount].mode = mode;
          timeoutTracking[timeoutCount].timeoutValue = timeoutValue;
          timeoutTracking[timeoutCount].startTime = millis();
          timeoutTracking[timeoutCount].timeoutDuration =
              timeLimitSeconds * 1000; // Convert to milliseconds
          timeoutTracking[timeoutCount].active = true;
          timeoutCount++;

          DEBUGLN("Automation timeout set for " + String(timeLimitSeconds) +
                  " seconds");
        }
      }
    }
  }

  // Check for timeouts
  checkAutomationTimeouts();
}

void Cynoiot::executeAutomationAction(String actionType, String target,
                                      String mode, String value) {
  if (actionType == "gpio" || actionType == "g") {
    // GPIO control action
    uint8_t gpioIndex = 0;
    while (gpio[gpioIndex].length() != 0 && gpioIndex < bufferIO) {
      gpioIndex++;
    }

    if (gpioIndex < bufferIO) {
      String gpioCommand = target + ":" + mode + ":" + value;
      gpio[gpioIndex] = gpioCommand;

      DEBUGLN("Automation GPIO: " + gpioCommand);
    } else {
      DEBUGLN("Automation GPIO buffer full");
    }
  } else if (actionType == "event" || actionType == "e") {
    // Event emit action (mode parameter not used for events)
    uint8_t eventIndex = 0;
    while (event[eventIndex].length() != 0 && eventIndex < bufferIO) {
      eventIndex++;
    }

    if (eventIndex < bufferIO) {
      event[eventIndex] = target;
      ::value[eventIndex] = value;

      DEBUGLN("Automation Event: " + target + " value: " + value);
    } else {
      DEBUGLN("Automation Event buffer full");
    }
  } else {
    DEBUGLN("Unknown automation action type: " + actionType);
  }
}

void Cynoiot::checkAutomationTimeouts() {
  unsigned long currentTime = millis();

  for (int i = 0; i < timeoutCount; i++) {
    if (timeoutTracking[i].active) {
      // Check if timeout period has elapsed
      unsigned long elapsedTime = currentTime - timeoutTracking[i].startTime;

      if (elapsedTime >= timeoutTracking[i].timeoutDuration) {
        // Execute timeout action
        executeAutomationAction(
            timeoutTracking[i].actionType, timeoutTracking[i].target,
            timeoutTracking[i].mode, timeoutTracking[i].timeoutValue);

        // Mark as inactive
        timeoutTracking[i].active = false;

        DEBUGLN("Automation timeout executed for: " +
                timeoutTracking[i].target);
      }
    }
  }

  // Clean up inactive timeouts
  int activeCount = 0;
  for (int i = 0; i < timeoutCount; i++) {
    if (timeoutTracking[i].active) {
      if (activeCount != i) {
        timeoutTracking[activeCount] = timeoutTracking[i];
      }
      activeCount++;
    }
  }
  timeoutCount = activeCount;
}