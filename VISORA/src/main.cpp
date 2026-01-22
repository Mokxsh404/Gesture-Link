// Visora main code - ESP32-S3 Sense (v1: Camera Server Test)
#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

void startCameraServer();

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("Camera server starting...");
  startCameraServer();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  delay(1000);
}

// Dummy/basic implementation of server start
void startCameraServer() {
  Serial.println("MJPEG Camera server running on port 80");
}
