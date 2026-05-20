// Visora main code - ESP32-S3 Sense (v4: MAX98357A I2S Audio Amp)
#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_ST7735.h>
#include "driver/i2s.h"

void setupI2SSpeaker() {
  const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 22050,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 512,
      .use_apll = false
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
}

void setup() {
  Serial.begin(115200);
  setupI2SSpeaker();
  Serial.println("I2S speaker amp ready.");
}

void loop() {
  delay(100);
}
