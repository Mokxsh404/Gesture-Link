// Visora main code - ESP32-S3 Sense (v2: YOLO TCP Client)
#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

Adafruit_ST7735 tft(1, 2, 6, 5, 3); // CS, DC, MOSI, SCLK, RST
WiFiClient client;

void setup() {
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 10);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("Object detection init");
}

void loop() {
  // Connect and read detections
  delay(100);
}
