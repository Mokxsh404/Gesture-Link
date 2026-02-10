// Glove A main code - ESP32-C6 (v3: IMU Integration)
#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#define FLEX1_PIN 6
#define FLEX2_PIN 0
#define FLEX3_PIN 1
#define FLEX4_PIN 2

#define IMU_SDA 21
#define IMU_SCL 22

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
  Wire.begin(IMU_SDA, IMU_SCL);
  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  
  pinMode(FLEX1_PIN, INPUT);
  pinMode(FLEX2_PIN, INPUT);
  pinMode(FLEX3_PIN, INPUT);
  pinMode(FLEX4_PIN, INPUT);
}

void loop() {
  int f1 = readFlexNormalized(FLEX1_PIN, 0);
  int f2 = readFlexNormalized(FLEX2_PIN, 1);
  int f3 = readFlexNormalized(FLEX3_PIN, 2);
  int f4 = readFlexNormalized(FLEX4_PIN, 3);
  
  // Dummy reading from MPU6050
  float pitch = 0.0;
  float roll = 0.0;
  
  Serial.printf("Flex: %d%%, IMU Pitch: %.1f, Roll: %.1f\n", f1, pitch, roll);
  delay(100);
}
