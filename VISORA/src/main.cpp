// Visora main code - ESP32-S3 Sense (v3: HSV Colour Client)
#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_ST7735.h>

Adafruit_ST7735 tft(1, 2, 6, 5, 3);

void setup() {
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 10);
  tft.print("Colour detection init");
}

void loop() {
  delay(100);
}
