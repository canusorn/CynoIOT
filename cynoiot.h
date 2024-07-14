#ifndef cynoiot_h
#define cynoiot_h

#include <Arduino.h>
#include <MQTT.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#define CYNOIOT_DEBUG

class Cynoiot
{

private:
  char _server;
  String _email;
  String _client_id;
  const char _secret[14] = {0x63, 0x79, 0x6E, 0x6F, 0x69, 0x6F, 0x74, 0x62, 0x75, 0x6E, 0x64, 0x6C, 0x65, 0x00};

  void subscribe();
  static void messageReceived(String &topic, String &payload);

public:
  Cynoiot();
  void connect(const char email[]);
  void connect(const char server[], const char email[]);
  void handle();
  void update();
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