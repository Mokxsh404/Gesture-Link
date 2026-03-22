// Glove B main code - ESP32 (v2: Vibration Motor Sequencer)
#include <Arduino.h>

const int braillePins[12] = {
    23, 27, 26, 4,  16, 2, 
    -1, 19, 5,  33, 25, -1 
};

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 12; i++) {
    if (braillePins[i] != -1) {
      pinMode(braillePins[i], OUTPUT);
      digitalWrite(braillePins[i], LOW);
    }
  }
}

void loop() {
  for (int i = 0; i < 12; i++) {
    if (braillePins[i] != -1) {
      digitalWrite(braillePins[i], HIGH);
      delay(200);
      digitalWrite(braillePins[i], LOW);
      delay(100);
    }
  }
  delay(2000);
}
