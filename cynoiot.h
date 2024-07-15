#ifndef cynoiot_h
#define cynoiot_h

#include <Arduino.h>
#include <MQTT.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#define DEFAULT_SERVER "192.168.0.101"
#define RECONNECT_SERVER_TIME 60000   // in ms
#define MAX_PUBLISH_TIME 4500   // in ms

#define CYNOIOT_DEBUG

class Cynoiot
{

private:
  String _client_id;
  const char _secret[14] = {0x63, 0x79, 0x6E, 0x6F, 0x69, 0x6F, 0x74, 0x62, 0x75, 0x6E, 0x64, 0x6C, 0x65, 0x00};
  String _var[16];
  uint8_t _numElements = 0;
  bool _connected = false;
  uint32_t _lastReConnect,_lastPublish;

  void subscribe();
  static void messageReceived(String &topic, String &payload);

  void publish(String payload);
  void publish(String payload, String topic);

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