// Glove A main code - ESP32-C6 (v5: WiFi Web Server Telemetry)
#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <WiFi.h>
#include <WebServer.h>

#define FLEX1_PIN 6
#define FLEX2_PIN 0
#define FLEX3_PIN 1
#define FLEX4_PIN 2

Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
WebServer server(80);

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  display.begin(0x3C, true);
  
  WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  
  server.on("/data", []() {
    String json = "{\"flex1\": 1024, \"gesture\": \"IDLE\"}";
    server.send(200, "application/json", json);
  });
  server.begin();
}

void loop() {
  server.handleClient();
  delay(5);
}
