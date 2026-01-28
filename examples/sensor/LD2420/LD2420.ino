#include "LD2420.h"

LD2420 radar;

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 16, 17); // RX, TX pins for ESP32
  delay(1000);
  if (radar.begin(Serial1)) {
    Serial.println("Radar initialized with Hardware Serial!");
    radar.onDetection(onObjectDetected);
  }
}

void loop() {
  radar.update();
  delay(10);
}

void onObjectDetected(int distance) {
  Serial.print("Object at ");
  Serial.print(distance);
  Serial.println(" cm");
}