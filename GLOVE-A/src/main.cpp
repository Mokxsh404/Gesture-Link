// Glove A main code - ESP32-C6 (v1: Initial Flex Read & DFPlayer)
#include <Arduino.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#define FLEX1_PIN 6
#define FLEX2_PIN 0
#define FLEX3_PIN 1
#define FLEX4_PIN 2

#define DFPLAYER_TX 17
#define DFPLAYER_RX 16

HardwareSerial dfPlayerSerial(1);
DFRobotDFPlayerMini dfPlayer;

void setup() {
  Serial.begin(115200);
  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  
  pinMode(FLEX1_PIN, INPUT);
  pinMode(FLEX2_PIN, INPUT);
  pinMode(FLEX3_PIN, INPUT);
  pinMode(FLEX4_PIN, INPUT);
  
  delay(1000);
  if (dfPlayer.begin(dfPlayerSerial, false, true)) {
    dfPlayer.volume(30);
    Serial.println("DFPlayer ready!");
  } else {
    Serial.println("DFPlayer failed!");
  }
}

void loop() {
  int f1 = analogRead(FLEX1_PIN);
  int f2 = analogRead(FLEX2_PIN);
  int f3 = analogRead(FLEX3_PIN);
  int f4 = analogRead(FLEX4_PIN);
  
  Serial.printf("Flex raw: %d, %d, %d, %d\n", f1, f2, f3, f4);
  
  if (f1 > 3000) {
    dfPlayer.play(1); // Play sound on finger bend
    delay(2000);
  }
  delay(100);
}
