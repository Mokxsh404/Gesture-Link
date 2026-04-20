#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/i2s.h>

// display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// server running on laptop
const char *server_host = "XX.XX.XX.XX"; // update with your server's IP
const int server_port = 8080;

// INMP441 mic
#define I2S_WS 15
#define I2S_SD 32
#define I2S_SCK 14
#define I2S_PORT I2S_NUM_0

#define RECORD_BUTTON_PIN 12

// braille output pins - 2 cells, 6 pins each
// -1 means no pin (unused solenoid slot)
const int braillePins[12] = {
    23, 27, 26, 4,  16, 2, // cell 1 (left hand)
    -1, 19, 5,  33, 25, -1 // cell 2 (right hand)
};

#define VIBRATION_TIME 2000 // how long each letter vibrates in ms

// standard braille alphabet A-Z
const byte brailleAlphabet[26] = {
    B100000, B101000, B110000, B110100, B100100, B111000, B111100,
    B101100, B011000, B011100, B100010, B101010, B110010, B110110,
    B100110, B111010, B111110, B101110, B011010, B011110, B100011,
    B101011, B011101, B110011, B110111, B100111};

// bitmaps
static const unsigned char PROGMEM image_hand_stop_bits[] = {
    0x0c, 0x00, 0x1b, 0x00, 0x2a, 0x80, 0x2a, 0x80, 0x6a, 0x80, 0xaa,
    0x80, 0xaa, 0x80, 0xaa, 0x98, 0xaa, 0xa8, 0xa0, 0xc8, 0x80, 0x90,
    0x80, 0x20, 0x80, 0x20, 0x40, 0x40, 0x40, 0x80, 0x21, 0x80};

static const unsigned char PROGMEM image_Pin_arrow_right_bits[] = {
    0x04, 0x00, 0x06, 0x00, 0xff, 0x00, 0xff,
    0x80, 0xff, 0x00, 0x06, 0x00, 0x04, 0x00};

static const unsigned char PROGMEM image_wifi_not_connected_bits[] = {
    0x21, 0xf0, 0x00, 0x16, 0x0c, 0x00, 0x08, 0x03, 0x00, 0x25, 0xf0, 0x80,
    0x42, 0x0c, 0x40, 0x89, 0x02, 0x20, 0x10, 0xa1, 0x00, 0x23, 0x58, 0x80,
    0x04, 0x24, 0x00, 0x08, 0x52, 0x00, 0x01, 0xa8, 0x00, 0x02, 0x04, 0x00,
    0x00, 0x42, 0x00, 0x00, 0xa1, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM image_wifi_bits[] = {
    0x01, 0xf0, 0x00, 0x06, 0x0c, 0x00, 0x18, 0x03, 0x00, 0x21, 0xf0, 0x80,
    0x46, 0x0c, 0x40, 0x88, 0x02, 0x20, 0x10, 0xe1, 0x00, 0x23, 0x18, 0x80,
    0x04, 0x04, 0x00, 0x08, 0x42, 0x00, 0x01, 0xb0, 0x00, 0x02, 0x08, 0x00,
    0x00, 0x40, 0x00, 0x00, 0xa0, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00};

WiFiClient client;

void i2s_install() {
  const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false};
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {.bck_io_num = I2S_SCK,
                                       .ws_io_num = I2S_WS,
                                       .data_out_num = -1,
                                       .data_in_num = I2S_SD};
  i2s_set_pin(I2S_PORT, &pin_config);
}

void clearBraille() {
  for (int i = 0; i < 12; i++) {
    if (braillePins[i] != -1) {
      digitalWrite(braillePins[i], LOW);
    }
  }
}

void showChar(char c) {
  if (c < 'A' || c > 'Z')
    return;
  byte pattern = brailleAlphabet[c - 'A'];

  for (int i = 0; i < 6; i++) {
    if (bitRead(pattern, 5 - i)) {
      if (braillePins[i] != -1)
        digitalWrite(braillePins[i], HIGH);
    }
  }
}

void showBraille(String text) {
  text.toUpperCase();
  // strip punctuation
  text.replace(".", "");
  text.replace(",", "");
  text.replace("!", "");
  text.replace("?", "");
  text.trim();

  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 4, WHITE);
  display.setCursor(10, 10);
  display.println("Translating...");
  display.display();
  delay(300);

  for (int i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == ' ') {
      delay(VIBRATION_TIME);
      continue;
    }

    display.clearDisplay();
    display.drawRoundRect(0, 0, 128, 64, 4, WHITE);

    display.drawLine(0, 16, 128, 16, WHITE);
    display.setTextSize(1);
    display.setCursor(4, 4);
    display.print("Braille Output");

    // big letter display
    display.setTextSize(4);
    display.setCursor(50, 25);
    display.print(c);

    // progress bar along the bottom
    int progress = map(i, 0, text.length(), 0, 128);
    display.fillRect(0, 60, progress, 4, WHITE);
    display.display();

    clearBraille();
    showChar(c);

    unsigned long start = millis();
    while (millis() - start < VIBRATION_TIME)
      yield();

    clearBraille();
    delay(200);
  }

  // show the full phrase at the end
  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 4, WHITE);

  display.setTextSize(1);
  display.setCursor(5, 5);
  display.println("You said:");

  display.setTextSize(2);
  display.setCursor(10, 22);
  if (text.length() > 12) {
    display.println(text.substring(0, 12));
    display.setCursor(10, 40);
    if (text.length() > 24) {
      display.println(text.substring(12, 24) + "...");
    } else {
      display.println(text.substring(12));
    }
  } else {
    display.println(text);
  }

  display.display();
  delay(2000);
}

void setup() {
  Serial.begin(115200);

  pinMode(RECORD_BUTTON_PIN, INPUT_PULLUP);
  for (int i = 0; i < 12; i++) {
    if (braillePins[i] != -1) {
      pinMode(braillePins[i], OUTPUT);
      digitalWrite(braillePins[i], LOW);
    }
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextWrap(true);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(2, 15);
  display.println("Gestura ");

  display.setFont();
  display.setCursor(5, 28);
  display.println("Glove B");

  display.drawBitmap(56, 23, image_hand_stop_bits, 13, 16, WHITE);
  display.drawBitmap(106, 2, image_wifi_not_connected_bits, 19, 16, WHITE);

  display.setCursor(39, 53);
  display.println("Initializing");

  display.drawBitmap(115, 54, image_Pin_arrow_right_bits, 9, 7, WHITE);
  display.display();

  delay(3000);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.setCursor(0, 20);
  display.println(ssid);
  display.drawRect(10, 40, 108, 10, WHITE);
  display.display();

  WiFi.begin(ssid, password);
  int load = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    load = (load + 10) % 100;
    display.fillRect(12, 42, load, 6, WHITE);
    display.display();
  }

  Serial.println("\nWiFi Connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(0, 20);
  display.println("WiFi Connected!");
  display.drawBitmap(50, 40, image_wifi_bits, 19, 16, WHITE);
  display.display();
  delay(1000);

  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);

  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 4, WHITE);
  display.setTextSize(2);
  display.setCursor(15, 20);
  display.println("Ready");
  display.setTextSize(1);
  display.setCursor(20, 45);
  display.println("Hold to Talk");
  display.display();

  Serial.println("Setup complete!");
}

enum State { IDLE, RECORDING, WAITING_RESPONSE, DISPLAYING };
State currentState = IDLE;

String receivedText = "";
unsigned long recordStartTime = 0;
unsigned long responseStartTime = 0;
unsigned long bytesSent = 0;

void loop() {
  static unsigned long lastIdleDraw = 0;
  bool btnPressed = (digitalRead(RECORD_BUTTON_PIN) == LOW);

  switch (currentState) {
  case IDLE:
    if (btnPressed) {
      Serial.println("Connecting to server...");
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 10);
      display.println("Connecting...");
      display.display();

      if (client.connect(server_host, server_port)) {
        Serial.println("Connected to server!");
        Serial.println("Recording started");
        currentState = RECORDING;
        recordStartTime = millis();
        bytesSent = 0;

        display.clearDisplay();
        display.fillCircle(64, 32, 12, WHITE);
        display.setCursor(30, 52);
        display.setTextSize(1);
        display.println("LISTENING...");
        display.display();
      } else {
        Serial.println("Connection failed");
        display.clearDisplay();
        display.setCursor(0, 20);
        display.println("Connect Failed");
        display.display();
        delay(2000);
      }
    } else {
      // redraw idle screen every second so it stays clean
      if (millis() - lastIdleDraw > 1000) {
        display.clearDisplay();
        display.drawRoundRect(0, 0, 128, 64, 4, WHITE);
        display.setTextSize(2);
        display.setCursor(15, 20);
        display.println("Ready");
        display.setTextSize(1);
        display.setCursor(20, 45);
        display.println("Hold to Talk");
        display.display();
        lastIdleDraw = millis();
      }
    }
    break;

  case RECORDING:
    if (!btnPressed) {
      // button released, stop sending audio
      unsigned long duration = millis() - recordStartTime;
      Serial.printf("Recording stopped. Duration: %.1fs, Sent: %lu bytes\n",
                    duration / 1000.0, bytesSent);

      client.flush();

      currentState = WAITING_RESPONSE;
      responseStartTime = millis();

      display.clearDisplay();
      display.drawRoundRect(0, 0, 128, 64, 4, WHITE);
      display.setCursor(10, 25);
      display.setTextSize(1);
      display.println("Processing...");
      display.drawRect(14, 40, 100, 8, WHITE);
      display.display();

    } else {
      if (client.connected()) {
        size_t bytes_read;
        int16_t buffer[1024];
        i2s_read(I2S_PORT, &buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

        if (bytes_read > 0) {
          client.write((const uint8_t *)buffer, bytes_read);
          bytesSent += bytes_read;
        }
      } else {
        Serial.println("Lost connection during recording");
        currentState = IDLE;
      }
    }
    break;

  case WAITING_RESPONSE: {
    // animate the progress bar while waiting
    unsigned long elapsed = millis() - responseStartTime;
    int progress = (elapsed % 1000);
    int width = map(progress, 0, 1000, 0, 96);
    display.fillRect(16, 42, width, 4, WHITE);
    display.display();
  }

    if (client.available()) {
      receivedText = client.readStringUntil('\n');
      receivedText.trim();

      Serial.print("Received: '");
      Serial.print(receivedText);
      Serial.print("' (Length: ");
      Serial.print(receivedText.length());
      Serial.println(")");

      client.stop();

      if (receivedText.length() > 0) {
        if (receivedText.startsWith("ERROR:")) {
          Serial.println("Error from server");
          display.clearDisplay();
          display.setCursor(10, 20);
          display.println("Error!");
          display.setCursor(5, 35);
          display.println("Try again");
          display.display();
          delay(1500);
          currentState = IDLE;
        } else {
          currentState = DISPLAYING;
        }
      } else {
        Serial.println("Empty response received");
        currentState = IDLE;
      }
    }

    // give up after 10 seconds
    if (millis() - responseStartTime > 10000) {
      Serial.println("Response timeout");
      display.clearDisplay();
      display.setCursor(10, 20);
      display.println("Timeout!");
      display.setCursor(5, 35);
      display.println("Try again");
      display.display();
      delay(1500);
      client.stop();
      currentState = IDLE;
    }
    break;

  case DISPLAYING:
    Serial.print("Displaying braille for: '");
    Serial.print(receivedText);
    Serial.println("'");
    showBraille(receivedText);
    Serial.println("Braille display complete");
    currentState = IDLE;
    break;
  }

  yield();
}