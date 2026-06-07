#include "DHT.h"
#include "driver/i2s.h"
#include "esp_camera.h"
#include "time.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <WiFi.h>

#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#include "board_config.h"

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// server addresses - update if laptop IP changes
const char *TTS_HOST = "192.168.x.x";
const uint16_t TTS_PORT = 5005;
const char *STT_HOST = "192.168.x.x";
const uint16_t STT_PORT = 5000;
const uint16_t COLOUR_SERVER_PORT = 8090;

// TFT display pins
#define TFT_CS 1
#define TFT_DC 2
#define TFT_RST 3
#define TFT_SCLK 5
#define TFT_MOSI 6
Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// I2S speaker output
#define I2S_DOUT 7
#define I2S_BCLK 8
#define I2S_LRC 9
#define SAMPLE_RATE_SPEAKER 22050

// I2S PDM mic input
#define I2S_MIC_SERIAL_CLOCK 42
#define I2S_MIC_LEFT_RIGHT_CLOCK 41
#define MIC_SAMPLE_RATE 16000
#define MIC_BUFFER_SAMPLES 512

// boost volume since the speaker is quiet
#define VOLUME_GAIN 3.0f

#define DHT_PIN 44
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

#define BUTTON_PIN 43

// screen states
enum ScreenState {
  SCREEN_INIT,
  SCREEN_VISORA,
  SCREEN_LANGUAGE_SELECT,
  SCREEN_CLOCK,
  SCREEN_DHT,
  SCREEN_OBJECT_DETECTION,
  SCREEN_COLOUR_RECOGNITION,
  SCREEN_GESTURE,
  SCREEN_STT
};

ScreenState screen = SCREEN_INIT;
ScreenState prevScreen = SCREEN_INIT;

WiFiClient sttClient;
WiFiServer colourServer(COLOUR_SERVER_PORT);
WiFiClient colourClient;

// object detection TCP server
const uint16_t OBJECT_DETECTION_PORT = 8010;
WiFiServer objectDetectionServer(OBJECT_DETECTION_PORT);
WiFiClient objectClient;
bool objectServerStarted = false;
String detectedObjects[5]; // Store up to 5 detected objects with positions
int detectedObjectCount = 0;
unsigned long lastObjectUpdate = 0;

String CAM_IP = ""; // set after wifi connects
const uint16_t CAM_PORT = 81;
bool colourServerStarted = false;

// gesture detection server
const uint16_t GESTURE_SERVER_PORT = 8095;
WiFiServer gestureServer(GESTURE_SERVER_PORT);
WiFiClient gestureClient;
bool gestureServerStarted = false;
String currentGestureLetter = "";
String gestureSentence = "";
unsigned long lastGestureUpdate = 0;

unsigned long lastDHTDraw = 0;
unsigned long lastButtonCheck = 0;
#define BUTTON_DEBOUNCE 300

uint16_t darkGreen = tft.color565(55, 213, 197);

unsigned long splashStart = 0;
bool splashDone = false;

bool visoraDone = false;

// NTP time sync (IST = UTC+5:30)
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;
unsigned long lastClockUpdate = 0;
bool timeConfigured = false;

int selectedLanguage = 0; // 0=English, 1=Hindi, 2=Punjabi, 3=Tamil, 4=Marathi
const char *languages[] = {"ENGLISH", "HINDI", "PUNJABI", "TAMIL", "MARATHI"};
const int numLanguages = 5;
bool languageSelected = false;
unsigned long buttonPressStart = 0;
bool buttonWasPressed = false;

String transcriptBuffer = "";
unsigned long lastTranscriptUpdate = 0;
String sttLines[7]; // 7 lines fits the display
int sttLineCount = 0;

String detectedColorName = "";
int colorR = 0, colorG = 0, colorB = 0;
bool colorDataReceived = false;
unsigned long lastColorUpdate = 0;

static const unsigned char PROGMEM image_microphone_muted_bits[] = {
    0x87, 0x00, 0x4f, 0x80, 0x26, 0x80, 0x13, 0x80, 0x09, 0x80, 0x04,
    0x80, 0x0a, 0x00, 0x0d, 0x00, 0x2e, 0xa0, 0x27, 0x40, 0x10, 0x20,
    0x0f, 0x90, 0x02, 0x08, 0x02, 0x04, 0x0f, 0x82, 0x00, 0x00};

static const unsigned char PROGMEM image_Release_arrow_bits[] = {
    0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00,
    0x33, 0x00, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x30, 0xc0,
    0x00, 0x00, 0x00, 0x30, 0xc0, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00,
    0x00, 0x30, 0x30, 0x00, 0x3f, 0xff, 0xf3, 0x0c, 0x00, 0x3f, 0xff, 0xf3,
    0x0c, 0x00, 0xc0, 0x00, 0x03, 0xc3, 0x00, 0xc0, 0x00, 0x03, 0xc3, 0x00,
    0xcf, 0xff, 0xff, 0xf0, 0xc0, 0xcf, 0xff, 0xff, 0xf0, 0xc0, 0xcf, 0xff,
    0xff, 0xfc, 0x30, 0xcf, 0xff, 0xff, 0xfc, 0x30, 0xcf, 0xff, 0xff, 0xf0,
    0xc0, 0xcf, 0xff, 0xff, 0xf0, 0xc0, 0xc0, 0x00, 0x03, 0xc3, 0x00, 0xc0,
    0x00, 0x03, 0xc3, 0x00, 0x3f, 0xff, 0xf3, 0x0c, 0x00, 0x3f, 0xff, 0xf3,
    0x0c, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00,
    0x00, 0x00, 0x30, 0xc0, 0x00, 0x00, 0x00, 0x30, 0xc0, 0x00, 0x00, 0x00,
    0x33, 0x00, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00,
    0x00, 0x00, 0x00, 0x3c, 0x00, 0x00};

static const unsigned char PROGMEM image_volume_no_sound_bits[] = {
    0x80, 0xe0, 0x00, 0x41, 0x20, 0x00, 0x22, 0x20, 0x00, 0x14, 0x20, 0x00,
    0x78, 0x20, 0x00, 0x44, 0x20, 0x00, 0x42, 0x20, 0x00, 0x41, 0x20, 0x00,
    0x40, 0xa0, 0x00, 0x40, 0x60, 0x00, 0x78, 0x20, 0x00, 0x04, 0x30, 0x00,
    0x02, 0x28, 0x00, 0x01, 0x24, 0x00, 0x00, 0xe2, 0x00, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM image_wifi_not_connected_bits[] = {
    0x21, 0xf0, 0x00, 0x16, 0x0c, 0x00, 0x08, 0x03, 0x00, 0x25, 0xf0, 0x80,
    0x42, 0x0c, 0x40, 0x89, 0x02, 0x20, 0x10, 0xa1, 0x00, 0x23, 0x58, 0x80,
    0x04, 0x24, 0x00, 0x08, 0x52, 0x00, 0x01, 0xa8, 0x00, 0x02, 0x04, 0x00,
    0x00, 0x42, 0x00, 0x00, 0xa1, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM image_images__1__1_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x07, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0xfb,
    0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x00, 0x0f, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x03, 0xff, 0xf8, 0x00, 0x03, 0xe0, 0x00, 0x00,
    0x00, 0x00, 0x3f, 0xff, 0xc0, 0x00, 0x01, 0xf8, 0x00, 0x00, 0x00, 0x00,
    0x3f, 0xfe, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x18, 0x3c, 0x7c,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x38, 0x07, 0xc0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0xfe, 0x38, 0x01, 0xf0, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3f, 0xc6, 0x18, 0x00, 0x3f, 0x81, 0xff, 0x80, 0x00, 0x01,
    0xff, 0x02, 0x18, 0x00, 0x1f, 0xff, 0xff, 0xfe, 0x00, 0x0f, 0xfe, 0x02,
    0x18, 0x00, 0x0f, 0xfc, 0x00, 0x1f, 0xdd, 0xff, 0xf8, 0x00, 0x1c, 0x00,
    0x0f, 0xf8, 0x00, 0x00, 0xff, 0xff, 0xe0, 0x00, 0x0c, 0x00, 0x06, 0x30,
    0x00, 0x00, 0x3f, 0xff, 0xc0, 0x00, 0x0c, 0x00, 0x06, 0x30, 0x00, 0x00,
    0x1f, 0xf8, 0x00, 0x00, 0x0c, 0x00, 0x06, 0x30, 0x00, 0x00, 0x1f, 0xc0,
    0x00, 0x00, 0x06, 0x00, 0x06, 0x38, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x0f, 0x3c, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x06, 0x00,
    0x0f, 0x3c, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x03, 0x00, 0x0e, 0x1e,
    0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x01, 0x80, 0x1e, 0x0d, 0x00, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x01, 0xc0, 0x1c, 0x0f, 0x00, 0x00, 0x0c, 0x00,
    0x00, 0x00, 0x00, 0xc0, 0x1c, 0x0f, 0x80, 0x00, 0x0c, 0x00, 0x00, 0x00,
    0x00, 0x60, 0x18, 0x07, 0x80, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x38,
    0x38, 0x03, 0x80, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xf0, 0x03,
    0xc0, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x07, 0xc0, 0x01, 0xc0, 0x00,
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x30, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x70, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0f, 0x87, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x03, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM image_volume_loud_bits[] = {
    0x01, 0xc1, 0x00, 0x02, 0x40, 0x80, 0x04, 0x44, 0x40, 0x08, 0x42, 0x20,
    0xf0, 0x51, 0x20, 0x80, 0x49, 0x10, 0x80, 0x44, 0x90, 0x80, 0x44, 0x90,
    0x80, 0x44, 0x90, 0x80, 0x49, 0x10, 0xf0, 0x51, 0x20, 0x08, 0x42, 0x20,
    0x04, 0x44, 0x40, 0x02, 0x40, 0x80, 0x01, 0xc1, 0x00, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM image_wifi_full_bits[] = {
    0x01, 0xf0, 0x00, 0x07, 0xfc, 0x00, 0x1e, 0x0f, 0x00, 0x39, 0xf3, 0x80,
    0x77, 0xfd, 0xc0, 0xef, 0x1e, 0xe0, 0x5c, 0xe7, 0x40, 0x3b, 0xfb, 0x80,
    0x17, 0x1d, 0x00, 0x0e, 0xee, 0x00, 0x05, 0xf4, 0x00, 0x03, 0xb8, 0x00,
    0x01, 0x50, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM image_arrow_curved_right_up_down_bits[] = {
    0x88, 0x6c, 0x3e, 0x0c, 0x08};
static const unsigned char PROGMEM image_camera_bits[] = {
    0x00, 0x78, 0x00, 0x30, 0x84, 0x00, 0x7f, 0x7b, 0x00, 0xc0, 0x00, 0x80,
    0x91, 0xc6, 0x80, 0x86, 0x30, 0x80, 0x85, 0xd0, 0x80, 0x8a, 0x28, 0x80,
    0x8a, 0x28, 0x80, 0x8a, 0x28, 0x80, 0x85, 0xd0, 0x80, 0x86, 0x30, 0x80,
    0x91, 0xc6, 0x80, 0xc0, 0x00, 0x80, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00};

// STT screen icons
static const unsigned char PROGMEM image_Layer_6_bits[] = {};
static const unsigned char PROGMEM image_microphone_recording_bits[] = {
    0x23, 0x88, 0x17, 0xd0, 0x05, 0x40, 0x67, 0xcc, 0x05, 0x40, 0x17,
    0xd0, 0x25, 0x48, 0x07, 0xc0, 0x07, 0xc0, 0x13, 0x90, 0x08, 0x20,
    0x07, 0xc0, 0x01, 0x00, 0x01, 0x00, 0x07, 0xc0, 0x00, 0x00};
static const unsigned char PROGMEM image_phone_call_in_out_bits[] = {
    0x38, 0x00, 0x44, 0x10, 0x84, 0x20, 0x85, 0x4e, 0x8d, 0x86, 0x89,
    0xca, 0x44, 0x10, 0x42, 0x20, 0x21, 0x00, 0x30, 0xbc, 0x18, 0x62,
    0x0c, 0x02, 0x06, 0x02, 0x01, 0x84, 0x00, 0x78, 0x00, 0x00};
static const unsigned char PROGMEM image_toilets_gentlemen_bits[] = {
    0x10, 0x38, 0x38, 0x10, 0x6c, 0xfe, 0xee, 0xfe,
    0xee, 0xfe, 0x7c, 0x6c, 0x6c, 0x28, 0x28, 0x6c};
static const unsigned char PROGMEM image_toilets_ladies_bits[] = {
    0x08, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x08, 0x00, 0x14, 0x00, 0x3e,
    0x00, 0x7f, 0x00, 0x5d, 0x00, 0x9c, 0x80, 0xbe, 0x80, 0x7f, 0x00,
    0x7f, 0x00, 0xff, 0x80, 0xff, 0x80, 0x14, 0x00, 0x36, 0x00};

// weather condition icons for DHT screen
static const unsigned char PROGMEM image_weather_cloud_lightning_bolt_bits[] = {
    0x00, 0x00, 0x00, 0x07, 0xc0, 0x00, 0x08, 0x20, 0x00, 0x10, 0x10, 0x00,
    0x30, 0x08, 0x00, 0x40, 0x0e, 0x00, 0x80, 0x81, 0x00, 0x81, 0x00, 0x80,
    0x43, 0x00, 0x80, 0x26, 0x3f, 0x00, 0x0f, 0x80, 0x00, 0x01, 0x80, 0x00,
    0x03, 0x00, 0x00, 0x02, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM image_weather_cloud_rain_bits[] = {
    0x00, 0x00, 0x00, 0x07, 0xc0, 0x00, 0x08, 0x20, 0x00, 0x10, 0x10, 0x00,
    0x30, 0x08, 0x00, 0x40, 0x0e, 0x00, 0x80, 0x01, 0x00, 0x80, 0x00, 0x80,
    0x40, 0x00, 0x80, 0x3f, 0xff, 0x00, 0x01, 0x10, 0x00, 0x22, 0x22, 0x00,
    0x44, 0x84, 0x00, 0x91, 0x28, 0x00, 0x22, 0x40, 0x00, 0x00, 0x80, 0x00};

static const unsigned char PROGMEM image_weather_cloud_snow_bits[] = {
    0x00, 0x00, 0x00, 0x07, 0xc0, 0x00, 0x08, 0x20, 0x00, 0x10, 0x10, 0x00,
    0x30, 0x08, 0x00, 0x40, 0x0e, 0x00, 0x80, 0x01, 0x00, 0x80, 0x00, 0x80,
    0x40, 0x00, 0x80, 0x3f, 0xff, 0x00, 0x00, 0x00, 0x00, 0x09, 0x4a, 0x00,
    0x20, 0x00, 0x00, 0x08, 0x90, 0x00, 0x22, 0x44, 0x00, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM image_weather_cloud_sunny_bits[] = {
    0x00, 0x20, 0x00, 0x02, 0x02, 0x00, 0x00, 0x70, 0x00, 0x01, 0x8c, 0x00,
    0x09, 0x04, 0x80, 0x02, 0x02, 0x00, 0x02, 0x02, 0x00, 0x07, 0x82, 0x00,
    0x08, 0x44, 0x80, 0x10, 0x2c, 0x00, 0x30, 0x30, 0x00, 0x60, 0x1e, 0x00,
    0x80, 0x03, 0x00, 0x80, 0x01, 0x00, 0x80, 0x01, 0x00, 0x7f, 0xfe, 0x00};

static const unsigned char PROGMEM image_weather_frost_bits[] = {
    0x01, 0x00, 0x13, 0x90, 0x31, 0x18, 0x73, 0x9c, 0x09, 0x20, 0x05,
    0x40, 0x53, 0x94, 0xfe, 0xfe, 0x53, 0x94, 0x05, 0x40, 0x09, 0x20,
    0x73, 0x9c, 0x31, 0x18, 0x13, 0x90, 0x01, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM image_weather_humidity_bits[] = {
    0x04, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x0e, 0x00, 0x1e, 0x00, 0x1f,
    0x00, 0x3f, 0x80, 0x3f, 0x80, 0x7e, 0xc0, 0x7f, 0x40, 0xff, 0x60,
    0xff, 0xe0, 0x7f, 0xc0, 0x7f, 0xc0, 0x3f, 0x80, 0x0f, 0x00};

static const unsigned char PROGMEM image_weather_sun_bits[] = {
    0x01, 0x00, 0x21, 0x08, 0x10, 0x10, 0x03, 0x80, 0x8c, 0x62, 0x48,
    0x24, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x48, 0x24, 0x8c, 0x62,
    0x03, 0x80, 0x10, 0x10, 0x21, 0x08, 0x01, 0x00, 0x00, 0x00};

static const unsigned char PROGMEM image_weather_temperature_bits[] = {
    0x1c, 0x00, 0x22, 0x02, 0x2b, 0x05, 0x2a, 0x02, 0x2b, 0x38, 0x2a,
    0x60, 0x2b, 0x40, 0x2a, 0x40, 0x2a, 0x60, 0x49, 0x38, 0x9c, 0x80,
    0xae, 0x80, 0xbe, 0x80, 0x9c, 0x80, 0x41, 0x00, 0x3e, 0x00};

// calendar icon
static const unsigned char PROGMEM image_date_day_empty_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc3, 0x0c, 0x00,
    0x00, 0xc3, 0x0c, 0x00, 0x3f, 0xff, 0xff, 0xf0, 0x3f, 0xff, 0xff, 0xf0,
    0xf0, 0xc3, 0x0c, 0x3c, 0xf0, 0xc3, 0x0c, 0x3c, 0xc0, 0x00, 0x00, 0x0c,
    0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c,
    0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c,
    0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c,
    0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c,
    0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c,
    0xc0, 0x00, 0x00, 0x0c, 0xc0, 0x00, 0x00, 0x0c, 0xf0, 0x00, 0x00, 0x3c,
    0xf0, 0x00, 0x00, 0x3c, 0x3f, 0xff, 0xff, 0xf0, 0x3f, 0xff, 0xff, 0xf0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// radio button icons for language selection
static const unsigned char PROGMEM image_choice_bullet_off_bits[] = {
    0x07, 0xc0, 0x1c, 0x70, 0x30, 0x18, 0x60, 0x0c, 0x40, 0x04, 0xc0,
    0x06, 0x80, 0x02, 0x80, 0x02, 0x80, 0x02, 0xc0, 0x06, 0x40, 0x04,
    0x60, 0x0c, 0x30, 0x18, 0x1c, 0x70, 0x07, 0xc0, 0x00, 0x00};
static const unsigned char PROGMEM image_choice_bullet_on_bits[] = {
    0x07, 0xc0, 0x1c, 0x70, 0x33, 0x98, 0x6f, 0xec, 0x5f, 0xf4, 0xdf,
    0xf6, 0xbf, 0xfa, 0xbf, 0xfa, 0xbf, 0xfa, 0xdf, 0xf6, 0x5f, 0xf4,
    0x6f, 0xec, 0x33, 0x98, 0x1c, 0x70, 0x07, 0xc0, 0x00, 0x00};

// forward declarations
void setupCamera();
void startCameraServer();
void setupI2SSpeaker();
void setupI2SMic();
void cleanupI2SSpeaker();
void cleanupI2SMic();
bool playTTS(const String &text);
void showMessage(const char *msg, uint16_t color = ST77XX_WHITE);
void showDHT();
void speakDHT();
void showObjectDetection();
void showColourRecognition();
void showGesture();
void cleanupCamera();
void cleanupSTT();
void runSTT();
void updateSTTDisplay();
bool connectSTT();
void showSTT();
void showClock();
void syncTime();
void showLanguageSelect();
void sendLanguageToServer();
void startColourServer();
void runColourServer();
void stopColourServer();
void updateColourDisplay();
void startObjectServer();
void runObjectServer();
void stopObjectServer();
void updateObjectDisplay();
void startGestureServer();
void runGestureServer();
void stopGestureServer();
void updateGestureDisplay();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(3);
  tft.fillScreen(darkGreen);

  tft.setTextColor(0x0);
  tft.setTextSize(2);
  tft.setTextWrap(false);
  tft.setCursor(13, 41);
  tft.print("INITILAZING");

  tft.setCursor(35, 66);
  tft.print("DISPLAY");

  tft.drawBitmap(135, 2, image_wifi_not_connected_bits, 19, 16, 0x0);

  tft.drawBitmap(93, 2, image_microphone_muted_bits, 15, 16, 0x0);

  tft.drawBitmap(115, 94, image_Release_arrow_bits, 36, 30, 0x0);

  tft.drawBitmap(115, 2, image_volume_no_sound_bits, 18, 16, 0x0);

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(300);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Set the camera IP to the actual ESP32 IP address
    CAM_IP = WiFi.localIP().toString();

    // Print ESP32 IP - USE THIS IN YOUR PYTHON SCRIPTS!
    // Print ESP32 IP
    Serial.print("ESP32 Connected! IP Address: ");
    Serial.println(CAM_IP);

    setupI2SSpeaker();
    dht.begin();

    // Configure NTP time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    timeConfigured = true;

    splashStart = millis();

  } else {
    showMessage("WiFi Failed", ST77XX_RED);
    delay(3000);
    ESP.restart();
  }
}

void loop() {
  // language select handling (long press)
  if (screen == SCREEN_LANGUAGE_SELECT) {
    bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);

    if (buttonPressed && !buttonWasPressed) {
      // Button just pressed
      buttonPressStart = millis();
      buttonWasPressed = true;
    } else if (buttonPressed && buttonWasPressed) {
      // Button held - check for long press (3 seconds)
      if (millis() - buttonPressStart >= 3000) {
        // Long press - confirm selection
        languageSelected = true;
        sendLanguageToServer();
        playTTS("Language selected");
        delay(500);
        screen = SCREEN_CLOCK;
        buttonWasPressed = false;
      }
    } else if (!buttonPressed && buttonWasPressed) {
      // Button released - short press cycles language
      if (millis() - buttonPressStart < 3000) {
        selectedLanguage = (selectedLanguage + 1) % numLanguages;
        showLanguageSelect();
      }
      buttonWasPressed = false;
    }

    // Show language select screen if just entered
    if (prevScreen != SCREEN_LANGUAGE_SELECT) {
      showLanguageSelect();
      playTTS("Select your language");
      prevScreen = SCREEN_LANGUAGE_SELECT;
    }
    return;
  }

  // normal debounced button for other screens
  if (millis() - lastButtonCheck > BUTTON_DEBOUNCE) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      lastButtonCheck = millis();

      // Cleanup previous screen
      switch (screen) {
      case SCREEN_STT:
        cleanupSTT();
        break;
      case SCREEN_COLOUR_RECOGNITION:
        stopColourServer();
        break;
      case SCREEN_GESTURE:
        stopGestureServer();
        break;
      default:
        break;
      }

      // Cycle to next screen
      switch (screen) {
      case SCREEN_CLOCK:
        screen = SCREEN_DHT;
        break;

      case SCREEN_DHT:
        screen = SCREEN_OBJECT_DETECTION;
        setupCamera();
        startCameraServer();
        startObjectServer();
        break;

      case SCREEN_OBJECT_DETECTION:
        stopObjectServer();
        screen = SCREEN_COLOUR_RECOGNITION;
        startColourServer();
        break;

      case SCREEN_COLOUR_RECOGNITION:
        stopColourServer();
        screen = SCREEN_GESTURE;
        startGestureServer();
        break;

      case SCREEN_GESTURE:
        stopGestureServer();
        screen = SCREEN_STT;
        cleanupCamera();
        cleanupI2SSpeaker();
        setupI2SMic();
        showSTT(); // Show UI immediately
        connectSTT();
        break;

      case SCREEN_STT:
        screen = SCREEN_CLOCK;
        cleanupI2SMic();
        setupI2SSpeaker();
        break;

      default:
        screen = SCREEN_CLOCK;
        break;
      }
    }
  }

  switch (screen) {

  case SCREEN_VISORA:
    if (!visoraDone) {
      tft.fillScreen(darkGreen);
      tft.setTextSize(1);

      tft.drawBitmap(92, 2, image_microphone_muted_bits, 15, 16, 0x0);
      tft.drawBitmap(111, 2, image_volume_loud_bits, 20, 16, 0x0);
      tft.drawBitmap(135, 2, image_wifi_full_bits, 19, 16, 0x0);

      tft.setTextColor(0x0);
      tft.setTextWrap(false);
      tft.setFont(&FreeMonoBold18pt7b);
      tft.setCursor(18, 54);
      tft.print("VISORA");

      tft.drawBitmap(41, 63, image_images__1__1_bits, 80, 45, 0x0);

      playTTS("Visora");
      visoraDone = true;
    }
    screen = SCREEN_LANGUAGE_SELECT; // Go to language select after VISORA
    break;

  case SCREEN_CLOCK:
    if (millis() - lastClockUpdate > 1000) {
      lastClockUpdate = millis();
      showClock();
    }
    if (prevScreen != SCREEN_CLOCK) {
      playTTS("Clock mode");
      prevScreen = SCREEN_CLOCK;
    }
    break;

  case SCREEN_DHT:
    if (millis() - lastDHTDraw > 2000) {
      lastDHTDraw = millis();
      showDHT();
    }

    if (prevScreen != SCREEN_DHT) {
      speakDHT();
      prevScreen = SCREEN_DHT;
    }

    break;

  case SCREEN_INIT:
    if (!splashDone) {
      if (millis() - splashStart >= 5000) {
        splashDone = true;
        screen = SCREEN_VISORA;
      }
    }
    break;

  case SCREEN_OBJECT_DETECTION:
    if (prevScreen != SCREEN_OBJECT_DETECTION) {
      showObjectDetection();
      prevScreen = SCREEN_OBJECT_DETECTION;
    }
    runObjectServer(); // Handle incoming object detection data
    break;

  case SCREEN_COLOUR_RECOGNITION:
    if (prevScreen != SCREEN_COLOUR_RECOGNITION) {
      showColourRecognition();
      prevScreen = SCREEN_COLOUR_RECOGNITION;
    }
    runColourServer(); // Handle incoming colour data
    break;

  case SCREEN_GESTURE:
    if (prevScreen != SCREEN_GESTURE) {
      showGesture();
      prevScreen = SCREEN_GESTURE;
    }
    runGestureServer();
    break;

  case SCREEN_STT:
    runSTT();
    break;
  }
}

void showMessage(const char *msg, uint16_t color) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setFont(NULL);  // Reset to default font
  tft.setTextSize(1); // Reset to base size first
  tft.setTextColor(color);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.println(msg);
}

void showDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  tft.fillScreen(darkGreen);
  tft.setFont(NULL);  // Reset to default font first
  tft.setTextSize(1); // Reset text size

  // Status bar icons
  tft.drawBitmap(92, 2, image_microphone_muted_bits, 15, 16, 0x0);
  tft.drawBitmap(111, 2, image_volume_loud_bits, 20, 16, 0x0);
  tft.drawBitmap(135, 2, image_wifi_full_bits, 19, 16, 0x0);

  // DHT 11 header
  tft.setTextColor(0x0);
  tft.setTextWrap(false);
  tft.setFont(&FreeMonoBold18pt7b);
  tft.setCursor(0, 22);
  tft.print("DHT");
  tft.setCursor(11, 50);
  tft.print("11");

  // Temperature label
  tft.setFont(&FreeSansBold9pt7b);
  tft.setTextColor(0x0);
  tft.setCursor(57, 46);
  tft.print("Temprature");

  // Temperature icon (white)
  tft.drawBitmap(115, 57, image_weather_temperature_bits, 16, 16, 0xFFFF);

  // Temperature value
  tft.setTextColor(0xFFFF);
  tft.setCursor(72, 67);
  if (isnan(t)) {
    tft.print("ERR");
  } else {
    tft.print((int)t);
    tft.print(" C");
  }

  // Humidity label
  tft.setTextColor(0x0);
  tft.setCursor(61, 92);
  tft.print("Humidity");

  // Humidity value
  tft.setTextColor(0xFFFF);
  tft.setCursor(73, 115);
  if (isnan(h)) {
    tft.print("ERR");
  } else {
    tft.print((int)h);
    tft.print(" %");
  }

  // Humidity icon (white)
  tft.drawBitmap(118, 102, image_weather_humidity_bits, 11, 16, 0xFFFF);

  // Weather condition icons - determine which one to highlight white based on
  // conditions Default all to black (0x0)
  uint16_t snowColor = 0x0;
  uint16_t cloudSunnyColor = 0x0;
  uint16_t rainColor = 0x0;
  uint16_t frostColor = 0x0;
  uint16_t lightningColor = 0x0;
  uint16_t sunColor = 0x0;

  if (!isnan(t) && !isnan(h)) {
    // Determine weather condition based on temp and humidity
    if (t < 0) {
      snowColor = 0xFFFF; // Snow for freezing temps
    } else if (t < 5) {
      frostColor = 0xFFFF; // Frost for very cold
    } else if (h >= 85) {
      lightningColor = 0xFFFF; // Storm/lightning for very humid
    } else if (h >= 70) {
      rainColor = 0xFFFF; // Rain for high humidity
    } else if (t >= 30 && h < 50) {
      sunColor = 0xFFFF; // Sunny for hot and dry
    } else {
      cloudSunnyColor = 0xFFFF; // Partly cloudy for moderate conditions
    }
  }

  // Draw weather icons (6 icons in 2 rows of 3)
  tft.drawBitmap(6, 59, image_weather_cloud_snow_bits, 17, 16, snowColor);
  tft.drawBitmap(35, 59, image_weather_cloud_sunny_bits, 17, 16,
                 cloudSunnyColor);
  tft.drawBitmap(6, 85, image_weather_cloud_rain_bits, 17, 16, rainColor);
  tft.drawBitmap(36, 85, image_weather_frost_bits, 15, 16, frostColor);
  tft.drawBitmap(6, 109, image_weather_cloud_lightning_bolt_bits, 17, 16,
                 lightningColor);
  tft.drawBitmap(36, 109, image_weather_sun_bits, 15, 16, sunColor);
}

void speakDHT() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    playTTS("Sensor error");
    return;
  }

  String msg = "Temperature is ";
  msg += String((int)t);
  msg += " degrees. Humidity is ";
  msg += String((int)h);
  msg += " percent.";

  playTTS(msg);
}

void showObjectDetection() {
  tft.fillScreen(darkGreen);
  tft.setFont(NULL);
  tft.setTextSize(1);

  tft.drawBitmap(92, 2, image_microphone_muted_bits, 15, 16, 0x0);
  tft.drawBitmap(135, 2, image_wifi_full_bits, 19, 16, 0x0);
  tft.drawBitmap(69, 2, image_camera_bits, 17, 16, 0x0);
  tft.drawBitmap(111, 2, image_volume_loud_bits, 20, 16, 0x0);

  tft.setTextColor(0x0);
  tft.setTextWrap(false);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(2, 15);
  tft.print("Object ");

  tft.setCursor(2, 36);
  tft.print("Detection");

  tft.setFont(); // Default font
  tft.setCursor(102, 22);
  tft.print("Camera");

  tft.setCursor(94, 31);
  tft.print("Streaming");

  tft.drawBitmap(96, 43, image_arrow_curved_right_up_down_bits, 7, 5, 0x0);

  tft.setTextColor(0x03E0);
  tft.setCursor(106, 42);
  tft.print("Enabled");

  // Camera stream IP display
  tft.setTextColor(0x0);
  tft.setCursor(5, 53);
  tft.print("CAM: ");
  tft.print(CAM_IP);
  tft.setCursor(5, 63);
  tft.print("Port: ");
  tft.print(CAM_PORT);

  // Draw box for detected objects
  tft.fillRoundRect(4, 75, 152, 48, 4, 0xFFFF);
  tft.setTextColor(0x0);
  tft.setCursor(8, 80);
  tft.print("Detected Objects:");

  // Initialize with "Waiting..."
  if (detectedObjectCount == 0) {
    tft.setTextColor(0x8410); // Gray
    tft.setCursor(40, 98);
    tft.print("Waiting...");
  }

  playTTS("Object detection camera enabled");
}

void showColourRecognition() {
  tft.fillScreen(darkGreen);
  tft.setFont(NULL);
  tft.setTextSize(1);

  // Status bar icons
  tft.drawBitmap(92, 2, image_microphone_muted_bits, 15, 16, 0x0);
  tft.drawBitmap(135, 2, image_wifi_full_bits, 19, 16, 0x0);
  tft.drawBitmap(69, 2, image_camera_bits, 17, 16, 0x0);
  tft.drawBitmap(111, 2, image_volume_loud_bits, 20, 16, 0x0);

  // Header
  tft.setTextColor(0x0);
  tft.setTextWrap(false);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(2, 15);
  tft.print("Colour");
  tft.setCursor(2, 36);
  tft.print("Recognition");

  // Camera stream IP display
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(0x0);
  tft.setCursor(5, 118);
  tft.print("CAM: ");
  tft.print(CAM_IP);
  tft.print(":");
  tft.print(CAM_PORT);

  // Show waiting message initially
  tft.setCursor(10, 55);
  tft.print("Waiting for data...");

  // Draw placeholder color box
  tft.drawRect(10, 70, 140, 40, 0x0);

  playTTS("Colour recognition mode. Point your finger at a color.");
}

void showGesture() {
  tft.fillScreen(darkGreen);
  tft.setFont(NULL);
  tft.setTextSize(1);

  tft.drawBitmap(92, 2, image_microphone_muted_bits, 15, 16, 0x0);
  tft.drawBitmap(135, 2, image_wifi_full_bits, 19, 16, 0x0);
  tft.drawBitmap(69, 2, image_camera_bits, 17, 16, 0x0);
  tft.drawBitmap(111, 2, image_volume_loud_bits, 20, 16, 0x0);

  tft.setTextColor(0x0);
  tft.setTextWrap(false);
  tft.setFont(&FreeSans9pt7b);
  tft.setCursor(0, 16);
  tft.print("Gesture");

  tft.setCursor(1, 36);
  tft.print("To Speech");

  tft.setFont();
  tft.setCursor(107, 21);
  tft.print("Camera");

  tft.setCursor(99, 31);
  tft.print("Streaming");

  tft.drawBitmap(103, 42, image_arrow_curved_right_up_down_bits, 7, 5, 0x0);

  tft.setTextColor(0x03E0);
  tft.setCursor(114, 41);
  tft.print("Enabled");

  // Camera stream IP display
  tft.setTextColor(0x0);
  tft.setCursor(5, 55);
  tft.print("Use Laptop Camera");
  tft.setCursor(5, 65);
  tft.print("Waiting for data...");

  playTTS("Gesture to speech camera enabled");
}

void showClock() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    tft.fillScreen(darkGreen);
    tft.setFont(NULL);
    tft.setTextColor(0x0);
    tft.setTextSize(2);
    tft.setCursor(10, 50);
    tft.print("Time Sync...");
    return;
  }

  tft.fillScreen(darkGreen);
  tft.setFont(NULL);
  tft.setTextSize(1);

  // Status bar icons
  tft.drawBitmap(91, 2, image_microphone_muted_bits, 15, 16, 0x0);
  tft.drawBitmap(111, 2, image_volume_loud_bits, 20, 16, 0x0);
  tft.drawBitmap(135, 2, image_wifi_full_bits, 19, 16, 0x0);

  // Header - CLOCK
  tft.setTextColor(0x0);
  tft.setTextWrap(false);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setCursor(1, 19);
  tft.print("CLOCK");

  // Day of week with comma
  const char *days[] = {"Sunday,",   "Monday,", "Tuesday,", "Wednesday,",
                        "Thursday,", "Friday,", "Saturday,"};
  const char *months[] = {"January",   "February", "March",    "April",
                          "May",       "June",     "July",     "August",
                          "September", "October",  "November", "December"};

  tft.setFont(&FreeSansBold9pt7b);
  tft.setTextColor(0x0);
  tft.setCursor(4, 44);
  tft.print(days[timeinfo.tm_wday]);

  // Calendar icon
  tft.drawBitmap(5, 50, image_date_day_empty_bits, 30, 32, 0x0);

  // Day number inside calendar
  tft.setCursor(9, 73);
  char dayNum[3];
  sprintf(dayNum, "%02d", timeinfo.tm_mday);
  tft.print(dayNum);

  // Month and year
  tft.setCursor(39, 72);
  char monthYear[20];
  sprintf(monthYear, "%s %04d", months[timeinfo.tm_mon],
          timeinfo.tm_year + 1900);
  tft.print(monthYear);

  // Time - Hours and Minutes
  tft.setFont(&FreeSansBold12pt7b);
  tft.setTextColor(0x0);
  int hour12 = timeinfo.tm_hour % 12;
  if (hour12 == 0)
    hour12 = 12;
  char timeStr[15];
  sprintf(timeStr, "%02d : %02d :", hour12, timeinfo.tm_min);
  tft.setCursor(2, 106);
  tft.print(timeStr);

  // Seconds
  tft.setFont(&FreeSansBold9pt7b);
  tft.setCursor(93, 110);
  char secStr[3];
  sprintf(secStr, "%02d", timeinfo.tm_sec);
  tft.print(secStr);

  // AM/PM - Active one is black, inactive one is white
  bool isPM = (timeinfo.tm_hour >= 12);

  // AM label
  tft.setTextColor(isPM ? 0xFFFF : 0x0); // White if PM, Black if AM
  tft.setCursor(126, 118);
  tft.print("AM");

  // PM label
  tft.setTextColor(isPM ? 0x0 : 0xFFFF); // Black if PM, White if AM
  tft.setCursor(127, 96);
  tft.print("PM");
}

void showLanguageSelect() {
  tft.fillScreen(darkGreen);
  tft.setFont(NULL);
  tft.setTextSize(1);

  // Status bar icons
  tft.drawBitmap(91, 2, image_microphone_muted_bits, 15, 16, 0x0);
  tft.drawBitmap(111, 2, image_volume_loud_bits, 20, 16, 0x0);
  tft.drawBitmap(135, 2, image_wifi_full_bits, 19, 16, 0x0);

  // Header
  tft.setTextColor(0x0);
  tft.setTextWrap(false);
  tft.setFont(&FreeSansBold12pt7b);
  tft.setCursor(4, 19);
  tft.print("Select");
  tft.setCursor(-3, 39);
  tft.print(" Language ");

  // Language options
  tft.setFont(&FreeMonoBold9pt7b);

  // ENGLISH
  tft.setCursor(6, 59);
  tft.print("ENGLISH");
  tft.drawBitmap(90, 48,
                 selectedLanguage == 0 ? image_choice_bullet_on_bits
                                       : image_choice_bullet_off_bits,
                 15, 16, 0x0);

  // HINDI
  tft.setCursor(6, 76);
  tft.print("HINDI");
  tft.drawBitmap(90, 64,
                 selectedLanguage == 1 ? image_choice_bullet_on_bits
                                       : image_choice_bullet_off_bits,
                 15, 16, 0x0);

  // PUNJABI
  tft.setCursor(6, 92);
  tft.print("PUNJABI");
  tft.drawBitmap(90, 80,
                 selectedLanguage == 2 ? image_choice_bullet_on_bits
                                       : image_choice_bullet_off_bits,
                 15, 16, 0x0);

  // TAMIL
  tft.setCursor(6, 108);
  tft.print("TAMIL");
  tft.drawBitmap(90, 96,
                 selectedLanguage == 3 ? image_choice_bullet_on_bits
                                       : image_choice_bullet_off_bits,
                 15, 16, 0x0);

  // MARATHI
  tft.setCursor(7, 124);
  tft.print("MARATHI");
  tft.drawBitmap(90, 112,
                 selectedLanguage == 4 ? image_choice_bullet_on_bits
                                       : image_choice_bullet_off_bits,
                 15, 16, 0x0);
}

void sendLanguageToServer() {
  // tell the server which language to use
  WiFiClient client;
  if (client.connect(TTS_HOST, TTS_PORT)) {
    String langCmd = "LANG:";
    langCmd += languages[selectedLanguage];
    langCmd += "\n";
    client.print(langCmd);
    client.flush();

    // Wait for server acknowledgment (4-byte zero response)
    uint32_t startWait = millis();
    while (!client.available() && (millis() - startWait < 2000)) {
      delay(10);
    }

    // Read the acknowledgment (should be 4 bytes with value 0)
    if (client.available() >= 4) {
      uint8_t ack[4];
      client.readBytes(ack, 4);
    }

    client.stop();
    Serial.print("Language sent: ");
    Serial.println(languages[selectedLanguage]);
  } else {
    Serial.println("Failed to send language to server");
  }
}

void setupCamera() {
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;
  c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM;
  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;
  c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM;
  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  c.pin_sccb_sda = SIOD_GPIO_NUM;
  c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;

  c.xclk_freq_hz = 20000000;
  c.pixel_format = PIXFORMAT_JPEG;

  // Higher resolution for better quality
  // Options: FRAMESIZE_QVGA (320x240), FRAMESIZE_VGA (640x480),
  //          FRAMESIZE_SVGA (800x600), FRAMESIZE_XGA (1024x768),
  //          FRAMESIZE_HD (1280x720), FRAMESIZE_SXGA (1280x1024)
  c.frame_size = FRAMESIZE_XGA; // Upgraded from SVGA to XGA (1024x768)

  // JPEG quality: 0-63, lower = better quality (4-10 is high quality)
  c.jpeg_quality = 8; // Improved from 12 to 8 for better quality

  c.fb_count = 2; // Double buffer for smoother streaming
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.grab_mode = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&c) == ESP_OK) {
    Serial.println("Camera OK - XGA 1024x768, Quality: 8");

    // Additional camera sensor settings for better image
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
      s->set_vflip(s, 1);         // Vertical flip
      s->set_hmirror(s, 0);       // No horizontal mirror
      s->set_brightness(s, 1);    // Slightly brighter (-2 to 2)
      s->set_contrast(s, 1);      // Slightly more contrast (-2 to 2)
      s->set_saturation(s, 0);    // Normal saturation (-2 to 2)
      s->set_sharpness(s, 1);     // Slightly sharper (-2 to 2)
      s->set_denoise(s, 1);       // Enable noise reduction
      s->set_whitebal(s, 1);      // Enable auto white balance
      s->set_awb_gain(s, 1);      // Enable AWB gain
      s->set_exposure_ctrl(s, 1); // Enable auto exposure
      s->set_aec2(s, 1);          // Enable AEC DSP
      s->set_gain_ctrl(s, 1);     // Enable auto gain
      s->set_bpc(s, 1);           // Enable black pixel correction
      s->set_wpc(s, 1);           // Enable white pixel correction
    }
  } else {
    Serial.println("Camera init failed!");
  }
}

void cleanupCamera() {
  esp_camera_deinit();
  Serial.println("camera off");
}

void setupI2SSpeaker() {
  i2s_config_t cfg = {.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
                      .sample_rate = SAMPLE_RATE_SPEAKER,
                      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
                      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
                      .communication_format =
                          (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
                      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
                      .dma_buf_count = 8,
                      .dma_buf_len = 512,
                      .use_apll = false,
                      .tx_desc_auto_clear = true,
                      .fixed_mclk = 0};

  i2s_pin_config_t p = {.bck_io_num = I2S_BCLK,
                        .ws_io_num = I2S_LRC,
                        .data_out_num = I2S_DOUT,
                        .data_in_num = I2S_PIN_NO_CHANGE};

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &p);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void cleanupI2SSpeaker() { i2s_driver_uninstall(I2S_NUM_0); }

void setupI2SMic() {
  i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
      .sample_rate = MIC_SAMPLE_RATE,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 512,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0};

  i2s_pin_config_t p = {.bck_io_num = I2S_PIN_NO_CHANGE,
                        .ws_io_num = I2S_MIC_SERIAL_CLOCK,
                        .data_out_num = I2S_PIN_NO_CHANGE,
                        .data_in_num = I2S_MIC_LEFT_RIGHT_CLOCK};

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &p);
  i2s_zero_dma_buffer(I2S_NUM_0);

  Serial.println("Mic ready");
}

void cleanupI2SMic() { i2s_driver_uninstall(I2S_NUM_0); }

bool playTTS(const String &text) {
  WiFiClient client;

  if (!client.connect(TTS_HOST, TTS_PORT)) {
    Serial.println("TTS connect failed");
    return false;
  }

  client.print(text);
  client.print("\n");
  client.flush();

  uint32_t startWait = millis();
  while (!client.available()) {
    if (!client.connected() || millis() - startWait > 10000) {
      client.stop();
      return false;
    }
    delay(5);
  }

  uint8_t lenBuf[4];
  if (client.readBytes(lenBuf, 4) != 4) {
    client.stop();
    return false;
  }

  uint32_t pcmLen = (uint32_t)lenBuf[0] | ((uint32_t)lenBuf[1] << 8) |
                    ((uint32_t)lenBuf[2] << 16) | ((uint32_t)lenBuf[3] << 24);

  if (pcmLen == 0 || pcmLen > 5000000) {
    client.stop();
    return false;
  }

  uint8_t buffer[512];
  int16_t amplifiedBuffer[256]; // Buffer for amplified samples
  uint32_t received = 0;

  while (received < pcmLen) {
    if (!client.connected())
      break;

    int avail = client.available();
    if (avail == 0) {
      delay(1);
      continue;
    }

    int toRead = min((int)sizeof(buffer), avail);
    int len = client.read(buffer, toRead);
    if (len <= 0)
      break;

    // Amplify audio samples for louder volume
    int sampleCount = len / 2; // 16-bit samples
    int16_t *samples = (int16_t *)buffer;

    for (int i = 0; i < sampleCount; i++) {
      // Amplify with VOLUME_GAIN and clip to prevent overflow
      int32_t amplified = (int32_t)(samples[i] * VOLUME_GAIN);
      if (amplified > 32767)
        amplified = 32767;
      if (amplified < -32768)
        amplified = -32768;
      amplifiedBuffer[i] = (int16_t)amplified;
    }

    size_t written;
    i2s_write(I2S_NUM_0, amplifiedBuffer, len, &written, portMAX_DELAY);
    received += len;
  }

  delay(50);
  client.stop();
  return true;
}

bool connectSTT() {
  Serial.println("Connecting STT...");
  if (sttClient.connect(STT_HOST, STT_PORT)) {
    Serial.println("STT connected");
    transcriptBuffer = "";
    sttLineCount = 0; // Reset scrolling text buffer
    // showSTT(); // UI is now handled in loop or refresh
    return true;
  }
  return false;
}

void showSTT() {
  tft.fillScreen(darkGreen);
  tft.setFont(NULL);
  tft.setTextSize(1);

  tft.drawBitmap(135, 2, image_wifi_full_bits, 19, 16, 0x0);
  tft.drawBitmap(91, 2, image_microphone_recording_bits, 15, 16, 0x0);
  tft.drawBitmap(113, 2, image_volume_no_sound_bits, 18, 16, 0x0);

  tft.setTextColor(0x0);
  tft.setTextWrap(false);
  tft.setFont(&FreeMonoBold9pt7b);
  tft.setCursor(0, 13);
  tft.print("Live");

  tft.setFont();
  tft.setCursor(65, 21);
  tft.print("(Speech To Text)");

  tft.drawBitmap(55, 20, image_arrow_curved_right_up_down_bits, 7, 5, 0x0);

  tft.setFont(&FreeMonoBold9pt7b);
  tft.setCursor(48, 13);
  tft.print("STT");

  tft.drawBitmap(4, 18, image_toilets_gentlemen_bits, 7, 16, 0x0);
  tft.drawBitmap(40, 18, image_toilets_ladies_bits, 9, 16, 0x0);
  tft.drawBitmap(17, 19, image_phone_call_in_out_bits, 15, 16, 0x0);

  // Rounded white box for transcript
  tft.fillRoundRect(4, 40, 152, 83, 6, 0xFFFF);
}

void runSTT() {
  if (!sttClient.connected()) {
    if (!connectSTT()) {
      delay(100); // Reduced delay for faster button response
      return;
    }
  }

  // Stream audio
  int16_t buffer[MIC_BUFFER_SAMPLES];
  size_t bytesRead = 0;

  i2s_read(I2S_NUM_0, buffer, MIC_BUFFER_SAMPLES * sizeof(int16_t), &bytesRead,
           50 / portTICK_PERIOD_MS // Reduced timeout for faster button response
  );

  if (bytesRead > 0) {
    sttClient.write((uint8_t *)buffer, bytesRead);
  }

  // Check for transcription - limit iterations to allow button check
  int maxReads = 100;
  while (sttClient.available() && maxReads-- > 0) {
    char c = sttClient.read();
    if (c == '\n') {
      updateSTTDisplay();
      transcriptBuffer = "";
    } else {
      transcriptBuffer += c;
    }
  }
}

void updateSTTDisplay() {
  if (transcriptBuffer.length() == 0)
    return;

  const int maxLines = 7; // Max lines that fit in display area

  // Add new line to buffer
  if (sttLineCount < maxLines) {
    sttLines[sttLineCount] = transcriptBuffer;
    sttLineCount++;
  } else {
    // Scroll: shift all lines up (older text moves up, new text at bottom)
    for (int i = 0; i < maxLines - 1; i++) {
      sttLines[i] = sttLines[i + 1];
    }
    sttLines[maxLines - 1] = transcriptBuffer;
  }

  // Clear the text box
  tft.fillRoundRect(4, 40, 152, 83, 6, 0xFFFF);

  // Redraw all lines (from top to bottom, oldest at top)
  tft.setTextColor(ST77XX_BLACK);
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextWrap(false);

  int lineHeight = 11;
  int startY = 45;

  // Display lines from top (oldest) to bottom (newest)
  for (int i = 0; i < sttLineCount; i++) {
    int yPos = startY + (i * lineHeight);
    if (yPos > 115)
      break; // Don't draw beyond the box
    tft.setCursor(8, yPos);
    // Truncate long lines to fit in display width (about 24 chars at size 1)
    if (sttLines[i].length() > 24) {
      tft.print(sttLines[i].substring(0, 24));
    } else {
      tft.print(sttLines[i]);
    }
  }

  Serial.print("[STT Display] ");
  Serial.println(transcriptBuffer);
}

void cleanupSTT() {
  if (sttClient.connected()) {
    sttClient.stop();
  }
  transcriptBuffer = "";
  sttLineCount = 0;
  for (int i = 0; i < 7; i++) {
    sttLines[i] = "";
  }
  Serial.println("STT cleaned");
}

void startColourServer() {
  if (!colourServerStarted) {
    colourServer.begin();
    colourServerStarted = true;
    colorDataReceived = false;
    detectedColorName = "";
    colorR = colorG = colorB = 0;
    Serial.print("Colour TCP server started on port ");
    Serial.println(COLOUR_SERVER_PORT);
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  }
}

void stopColourServer() {
  if (colourClient.connected()) {
    colourClient.stop();
  }
  if (colourServerStarted) {
    colourServer.end();
    colourServerStarted = false;
    colorDataReceived = false;
    Serial.println("Colour TCP server stopped");
  }
}

void runColourServer() {
  // Check for new client connection
  if (!colourClient || !colourClient.connected()) {
    WiFiClient newClient = colourServer.available();
    if (newClient) {
      colourClient = newClient;
      Serial.println("Colour client connected");
    }
  }

  // Handle connected client
  if (colourClient && colourClient.connected() && colourClient.available()) {
    String line = colourClient.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
      Serial.print("Color Received: ");
      Serial.println(line);

      // Parse the color data
      // Format: "Color: ColorName | RGB(R,G,B) | Ray: ..."
      int colorIdx = line.indexOf("Color: ");
      int rgbIdx = line.indexOf("RGB(");

      if (colorIdx >= 0 && rgbIdx >= 0) {
        // Extract color name
        int pipeIdx = line.indexOf(" |", colorIdx);
        if (pipeIdx > colorIdx + 7) {
          detectedColorName = line.substring(colorIdx + 7, pipeIdx);
          detectedColorName.trim();
        }

        // Extract RGB values
        int rgbEnd = line.indexOf(")", rgbIdx);
        if (rgbEnd > rgbIdx + 4) {
          String rgbStr = line.substring(rgbIdx + 4, rgbEnd);
          // Parse R,G,B
          int comma1 = rgbStr.indexOf(",");
          int comma2 = rgbStr.indexOf(",", comma1 + 1);
          if (comma1 > 0 && comma2 > comma1) {
            colorR = rgbStr.substring(0, comma1).toInt();
            colorG = rgbStr.substring(comma1 + 1, comma2).toInt();
            colorB = rgbStr.substring(comma2 + 1).toInt();
          }
        }

        colorDataReceived = true;
        lastColorUpdate = millis();

        // Update display with new color
        updateColourDisplay();
      }
    }
  }
}

void updateColourDisplay() {
  if (!colorDataReceived)
    return;

  // Clear the display area below the header
  tft.fillRect(0, 45, 160, 83, darkGreen);

  // Draw status text
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(0x0);
  tft.setCursor(10, 48);
  tft.print("Detected Color:");

  // Draw color box with the detected color
  uint16_t displayColor = tft.color565(colorR, colorG, colorB);
  tft.fillRect(10, 60, 140, 40, displayColor);
  tft.drawRect(10, 60, 140, 40, 0x0); // Black border

  // Calculate brightness for text color contrast
  int brightness = (colorR * 299 + colorG * 587 + colorB * 114) / 1000;
  uint16_t textColor = (brightness > 128) ? 0x0000 : 0xFFFF; // Black or White

  // Display color name centered in the box
  tft.setTextColor(textColor);
  tft.setFont(&FreeSansBold9pt7b);

  // Calculate text position for centering
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(detectedColorName.c_str(), 0, 0, &x1, &y1, &w, &h);
  int textX = 10 + (140 - w) / 2;
  int textY = 60 + 25; // Centered vertically in the 40px box

  tft.setCursor(textX, textY);
  tft.print(detectedColorName);

  // Display RGB values below the box
  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(0x0);
  tft.setCursor(30, 108);
  char rgbText[30];
  sprintf(rgbText, "RGB(%d, %d, %d)", colorR, colorG, colorB);
  tft.print(rgbText);
}

void startObjectServer() {
  if (!objectServerStarted) {
    objectDetectionServer.begin();
    objectServerStarted = true;
    detectedObjectCount = 0;
    for (int i = 0; i < 5; i++) {
      detectedObjects[i] = "";
    }
    Serial.print("Object Detection TCP server started on port ");
    Serial.println(OBJECT_DETECTION_PORT);
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  }
}

void stopObjectServer() {
  if (objectClient.connected()) {
    objectClient.stop();
  }
  if (objectServerStarted) {
    objectDetectionServer.end();
    objectServerStarted = false;
    detectedObjectCount = 0;
    Serial.println("Object Detection TCP server stopped");
  }
}

void runObjectServer() {
  // Check for new client connection
  if (!objectClient || !objectClient.connected()) {
    WiFiClient newClient = objectDetectionServer.available();
    if (newClient) {
      objectClient = newClient;
      Serial.println("Object detection client connected");
    }
  }

  // Handle connected client
  if (objectClient && objectClient.connected() && objectClient.available()) {
    String line = objectClient.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
      Serial.print("Object Received: ");
      Serial.println(line);

      // Parse the object data
      // Format: "object1 Position1,object2 Position2,..." or "None"
      if (line == "None") {
        detectedObjectCount = 0;
        for (int i = 0; i < 5; i++) {
          detectedObjects[i] = "";
        }
      } else {
        detectedObjectCount = 0;
        int startIdx = 0;
        int commaIdx;

        while (detectedObjectCount < 5 && startIdx < (int)line.length()) {
          commaIdx = line.indexOf(',', startIdx);
          String objectStr;

          if (commaIdx == -1) {
            objectStr = line.substring(startIdx);
            startIdx = line.length();
          } else {
            objectStr = line.substring(startIdx, commaIdx);
            startIdx = commaIdx + 1;
          }

          objectStr.trim();
          if (objectStr.length() > 0) {
            detectedObjects[detectedObjectCount] = objectStr;
            detectedObjectCount++;
          }
        }
      }

      lastObjectUpdate = millis();
      updateObjectDisplay();
    }
  }
}

void updateObjectDisplay() {
  // Clear the object display area
  tft.fillRoundRect(4, 75, 152, 48, 4, 0xFFFF);

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(0x0);
  tft.setCursor(8, 80);
  tft.print("Detected Objects:");

  if (detectedObjectCount == 0) {
    tft.setTextColor(0x8410); // Gray
    tft.setCursor(40, 98);
    tft.print("None detected");
    return;
  }

  // Display up to 2 objects (limited space)
  int yPos = 92;
  for (int i = 0; i < min(2, detectedObjectCount); i++) {
    tft.setTextColor(0x001F); // Blue for objects
    tft.setCursor(8, yPos);

    // Truncate long names
    String displayStr = detectedObjects[i];
    if (displayStr.length() > 24) {
      displayStr = displayStr.substring(0, 21) + "...";
    }
    tft.print(displayStr);
    yPos += 12;
  }

  // If more objects detected, show count
  if (detectedObjectCount > 2) {
    tft.setTextColor(0x8410); // Gray
    tft.setCursor(100, 80);
    tft.print("+");
    tft.print(detectedObjectCount - 2);
  }
}

void startGestureServer() {
  if (!gestureServerStarted) {
    gestureServer.begin();
    gestureServerStarted = true;
    currentGestureLetter = "";
    gestureSentence = "";

    Serial.print("Gesture TCP server started on port ");
    Serial.println(GESTURE_SERVER_PORT);
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  }
}

void stopGestureServer() {
  if (gestureClient.connected()) {
    gestureClient.stop();
  }
  if (gestureServerStarted) {
    gestureServer.end();
    gestureServerStarted = false;
    currentGestureLetter = "";
    gestureSentence = "";
    Serial.println("Gesture TCP server stopped");
  }
}

void runGestureServer() {
  // Check for new client connection
  if (!gestureClient || !gestureClient.connected()) {
    WiFiClient newClient = gestureServer.available();
    if (newClient) {
      gestureClient = newClient;
      Serial.println("Gesture client connected");
    }
  }

  // Handle connected client
  if (gestureClient && gestureClient.connected() && gestureClient.available()) {
    String line = gestureClient.readStringUntil('\n');
    line.trim();

    if (line.length() > 0 && line.startsWith("GESTURE:")) {
      Serial.print("Gesture Received: ");
      Serial.println(line);

      // Parse format: "GESTURE:Letter|Sentence"
      int pipeIdx = line.indexOf('|');
      if (pipeIdx > 0) {
        currentGestureLetter = line.substring(8, pipeIdx);
        gestureSentence = line.substring(pipeIdx + 1);

        lastGestureUpdate = millis();
        updateGestureDisplay();
      }
    }
  }
}

void updateGestureDisplay() {
  // Clear the display area for gesture info
  tft.fillRect(0, 75, 160, 53, darkGreen); // Clear bottom half

  // Selection Box (Top right)
  // Position: x=110, y=55-75 (small box)
  tft.fillRect(115, 50, 35, 30, 0x0);    // Black box
  tft.drawRect(115, 50, 35, 30, 0xFFFF); // White border

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  tft.setCursor(117, 40);
  tft.print("Char:");

  if (currentGestureLetter.length() > 0) {
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(0xFFFF); // Green
    // Center the character roughly
    tft.setCursor(120, 72);
    tft.print(currentGestureLetter);
  } else {
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(0x8410); // Gray
    tft.setCursor(120, 72);
    tft.print("-");
  }

  // Sentence Box at bottom
  tft.fillRoundRect(2, 85, 156, 40, 4, 0x0);    // Black background
  tft.drawRoundRect(2, 85, 156, 40, 4, 0xFFFF); // White border

  tft.setFont(NULL);
  tft.setTextSize(1);
  tft.setTextColor(0xFFFF);
  tft.setCursor(5, 75);
  tft.print("Sentence:");

  tft.setCursor(6, 90);
  tft.setTextWrap(true);

  // Truncate if too long (simple check)
  String displayStr = gestureSentence;
  if (displayStr.length() > 50) {
    displayStr = "..." + displayStr.substring(displayStr.length() - 47);
  }

  tft.print(displayStr);
}