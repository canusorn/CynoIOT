#ifndef cynoiot_h
#define cynoiot_h

#include <Arduino.h>
#include <MQTT.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#endif

#ifndef DEFAULT_SERVER
#define DEFAULT_SERVER "cynoiot.com"
//  #define DEFAULT_SERVER "192.168.0.101"
#endif

#define RECONNECT_SERVER_TIME 60000 // in ms

#ifndef MAX_PUBLISH_TIME
#define MAX_PUBLISH_TIME 500 // in ms
#endif

#define CYNOIOT_DEBUG

class Cynoiot
{

private:
  const char _secret[14] = {0x63, 0x79, 0x6E, 0x6F, 0x69, 0x6F, 0x74, 0x62, 0x75, 0x6E, 0x64, 0x6C, 0x65, 0x00};
  String _var[32];
  uint8_t _numElements = 0;
  bool _connected = false;
  const uint8_t _noSubTime = 5;
  uint32_t _lastReConnect, _lastPublish;
  String _topic;
  bool _Subscribed = false;


  String getPublishTopic();
  String getClientId();
  bool subscribe();
  void publish(String payload);
  void publish(String payload, String topic);
  void parsePinsString(const String &input);
  void pinHandle(const String &pins, const String &modes, const String &values);
  void checkSubscription();
#ifdef ESP8266
  int getPinNumber(String pinId);
  int Readpin[9];
#elif defined(ESP32)
  int Readpin[30];
#endif
  static void messageReceived(String &topic, String &payload);

public:
  Cynoiot();

  bool connect(const char email[]);
  bool connect(const char server[], const char email[]);

  void handle();

  void setkeyname(String keyname[], uint8_t numElements);

  void update(float val[]);

  bool status();
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