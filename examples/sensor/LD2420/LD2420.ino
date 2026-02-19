/*
 * LD2420 Radar Sensor Wiring for ESP8266
 * ======================================
 *
 * LD2420 Module          ESP8266 NodeMCU/Wemos
 * -------------         --------------------
 * VCC (5V)      ----->   Vin or 5V
 * GND           ----->   GND
 * Ot1            ----->   D5 (GPIO14) - SoftwareSerial RX
 * RX            ----->   D6 (GPIO12) - SoftwareSerial TX
 *
 * For ESP32 Wiring:
 * LD2420 Ot1  ----->   ESP32 RX2 (GPIO16)
 * LD2420 RX  ----->   ESP32 TX2 (GPIO17)
 *
 * Note: The LD2420 operates at 5V, but TX/RX pins are 3.3V compatible
 * Ensure proper power supply for stable operation
 */

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include "LD2420.h"
#include <cynoiot.h>

// Serial configuration for radar
#ifdef ESP8266
SoftwareSerial RadarSerial(D5, D6); // RX, TX for ESP8266
#elif defined(ESP32)
#define RadarSerial Serial1
#endif

#define LED_PIN D8 // GPIO2 for ESP8266 or ESP32

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

// PWM fading variables
int currentPWM = 0;
int targetPWM = 0;
unsigned long lastFadeUpdate = 0;
unsigned long fadeInterval = 10;

// Calculate target PWM based on distance
// Closer = brighter, farther = dimmer
void calculateTargetPWM() {
  if (objectDetected) {
    // Map distance to PWM: 0cm = 255, 400cm = 0
    // Using inverse relationship - closer is brighter
    int pwm = map(lastDistance, 250, 450, 255, 0);
    // Constrain to valid PWM range
    targetPWM = constrain(pwm, 0, 255);
  } else {
    targetPWM = 0;
  }
}

// Fade PWM function with asymmetric timing
// Fast response when brightening (10ms), slow when dimming (100ms)
void fadePWM() {
  if (currentPWM != targetPWM) {
    unsigned long currentMillis = millis();

    if (currentMillis - lastFadeUpdate >= fadeInterval) {
      lastFadeUpdate = currentMillis;

      if (currentPWM < targetPWM) {
        // Brightening - fast update every 10ms
        currentPWM++;
        fadeInterval = 10;
      } else if (currentPWM > targetPWM) {
        // Dimming - slow update every 100ms
        currentPWM--;
        fadeInterval = 50;
      }

      analogWrite(LED_PIN, currentPWM);

    }
  }
}

void iotSetup() {
  uint8_t numVariables = 3;
  String keyname[numVariables] = {"state", "distance","pmw"};
  iot.setkeyname(keyname, numVariables);
  Serial.print("Connecting to server.");
  iot.connect(email);
}

bool verifyRadarConnection() {
  Serial.println("Verifying radar connection...");
  unsigned long timeout = millis() + 2000; // 2 second timeout

  while (millis() < timeout) {
    radar.update();
    delay(10);
    // If we get any data (distance changes from default 800), radar is
    // connected
    if (lastDistance != 800 || objectDetected) {
      return true;
    }
  }
  return false;
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
  Serial.println("Initializing radar...");

  // Initialize LED pin for PWM
  pinMode(LED_PIN, OUTPUT);
  analogWrite(LED_PIN, 0);

  if (radar.begin(RadarSerial)) {
    radar.onDetection(onObjectDetected);
    // radar.onStateChange(onStateChange);

    // Verify radar is actually connected by checking for data
    if (verifyRadarConnection()) {
      Serial.println("Radar initialized successfully!");
      Serial.println("LD2420 is connected and responding.");
    } else {
      Serial.println("WARNING: Radar initialized but no data received!");
      Serial.println("Please check wiring:");
      Serial.println("  - LD2420 TX -> D5 (ESP8266) or RX2/GPIO16 (ESP32)");
      Serial.println("  - LD2420 RX -> D6 (ESP8266) or TX2/GPIO17 (ESP32)");
      Serial.println("  - LD2420 VCC -> 5V");
      Serial.println("  - LD2420 GND -> GND");
    }
  } else {
    Serial.println("ERROR: Failed to initialize radar!");
    Serial.println("Please check your wiring and connections.");
  }
}

void loop() {
  iot.handle();

  unsigned long currentMillis = millis();

  if (currentMillis - radarMillis >= 10) {
    radarMillis = currentMillis;
    radar.update();
  }

  if (objectDetected && currentMillis - lastDetectionMillis > 5000) {
    // Serial.print(String(currentMillis) + " - " + String(lastDetectionMillis) +
    //              " = " + String(currentMillis - lastDetectionMillis));
    objectDetected = false;
    lastDistance = 800;
    Serial.println("Object lost from delay");
  }

  // Calculate target PWM based on distance
  calculateTargetPWM();

  // Apply PWM fading with asymmetric timing
  fadePWM();

  if (currentMillis - previousMillis >= 5000) {
    previousMillis = currentMillis;

    float val[3] = {objectDetected ? 1 : 0, (int)lastDistance, (int)currentPWM};
    iot.update(val);

    // Serial.print("State: ");
    // Serial.print(objectDetected ? "Detected" : "No Object");
    // Serial.print(", Distance: ");
    // Serial.print(lastDistance);
    // Serial.println(" cm");
  }
}

void onObjectDetected(int distance) {
  lastDistance = distance;
  objectDetected = true;
  lastDetectionMillis = millis() - 1000;
  // Serial.println("set lastDetectionMillis = " + String(lastDetectionMillis));
  // Serial.print("Object at ");
  // Serial.print(distance);
  // Serial.println(" cm");
}

// void onStateChange(LD2420_DetectionState oldState, LD2420_DetectionState newState) {
//   if (newState == LD2420_NO_DETECTION || newState == LD2420_DETECTION_LOST) {
//     // Library reports object lost, but don't print yet - wait for 5-second timeout
//     objectDetected = false;
//     lastDistance = 800;
//     Serial.println("Object lost state");
//   }
// }
