// Glove B main code - ESP32 (v3: Braille Lookup Table)
#include <Arduino.h>

const int braillePins[12] = {
    23, 27, 26, 4,  16, 2, 
    -1, 19, 5,  33, 25, -1 
};

const byte brailleAlphabet[26] = {
    B100000, B101000, B110000, B110100, B100100, B111000, B111100,
    B101100, B011000, B011100, B100010, B101010, B110010, B110110,
    B100110, B111010, B111110, B101110, B011010, B011110, B100011,
    B101011, B011101, B110011, B110111, B100111
};

void fireBraille(char c) {
  if (c < 'A' || c > 'Z') return;
  byte pattern = brailleAlphabet[c - 'A'];
  for (int i = 0; i < 6; i++) {
    if (bitRead(pattern, 5 - i)) {
      if (braillePins[i] != -1) digitalWrite(braillePins[i], HIGH);
    }
  }
  delay(1500);
  for (int i = 0; i < 6; i++) {
    if (braillePins[i] != -1) digitalWrite(braillePins[i], LOW);
  }
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 12; i++) {
    if (braillePins[i] != -1) pinMode(braillePins[i], OUTPUT);
  }
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c >= 'a' && c <= 'z') c = c - 32;
    fireBraille(c);
  }
}
