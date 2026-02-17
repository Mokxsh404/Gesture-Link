// Glove A main code - ESP32-C6 (v4: OLED & State Machine)
#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define FLEX1_PIN 6
#define FLEX2_PIN 0
#define FLEX3_PIN 1
#define FLEX4_PIN 2

#define OLED_RESET -1
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, OLED_RESET);

HardwareSerial dfPlayerSerial(1);
DFRobotDFPlayerMini dfPlayer;

int flexValues[4] = {0,0,0,0};
String currentGesture = "IDLE";

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  
  display.begin(0x3C, true);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(10, 10);
  display.print("Glove A Init...");
  display.display();
  
  dfPlayerSerial.begin(9600, SERIAL_8N1, 16, 17);
}

void loop() {
  flexValues[0] = analogRead(FLEX1_PIN);
  
  display.clearDisplay();
  display.setCursor(10, 10);
  display.printf("F1: %d", flexValues[0]);
  display.setCursor(10, 30);
  display.print("Gesture: " + currentGesture);
  display.display();
  
  if (flexValues[0] > 3000) {
    currentGesture = "HELLO";
    dfPlayer.play(11);
    delay(2000);
  } else {
    currentGesture = "IDLE";
  }
  delay(100);
}
