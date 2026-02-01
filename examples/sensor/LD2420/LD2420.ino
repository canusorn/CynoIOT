#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <cynoiot.h>
#include "LD2420.h"

// Serial configuration for radar
#ifdef ESP8266
SoftwareSerial RadarSerial(D4, D3); // RX, TX for ESP8266
#elif defined(ESP32)
#define RadarSerial Serial1
#endif

LD2420 radar;
Cynoiot iot;

const char ssid[] = "G6PD_2.4G";
const char pass[] = "570610193";
const char email[] = "anusorn1998@gmail.com";

unsigned long previousMillis = 0;
unsigned long lastDetectionMillis = 0;
unsigned long radarMillis = 0;
int lastDistance = 800;
bool objectDetected = false;

void iotSetup() {
  uint8_t numVariables = 2;
  String keyname[numVariables] = {"state", "distance"};
  iot.setkeyname(keyname, numVariables);
  Serial.print("Connecting to server.");
  iot.connect(email);
}

void setup() {
  Serial.begin(115200);
#ifdef ESP8266
  RadarSerial.begin(115200);
#elif defined(ESP32)
  Serial1.begin(115200, SERIAL_8N1, 16, 17);
#endif
  
  Serial.println();
  Serial.print("Wifi connecting to ");
  Serial.print(ssid);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("\nWiFi connected, IP address: ");
  Serial.println(WiFi.localIP());

  iotSetup();

  delay(1000);
  if (radar.begin(RadarSerial)) {
    Serial.println("Radar initialized successfully!");
    radar.onDetection(onObjectDetected);
  }
}

void loop() {
  iot.handle();
  
  unsigned long currentMillis = millis();
  
  if (currentMillis - radarMillis >= 10) {
    radarMillis = currentMillis;
    radar.update();
  }
  
  if (objectDetected && currentMillis - lastDetectionMillis > 2000) {
    objectDetected = false;
    lastDistance = 800;
  }
  
  if (currentMillis - previousMillis >= 5000) {
    previousMillis = currentMillis;
    
    float val[2] = {objectDetected ? 1 : 0, (float)lastDistance};
    iot.update(val);
    
    Serial.print("State: ");
    Serial.print(objectDetected ? "Detected" : "No Object");
    Serial.print(", Distance: ");
    Serial.print(lastDistance);
    Serial.println(" cm");
  }
  
}

void onObjectDetected(int distance) {
  lastDistance = distance;
  objectDetected = true;
  lastDetectionMillis = millis();
  Serial.print("Object at ");
  Serial.print(distance);
  Serial.println(" cm");
}