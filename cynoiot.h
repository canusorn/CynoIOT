#ifndef cynoiot_h
#define cynoiot_h

#include <Arduino.h>
#include <EEPROM.h>
#include <MQTT.h>

#ifdef ESP8266
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecureBearSSL.h>

#elif defined(ESP32)
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#endif

#define IOTVERSION "1.0.7"

#ifndef DEFAULT_SERVER
#define DEFAULT_SERVER "cynoiot.com"
// #define DEFAULT_SERVER "192.168.1.221"
#endif

#define RECONNECT_SERVER_TIME 15000 // in ms

#ifndef MAX_PUBLISH_TIME
#define MAX_PUBLISH_TIME 500 // in ms
#endif

#define CYNOIOT_DEBUG
#define PORT 7458

// Define callback function type
typedef void (*EventCallbackFunction)(String event, String value);

class Cynoiot {

private:
  String _email = "";
  const char _secret[14] = {0x63, 0x79, 0x6E, 0x6F, 0x69, 0x6F, 0x74,
                            0x62, 0x75, 0x6E, 0x64, 0x6C, 0x65, 0x00};
  String _var[32];
  uint8_t _numElements = 0;
  bool _connected = false;
  const uint8_t _noSubTime = 5;
  uint32_t _lastReConnect, _lastPublish;
  String _topic;
  bool _Subscribed = false;
  String _template = "";
  uint8_t _templateVersion = 0;
  EventCallbackFunction _eventCallback = NULL; // Store the callback function
  bool _initRequested = false; // Flag to track if init data has been requested

  String getPublishTopic();
  bool subscribe();
  void publish(String payload);
  void publish(String payload, String topic);
  void parsePinsString(const String &input);
  void pinHandle(const String &pins, const String &modes, const String &values);
  void checkSubscription();
  void updateOTA(String otafile);
  void handleTimestamp();
  void handleAutomation(String varrable, float value);
  void executeAutomationAction(String actionType, String target, String mode, String value);
  void checkAutomationTimeouts();
  void removeBufferEntry(const String &prefix);
  
#ifdef ESP8266
  int getPinNumber(String pinId);
  String getDpin(int pin);
#elif defined(ESP32)
#endif

  static void messageReceived(String &topic, String &payload);

  void templatePublish();
  void checkUpdateTimestamps();
  void requestInitData();

public:
  Cynoiot();

  // masage from server
  String noti;

  String getClientId();

  void debug(String msg);
  bool connect(String email);

  bool connect(const char email[]);

  bool connect(const char server[], const char email[]);

  void handle();

  void setkeyname(String keyname[], uint8_t numElements);

  void update(float val[]);

  bool status();

  void setTemplate(String templateName);
  void setTemplate(String templateName, uint8_t version);

  void sendDeviceInfo();

  // Set the event callback function
  void setEventCallback(EventCallbackFunction callback);

  // Call the event callback function
  void triggerEvent(String event, String value);

  void eventUpdate(String event, String value);
  void eventUpdate(String event, int value);

  void gpioUpdate(int pin, String value);
  void gpioUpdate(int pin, int value);
  void gpioUpdate(String pin, int value);

  uint32_t getTime();
  uint32_t getDaytimestamps();
  uint8_t getDayofWeek();
  uint8_t getHour();
  uint8_t getMinute();
  uint8_t getSecond();

  void printTimeDetails();
};

// for set debug mode
#ifdef CYNOIOT_DEBUG

#ifndef CYNOIOT_DEBUG_SERIAL
#define CYNOIOT_DEBUG_SERIAL Serial
#endif

#define DEBUG(...) CYNOIOT_DEBUG_SERIAL.print(__VA_ARGS__)
#define DEBUGLN(...) CYNOIOT_DEBUG_SERIAL.println(__VA_ARGS__)

#else
// Debugging mode off, disable the macros
#define DEBUG(...)
#define DEBUGLN(...)

#endif

#endif