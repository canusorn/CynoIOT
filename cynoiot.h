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
  String _client_id;
  const char _secret[14] = {0x63, 0x79, 0x6E, 0x6F, 0x69, 0x6F, 0x74, 0x62, 0x75, 0x6E, 0x64, 0x6C, 0x65, 0x00};
  String var[16];
  float val[16];
  uint8_t varNum = 0;

  void subscribe();
  static void messageReceived(String &topic, String &payload);

public:
  Cynoiot();
  void connect(const char email[]);
  void connect(const char server[], const char email[]);
  void handle();

  void setVar(const char v1[]);
  void setVar(const char v1[], const char v2[]);
  void setVar(const char v1[], const char v2[], const char v3[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[], const char v12[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[], const char v12[], const char v13[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[], const char v12[], const char v13[], const char v14[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[], const char v12[], const char v13[], const char v14[], const char v15[]);
  void setVar(const char v1[], const char v2[], const char v3[], const char v4[], const char v5[], const char v6[], const char v7[], const char v8[], const char v9[], const char v10[], const char v11[], const char v12[], const char v13[], const char v14[], const char v15[], const char v16[]);

  void update(float v1[]);
  void update(float v1[], float v2[]);
  void update(float v1[], float v2[], float v3[]);
  void update(float v1[], float v2[], float v3[], float v4[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[], float v12[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[], float v12[], float v13[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[], float v12[], float v13[], float v14[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[], float v12[], float v13[], float v14[], float v15[]);
  void update(float v1[], float v2[], float v3[], float v4[], float v5[], float v6[], float v7[], float v8[], float v9[], float v10[], float v11[], float v12[], float v13[], float v14[], float v15[], float v16[]);

  // void update();

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