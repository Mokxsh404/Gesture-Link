// Glove A main code - ESP32-C6 (v2: Calibration added)
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

int flexMin[4] = {1000, 1000, 1000, 1000};
int flexMax[4] = {3500, 3500, 3500, 3500};

int readFlexNormalized(int pin, int idx) {
  int raw = analogRead(pin);
  int pct = map(raw, flexMin[idx], flexMax[idx], 0, 100);
  return constrain(pct, 0, 100);
}

void setup() {
  Serial.begin(115200);
  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  
  pinMode(FLEX1_PIN, INPUT);
  pinMode(FLEX2_PIN, INPUT);
  pinMode(FLEX3_PIN, INPUT);
  pinMode(FLEX4_PIN, INPUT);
  
  // Basic calibration routine
  Serial.println("Keep hand flat for calibration...");
  delay(2000);
  for(int i=0; i<4; i++) {
    flexMin[0] = analogRead(FLEX1_PIN);
    flexMin[1] = analogRead(FLEX2_PIN);
    flexMin[2] = analogRead(FLEX3_PIN);
    flexMin[3] = analogRead(FLEX4_PIN);
  }
  Serial.println("Calibration done!");
}

void loop() {
  int f1 = readFlexNormalized(FLEX1_PIN, 0);
  int f2 = readFlexNormalized(FLEX2_PIN, 1);
  int f3 = readFlexNormalized(FLEX3_PIN, 2);
  int f4 = readFlexNormalized(FLEX4_PIN, 3);
  
  Serial.printf("Flex percents: %d%%, %d%%, %d%%, %d%%\n", f1, f2, f3, f4);
  
  if (f1 > 80) {
    dfPlayer.play(1);
    delay(2000);
  }
  delay(100);
}
