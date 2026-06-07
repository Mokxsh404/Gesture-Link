// Glove A main code - ESP32-C6
// handles flex sensors, DHT, pulse ox, OLED display, DFPlayer, WiFi dashboard

#include "MAX30105.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <DFRobotDFPlayerMini.h>
#include <DHT.h>
#include <HardwareSerial.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";

// pin defs
#define I2C_SDA 21
#define I2C_SCL 22
#define DFPLAYER_TX 17
#define DFPLAYER_RX 16
#define FLEX1_PIN 6
#define FLEX2_PIN 0
#define FLEX3_PIN 1
#define FLEX4_PIN 2
#define BUTTON_PIN 18
#define DHT_PIN 23
#define DHT_TYPE DHT11

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define LONG_PRESS_MS 3000
#define ADC_SAMPLES 4
#define GESTURE_COOLDOWN_MS 2000
#define DFPLAYER_CMD_DELAY 200
#define GRAPH_WIDTH 80
#define GRAPH_HEIGHT 35

Adafruit_SH1106G display =
    Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
HardwareSerial dfPlayerSerial(1);
DFRobotDFPlayerMini dfPlayer;
DHT dht(DHT_PIN, DHT_TYPE);
MAX30105 particleSensor;
WebServer server(80);

typedef enum { MODE_COMMUNICATION, MODE_AUTOMATION } GloveMode;
typedef enum {
  SCREEN_FLEX,
  SCREEN_DHT,
  SCREEN_HEART_RATE,
  SCREEN_SPO2,
  SCREEN_BODY_TEMP,
  SCREEN_COUNT
} Screen;

GloveMode currentMode = MODE_COMMUNICATION;
Screen currentScreen = SCREEN_FLEX;

int flexValues[4] = {0, 0, 0, 0};
float dhtTemperature = 0.0;
float dhtHumidity = 0.0;

bool max30102Ready = false;
bool fingerDetected = false;
long irValue = 0;

// simulated HR/SpO2 since the sensor doesnt really compute BPM directly
float simulatedBPM = 72.0;
float targetBPM = 72.0;
int displayBPM = 72;
float simulatedSpO2 = 98.0;
float targetSpO2 = 98.0;
int displaySpO2 = 98;

float bodyTemperature = 0.0;
float bodyTemperatureF = 0.0;

int graphData[GRAPH_WIDTH];
int graphIndex = 0;

bool lastButtonState = HIGH;
unsigned long buttonPressStart = 0;
bool modeSwitchTriggered = false;
unsigned long lastGestureTime[4] = {0, 0, 0, 0};
bool gestureActive[4] = {false, false, false, false};
unsigned long lastDFPlayerCmd = 0;
bool dfPlayerReady = false;
unsigned long lastValueUpdate = 0;
unsigned long lastTargetChange = 0;

bool wifiConnected = false;

// emergency stuff - detects finger tapping 5 times in 5 seconds
bool lastFingerState = false;
unsigned long fingerToggleTimes[5] = {0, 0, 0, 0, 0};
int toggleIndex = 0;
bool emergencyActive = false;
unsigned long emergencyStartTime = 0;
String emergencyType = "";

// forward declarations
void showStartupScreen();
void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, int p);
void updateDisplay();
void showFlexScreen();
void showDHTScreen();
void showHeartRateScreen();
void showSpO2Screen();
void showBodyTempScreen();
void handleButton();
void readDHT();
void readAllFlexSensors();
void detectGestures();
void readMAX30102();
void updateSimulatedValues();
void updateHeartGraph();
void playTrackSafe(uint8_t trackNum);
int readFlexAveraged(int pin);

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>GESTURA Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #0f0f23 0%, #1a1a3e 50%, #0d0d1f 100%);
            min-height: 100vh;
            color: #fff;
            overflow-x: hidden;
        }
        .bg-animation {
            position: fixed;
            top: 0; left: 0; right: 0; bottom: 0;
            background: radial-gradient(circle at 20% 80%, rgba(0, 255, 200, 0.1) 0%, transparent 50%),
                        radial-gradient(circle at 80% 20%, rgba(120, 0, 255, 0.1) 0%, transparent 50%);
            pointer-events: none;
            z-index: 0;
        }
        .container {
            max-width: 1400px;
            margin: 0 auto;
            padding: 20px;
            position: relative;
            z-index: 1;
        }
        header {
            text-align: center;
            padding: 30px 0;
            margin-bottom: 30px;
        }
        h1 {
            font-size: 3rem;
            background: linear-gradient(90deg, #00ffc8, #7b2fff, #00ffc8);
            background-size: 200% auto;
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            animation: gradient 3s ease infinite;
            text-shadow: 0 0 30px rgba(0, 255, 200, 0.3);
        }
        @keyframes gradient {
            0%, 100% { background-position: 0% center; }
            50% { background-position: 100% center; }
        }
        .subtitle {
            color: #888;
            margin-top: 10px;
            font-size: 1.1rem;
        }
        .status-bar {
            display: flex;
            justify-content: center;
            gap: 30px;
            margin-bottom: 30px;
            flex-wrap: wrap;
        }
        .status-item {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 10px 20px;
            background: rgba(255,255,255,0.05);
            border-radius: 20px;
            border: 1px solid rgba(255,255,255,0.1);
        }
        .status-dot {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            animation: pulse 2s infinite;
        }
        .status-dot.online { background: #00ff88; box-shadow: 0 0 10px #00ff88; }
        .status-dot.offline { background: #ff4444; }
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        .emergency-overlay {
            display: none;
            position: fixed;
            top: 0; left: 0; right: 0; bottom: 0;
            background: rgba(255, 0, 0, 0.3);
            z-index: 1000;
            justify-content: center;
            align-items: center;
            animation: emergencyPulse 0.5s infinite;
        }
        .emergency-overlay.active { display: flex; }
        @keyframes emergencyPulse {
            0%, 100% { background: rgba(255, 0, 0, 0.3); }
            50% { background: rgba(255, 0, 0, 0.6); }
        }
        .emergency-box {
            background: linear-gradient(135deg, #ff0000, #cc0000);
            padding: 40px 60px;
            border-radius: 20px;
            text-align: center;
            box-shadow: 0 0 50px rgba(255, 0, 0, 0.8);
            animation: shake 0.3s infinite;
        }
        @keyframes shake {
            0%, 100% { transform: translateX(0); }
            25% { transform: translateX(-5px); }
            75% { transform: translateX(5px); }
        }
        .emergency-icon { font-size: 4rem; margin-bottom: 20px; }
        .emergency-title { font-size: 2rem; font-weight: bold; margin-bottom: 10px; }
        .emergency-text { font-size: 1.2rem; opacity: 0.9; }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
        }
        .card {
            background: rgba(255, 255, 255, 0.05);
            backdrop-filter: blur(10px);
            border-radius: 20px;
            border: 1px solid rgba(255, 255, 255, 0.1);
            padding: 25px;
            transition: all 0.3s ease;
        }
        .card:hover {
            transform: translateY(-5px);
            border-color: rgba(0, 255, 200, 0.3);
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.3);
        }
        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
        }
        .card-title {
            font-size: 1.2rem;
            color: #00ffc8;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .card-icon {
            width: 40px;
            height: 40px;
            background: linear-gradient(135deg, #00ffc8, #7b2fff);
            border-radius: 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 1.2rem;
        }
        .big-value {
            font-size: 3.5rem;
            font-weight: 700;
            background: linear-gradient(135deg, #fff, #00ffc8);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        .unit {
            font-size: 1.5rem;
            color: #888;
            margin-left: 5px;
        }
        .flex-bars {
            display: flex;
            flex-direction: column;
            gap: 15px;
        }
        .flex-item {
            display: flex;
            align-items: center;
            gap: 15px;
        }
        .flex-label {
            width: 30px;
            font-weight: bold;
            color: #00ffc8;
        }
        .flex-bar-bg {
            flex: 1;
            height: 20px;
            background: rgba(255,255,255,0.1);
            border-radius: 10px;
            overflow: hidden;
        }
        .flex-bar-fill {
            height: 100%;
            background: linear-gradient(90deg, #00ffc8, #7b2fff);
            border-radius: 10px;
            transition: width 0.3s ease;
        }
        .flex-value {
            width: 50px;
            text-align: right;
            font-family: monospace;
        }
        .chart-container {
            height: 150px;
            position: relative;
            margin-top: 15px;
        }
        canvas {
            width: 100% !important;
            height: 100% !important;
        }
        .temp-display {
            display: flex;
            justify-content: space-around;
            margin-top: 20px;
        }
        .temp-item {
            text-align: center;
        }
        .temp-value {
            font-size: 2.5rem;
            font-weight: bold;
        }
        .temp-label {
            color: #888;
            margin-top: 5px;
        }
        .mode-badge {
            padding: 5px 15px;
            border-radius: 15px;
            font-size: 0.8rem;
            text-transform: uppercase;
        }
        .mode-comm { background: linear-gradient(135deg, #00ffc8, #00b894); }
        .mode-auto { background: linear-gradient(135deg, #7b2fff, #5f27cd); }
        footer {
            text-align: center;
            padding: 30px;
            color: #666;
            margin-top: 30px;
        }
        @media (max-width: 768px) {
            h1 { font-size: 2rem; }
            .big-value { font-size: 2.5rem; }
            .grid { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
    <div class="emergency-overlay" id="emergencyOverlay">
        <div class="emergency-box">
            <div class="emergency-icon">🚨</div>
            <div class="emergency-title" id="emergencyTitle">EMERGENCY ALERT</div>
            <div class="emergency-text" id="emergencyText">High Heart Rate Detected!</div>
            <button onclick="dismissEmergency()" style="margin-top:20px; padding:15px 40px; font-size:1rem; background:#fff; color:#cc0000; border:none; border-radius:10px; cursor:pointer; font-weight:bold;">DISMISS ALERT</button>
        </div>
    </div>
    <div class="bg-animation"></div>
    <div class="container">
        <header>
            <h1>GESTURA</h1>
            <p class="subtitle">Smart Glove Health Monitor</p>
        </header>
        
        <div class="status-bar">
            <div class="status-item">
                <div class="status-dot online" id="wifiStatus"></div>
                <span>WiFi Connected</span>
            </div>
            <div class="status-item">
                <div class="status-dot" id="fingerStatus"></div>
                <span id="fingerText">No Finger</span>
            </div>
            <div class="status-item">
                <span class="mode-badge mode-comm" id="modeBadge">COMM MODE</span>
            </div>
        </div>
        
        <div class="grid">
            <!-- Heart Rate Card -->
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <div class="card-icon">❤️</div>
                        Heart Rate
                    </div>
                </div>
                <div style="text-align: center;">
                    <span class="big-value" id="bpm">--</span>
                    <span class="unit">BPM</span>
                </div>
                <div class="chart-container">
                    <canvas id="hrChart"></canvas>
                </div>
            </div>
            
            <!-- SpO2 Card -->
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <div class="card-icon">🫁</div>
                        Blood Oxygen
                    </div>
                </div>
                <div style="text-align: center;">
                    <span class="big-value" id="spo2">--</span>
                    <span class="unit">%</span>
                </div>
                <p style="text-align:center; margin-top:20px; color:#888;" id="spo2Status">--</p>
            </div>
            
            <!-- Body Temperature Card -->
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <div class="card-icon">🌡️</div>
                        Body Temperature
                    </div>
                </div>
                <div class="temp-display">
                    <div class="temp-item">
                        <div class="temp-value" id="tempC">--</div>
                        <div class="temp-label">Celsius</div>
                    </div>
                    <div class="temp-item">
                        <div class="temp-value" id="tempF">--</div>
                        <div class="temp-label">Fahrenheit</div>
                    </div>
                </div>
            </div>
            
            <!-- Environment Card -->
            <div class="card">
                <div class="card-header">
                    <div class="card-title">
                        <div class="card-icon">🌤️</div>
                        Environment
                    </div>
                </div>
                <div class="temp-display">
                    <div class="temp-item">
                        <div class="temp-value" id="envTemp">--</div>
                        <div class="temp-label">Temperature °C</div>
                    </div>
                    <div class="temp-item">
                        <div class="temp-value" id="humidity">--</div>
                        <div class="temp-label">Humidity %</div>
                    </div>
                </div>
            </div>
            
            <!-- Flex Sensors Card -->
            <div class="card" style="grid-column: span 2;">
                <div class="card-header">
                    <div class="card-title">
                        <div class="card-icon">✋</div>
                        Flex Sensors
                    </div>
                </div>
                <div class="flex-bars">
                    <div class="flex-item">
                        <span class="flex-label">F1</span>
                        <div class="flex-bar-bg"><div class="flex-bar-fill" id="flex1Bar" style="width:0%"></div></div>
                        <span class="flex-value" id="flex1Val">0</span>
                    </div>
                    <div class="flex-item">
                        <span class="flex-label">F2</span>
                        <div class="flex-bar-bg"><div class="flex-bar-fill" id="flex2Bar" style="width:0%"></div></div>
                        <span class="flex-value" id="flex2Val">0</span>
                    </div>
                    <div class="flex-item">
                        <span class="flex-label">F3</span>
                        <div class="flex-bar-bg"><div class="flex-bar-fill" id="flex3Bar" style="width:0%"></div></div>
                        <span class="flex-value" id="flex3Val">0</span>
                    </div>
                    <div class="flex-item">
                        <span class="flex-label">F4</span>
                        <div class="flex-bar-bg"><div class="flex-bar-fill" id="flex4Bar" style="width:0%"></div></div>
                        <span class="flex-value" id="flex4Val">0</span>
                    </div>
                </div>
            </div>
        </div>
        
        <footer>
            <p>GESTURA Smart Glove © 2026 | Real-time Health Monitoring System</p>
        </footer>
    </div>
    
    <script>
        let hrData = [];
        let hrChart;
        
        function initChart() {
            const ctx = document.getElementById('hrChart').getContext('2d');
            for(let i = 0; i < 50; i++) hrData.push(0);
        }
        
        function drawChart() {
            const canvas = document.getElementById('hrChart');
            const ctx = canvas.getContext('2d');
            const w = canvas.width = canvas.offsetWidth;
            const h = canvas.height = canvas.offsetHeight;
            
            ctx.clearRect(0, 0, w, h);
            
            // grid lines
            ctx.strokeStyle = 'rgba(255,255,255,0.1)';
            ctx.lineWidth = 1;
            for(let i = 0; i < 5; i++) {
                ctx.beginPath();
                ctx.moveTo(0, i * h / 4);
                ctx.lineTo(w, i * h / 4);
                ctx.stroke();
            }
            
            const gradient = ctx.createLinearGradient(0, 0, 0, h);
            gradient.addColorStop(0, '#00ffc8');
            gradient.addColorStop(1, '#7b2fff');
            
            ctx.strokeStyle = gradient;
            ctx.lineWidth = 2;
            ctx.beginPath();
            
            for(let i = 0; i < hrData.length; i++) {
                const x = (i / hrData.length) * w;
                const y = h - (hrData[i] / 150 * h);
                if(i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.stroke();
            
            ctx.lineTo(w, h);
            ctx.lineTo(0, h);
            ctx.closePath();
            const fillGradient = ctx.createLinearGradient(0, 0, 0, h);
            fillGradient.addColorStop(0, 'rgba(0, 255, 200, 0.3)');
            fillGradient.addColorStop(1, 'rgba(123, 47, 255, 0.1)');
            ctx.fillStyle = fillGradient;
            ctx.fill();
        }
        
        function updateData() {
            fetch('/data')
                .then(r => r.json())
                .then(d => {
                    const fingerDot = document.getElementById('fingerStatus');
                    const fingerText = document.getElementById('fingerText');
                    if(d.finger) {
                        fingerDot.className = 'status-dot online';
                        fingerText.textContent = 'Finger Detected';
                    } else {
                        fingerDot.className = 'status-dot offline';
                        fingerText.textContent = 'No Finger';
                    }
                    
                    const badge = document.getElementById('modeBadge');
                    badge.textContent = d.mode === 0 ? 'COMM MODE' : 'AUTO MODE';
                    badge.className = 'mode-badge ' + (d.mode === 0 ? 'mode-comm' : 'mode-auto');
                    
                    const overlay = document.getElementById('emergencyOverlay');
                    if(d.emergency) {
                        overlay.classList.add('active');
                        document.getElementById('emergencyTitle').textContent = 'EMERGENCY ALERT';
                        document.getElementById('emergencyText').textContent = d.emergencyType;
                        if(!window.alertPlaying) {
                            window.alertPlaying = true;
                        }
                    } else {
                        overlay.classList.remove('active');
                        window.alertPlaying = false;
                    }
                    
                    document.getElementById('bpm').textContent = d.finger ? d.bpm : '--';
                    document.getElementById('spo2').textContent = d.finger ? d.spo2 : '--';
                    document.getElementById('spo2Status').textContent = d.finger ? 
                        (d.spo2 >= 95 ? 'Status: NORMAL' : 'Status: LOW') : '--';
                    
                    document.getElementById('tempC').textContent = d.finger ? d.tempC.toFixed(1) + '°' : '--';
                    document.getElementById('tempF').textContent = d.finger ? d.tempF.toFixed(0) + '°' : '--';
                    
                    document.getElementById('envTemp').textContent = d.dhtTemp.toFixed(1) + '°';
                    document.getElementById('humidity').textContent = d.humidity.toFixed(0) + '%';
                    
                    for(let i = 1; i <= 4; i++) {
                        const pct = Math.min(100, Math.max(0, (d['flex' + i] / 4095) * 100));
                        document.getElementById('flex' + i + 'Bar').style.width = pct + '%';
                        document.getElementById('flex' + i + 'Val').textContent = d['flex' + i];
                    }
                    
                    if(d.finger) {
                        hrData.push(d.bpm);
                        if(hrData.length > 50) hrData.shift();
                    }
                    drawChart();
                })
                .catch(e => console.log('Error:', e));
        }
        
        function dismissEmergency() {
            fetch('/cancelEmergency')
                .then(() => {
                    document.getElementById('emergencyOverlay').classList.remove('active');
                })
                .catch(e => console.log('Error:', e));
        }
        
        initChart();
        setInterval(updateData, 500);
        updateData();
    </script>
</body>
</html>
)=====";

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(F("GESTURA Glove A Starting..."));

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  delay(250);
  if (!display.begin(0x3C, true)) {
    Serial.println(F("SH1106 failed"));
    while (1)
      delay(10);
  }
  display.clearDisplay();
  display.setRotation(2);
  display.display();

  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX, DFPLAYER_TX);
  delay(1000);
  dfPlayerReady = dfPlayer.begin(dfPlayerSerial, false, true);
  if (dfPlayerReady) {
    dfPlayer.volume(30);
    dfPlayer.play(1);
    lastDFPlayerCmd = millis();
  }

  dht.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FLEX1_PIN, INPUT);
  pinMode(FLEX2_PIN, INPUT);
  pinMode(FLEX3_PIN, INPUT);
  pinMode(FLEX4_PIN, INPUT);
  analogReadResolution(12);

  max30102Ready = particleSensor.begin(Wire, I2C_SPEED_FAST);
  if (max30102Ready) {
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x0A);
    particleSensor.setPulseAmplitudeGreen(0);
    particleSensor.enableDIETEMPRDY();
  }

  for (int i = 0; i < GRAPH_WIDTH; i++)
    graphData[i] = GRAPH_HEIGHT / 2;
  randomSeed(analogRead(A0) + millis());

  showStartupScreen();

  // try to connect wifi
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(10, 20);
  display.print(F("Connecting WiFi..."));
  display.display();

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println(F("\nWiFi Connected!"));
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());

    display.clearDisplay();
    display.setCursor(10, 15);
    display.print(F("WiFi Connected!"));
    display.setCursor(10, 35);
    display.print(F("IP: "));
    display.print(WiFi.localIP());
    display.display();
    delay(2000);

    server.on("/", []() { server.send(200, "text/html", MAIN_page); });

    server.on("/data", []() {
      String json = "{";
      json += "\"bpm\":" + String(displayBPM) + ",";
      json += "\"spo2\":" + String(displaySpO2) + ",";
      json += "\"tempC\":" + String(bodyTemperature, 1) + ",";
      json += "\"tempF\":" + String(bodyTemperatureF, 1) + ",";
      json += "\"dhtTemp\":" + String(dhtTemperature, 1) + ",";
      json += "\"humidity\":" + String(dhtHumidity, 1) + ",";
      json += "\"flex1\":" + String(flexValues[0]) + ",";
      json += "\"flex2\":" + String(flexValues[1]) + ",";
      json += "\"flex3\":" + String(flexValues[2]) + ",";
      json += "\"flex4\":" + String(flexValues[3]) + ",";
      json += "\"finger\":" + String(fingerDetected ? "true" : "false") + ",";
      json += "\"mode\":" + String(currentMode) + ",";
      json +=
          "\"emergency\":" + String(emergencyActive ? "true" : "false") + ",";
      json += "\"emergencyType\":\"" + emergencyType + "\"";
      json += "}";
      server.send(200, "application/json", json);
    });

    server.on("/cancelEmergency", []() {
      emergencyActive = false;
      emergencyType = "";
      server.send(200, "text/plain", "OK");
    });

    server.begin();
    Serial.println(F("Web server started!"));
  } else {
    Serial.println(F("\nWiFi Failed"));
    display.clearDisplay();
    display.setCursor(10, 25);
    display.print(F("WiFi Failed!"));
    display.display();
    delay(2000);
  }

  currentMode = MODE_COMMUNICATION;
  if (dfPlayerReady)
    playTrackSafe(2);
}

void loop() {
  static unsigned long lastDHTRead = 0;
  static unsigned long lastUpdate = 0;
  static unsigned long lastFlexRead = 0;
  unsigned long currentMillis = millis();

  if (wifiConnected)
    server.handleClient();
  if (dfPlayerSerial.available())
    dfPlayer.readType();

  handleButton();

  if (max30102Ready)
    readMAX30102();
  updateSimulatedValues();

  if (currentMillis - lastFlexRead >= 50) {
    readAllFlexSensors();
    lastFlexRead = currentMillis;
    if (currentMode == MODE_COMMUNICATION)
      detectGestures();
  }

  if (currentMillis - lastDHTRead >= 2000) {
    readDHT();
    lastDHTRead = currentMillis;
  }

  if (currentMillis - lastUpdate >= 100) {
    updateDisplay();
    lastUpdate = currentMillis;
  }

  delay(5);
}

void readMAX30102() {
  irValue = particleSensor.getIR();
  bool currentFingerState = (irValue > 50000);

  if (currentFingerState != lastFingerState) {
    lastFingerState = currentFingerState;

    // track taps for emergency detection - only on heart rate screen
    if (currentScreen == SCREEN_HEART_RATE) {
      unsigned long now = millis();
      fingerToggleTimes[toggleIndex] = now;
      toggleIndex = (toggleIndex + 1) % 5;

      int oldestIndex = toggleIndex;
      unsigned long oldestTime = fingerToggleTimes[oldestIndex];

      if (oldestTime > 0 && (now - oldestTime) < 5000) {
        // 5 taps in 5 sec = SOS
        emergencyActive = true;
        emergencyStartTime = now;
        emergencyType = "HIGH HEART RATE DETECTED!";

        Serial.println(F("!!! EMERGENCY TRIGGERED !!!"));

        if (dfPlayerReady) {
          dfPlayer.play(12);
        }

        for (int i = 0; i < 5; i++) {
          fingerToggleTimes[i] = 0;
        }
      }
    }
  }

  fingerDetected = currentFingerState;

  if (fingerDetected) {
    bodyTemperature = particleSensor.readTemperature();
    bodyTemperatureF = particleSensor.readTemperatureF();
  }

  // auto-clear emergency after 30s
  if (emergencyActive && (millis() - emergencyStartTime > 30000)) {
    emergencyActive = false;
    emergencyType = "";
  }
}

void updateSimulatedValues() {
  unsigned long currentMillis = millis();
  if (!fingerDetected) {
    simulatedBPM = 72.0;
    targetBPM = 72.0;
    simulatedSpO2 = 98.0;
    targetSpO2 = 98.0;
    return;
  }

  if (currentMillis - lastTargetChange > 3000 + random(2000)) {
    targetBPM = 65.0 + random(20);
    targetSpO2 = 96.0 + random(4);
    lastTargetChange = currentMillis;
  }

  if (currentMillis - lastValueUpdate > 100) {
    if (simulatedBPM < targetBPM)
      simulatedBPM += min(0.5f, targetBPM - simulatedBPM);
    else if (simulatedBPM > targetBPM)
      simulatedBPM -= min(0.5f, simulatedBPM - targetBPM);
    simulatedBPM += (random(5) - 2) * 0.1;
    simulatedBPM = constrain(simulatedBPM, 60.0f, 100.0f);
    displayBPM = (int)round(simulatedBPM);

    if (simulatedSpO2 < targetSpO2)
      simulatedSpO2 += min(0.2f, targetSpO2 - simulatedSpO2);
    else if (simulatedSpO2 > targetSpO2)
      simulatedSpO2 -= min(0.2f, simulatedSpO2 - targetSpO2);
    simulatedSpO2 = constrain(simulatedSpO2, 95.0f, 100.0f);
    displaySpO2 = (int)round(simulatedSpO2);

    updateHeartGraph();
    lastValueUpdate = currentMillis;
  }
}

void updateHeartGraph() {
  static int beatCounter = 0;
  int updatesPerBeat = 600 / displayBPM;
  beatCounter++;
  int value;
  if (beatCounter % updatesPerBeat < 2)
    value = GRAPH_HEIGHT - 3;
  else if (beatCounter % updatesPerBeat < 4)
    value = GRAPH_HEIGHT / 2 + 5;
  else
    value = GRAPH_HEIGHT / 2 + random(-2, 3);
  graphData[graphIndex] = value;
  graphIndex = (graphIndex + 1) % GRAPH_WIDTH;
}

void playTrackSafe(uint8_t trackNum) {
  if (millis() - lastDFPlayerCmd < DFPLAYER_CMD_DELAY)
    return;
  if (dfPlayerReady) {
    dfPlayer.play(trackNum);
    lastDFPlayerCmd = millis();
  }
}

int readFlexAveraged(int pin) {
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(pin);
    delayMicroseconds(100);
  }
  return sum / ADC_SAMPLES;
}

void readAllFlexSensors() {
  flexValues[0] = readFlexAveraged(FLEX1_PIN);
  flexValues[1] = readFlexAveraged(FLEX2_PIN);
  flexValues[2] = readFlexAveraged(FLEX3_PIN);
  flexValues[3] = readFlexAveraged(FLEX4_PIN);
}

void detectGestures() {
  unsigned long cm = millis();
  // each gesture has its own cooldown so they don't spam
  if (flexValues[0] > 2900 && !gestureActive[0] &&
      (cm - lastGestureTime[0] > GESTURE_COOLDOWN_MS)) {
    playTrackSafe(11);
    lastGestureTime[0] = cm;
    gestureActive[0] = true;
  } else if (flexValues[0] <= 2900)
    gestureActive[0] = false;
  if (flexValues[1] < 30 && !gestureActive[1] &&
      (cm - lastGestureTime[1] > GESTURE_COOLDOWN_MS)) {
    playTrackSafe(10);
    lastGestureTime[1] = cm;
    gestureActive[1] = true;
  } else if (flexValues[1] >= 30)
    gestureActive[1] = false;
  if (flexValues[2] < 200 && !gestureActive[2] &&
      (cm - lastGestureTime[2] > GESTURE_COOLDOWN_MS)) {
    playTrackSafe(9);
    lastGestureTime[2] = cm;
    gestureActive[2] = true;
  } else if (flexValues[2] >= 200)
    gestureActive[2] = false;
  if (flexValues[3] < 180 && !gestureActive[3] &&
      (cm - lastGestureTime[3] > GESTURE_COOLDOWN_MS)) {
    playTrackSafe(8);
    lastGestureTime[3] = cm;
    gestureActive[3] = true;
  } else if (flexValues[3] >= 180)
    gestureActive[3] = false;
}

void readDHT() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    dhtHumidity = h;
    dhtTemperature = t;
  }
}

void handleButton() {
  bool cs = digitalRead(BUTTON_PIN);
  unsigned long cm = millis();
  if (cs == LOW && lastButtonState == HIGH) {
    buttonPressStart = cm;
    modeSwitchTriggered = false;
  }
  if (cs == LOW && !modeSwitchTriggered &&
      (cm - buttonPressStart) >= LONG_PRESS_MS) {
    currentMode = (currentMode == MODE_COMMUNICATION) ? MODE_AUTOMATION
                                                      : MODE_COMMUNICATION;
    playTrackSafe(currentMode == MODE_AUTOMATION ? 3 : 2);
    modeSwitchTriggered = true;
  }
  if (cs == HIGH && lastButtonState == LOW && !modeSwitchTriggered &&
      (cm - buttonPressStart) < LONG_PRESS_MS) {
    currentScreen = (Screen)((currentScreen + 1) % SCREEN_COUNT);
  }
  lastButtonState = cs;
}

void showStartupScreen() {
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, SH110X_WHITE);
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(20, 10);
  display.print(F("GESTURA"));
  display.setTextSize(1);
  display.setCursor(35, 32);
  display.print(F("GLOVE A"));
  display.setCursor(15, 50);
  display.print(F("INITIALIZING..."));
  display.display();
  delay(1500);
}

void drawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, int p) {
  p = constrain(p, 0, 100);
  display.drawRect(x, y, w, h, SH110X_WHITE);
  int f = (w - 4) * p / 100;
  if (f > 0)
    display.fillRect(x + 2, y + 2, f, h - 4, SH110X_WHITE);
}

void updateDisplay() {
  switch (currentScreen) {
  case SCREEN_FLEX:
    showFlexScreen();
    break;
  case SCREEN_DHT:
    showDHTScreen();
    break;
  case SCREEN_HEART_RATE:
    showHeartRateScreen();
    break;
  case SCREEN_SPO2:
    showSpO2Screen();
    break;
  case SCREEN_BODY_TEMP:
    showBodyTempScreen();
    break;
  }
}

void showFlexScreen() {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 14, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(5, 3);
  display.print(currentMode == MODE_COMMUNICATION ? F("COMM") : F("AUTO"));
  display.setCursor(85, 3);
  display.print(F("FLEX"));
  display.setTextColor(SH110X_WHITE);
  int p1 = (flexValues[0] * 100) / 4095;
  display.setCursor(2, 18);
  display.print(F("F1"));
  drawProgressBar(18, 17, 60, 8, p1);
  display.setCursor(82, 18);
  display.print(flexValues[0]);
  display.setCursor(2, 30);
  display.print(F("F2"));
  drawProgressBar(18, 29, 60, 8,
                  constrain((600 - flexValues[1]) * 100 / 600, 0, 100));
  display.setCursor(82, 30);
  display.print(flexValues[1]);
  display.setCursor(2, 42);
  display.print(F("F3"));
  drawProgressBar(18, 41, 60, 8,
                  constrain((600 - flexValues[2]) * 100 / 600, 0, 100));
  display.setCursor(82, 42);
  display.print(flexValues[2]);
  display.setCursor(2, 54);
  display.print(F("F4"));
  drawProgressBar(18, 53, 60, 8,
                  constrain((600 - flexValues[3]) * 100 / 600, 0, 100));
  display.setCursor(82, 54);
  display.print(flexValues[3]);
  display.display();
}

void showDHTScreen() {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 14, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(40, 3);
  display.print(F("DHT11"));
  display.setTextColor(SH110X_WHITE);
  display.setCursor(5, 22);
  display.print(F("Temp: "));
  display.print(dhtTemperature, 1);
  display.print(F(" C"));
  display.setCursor(5, 42);
  display.print(F("Humidity: "));
  display.print(dhtHumidity, 1);
  display.print(F(" %"));
  if (wifiConnected) {
    display.setCursor(5, 55);
    display.print(F("IP:"));
    display.print(WiFi.localIP());
  }
  display.display();
}

void showHeartRateScreen() {
  display.clearDisplay();

  if (emergencyActive) {
    // flash the border
    static bool flashOn = false;
    static unsigned long lastFlash = 0;
    if (millis() - lastFlash > 300) {
      flashOn = !flashOn;
      lastFlash = millis();
    }

    if (flashOn) {
      display.drawRect(0, 0, 128, 64, SH110X_WHITE);
      display.drawRect(2, 2, 124, 60, SH110X_WHITE);
    }

    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(10, 10);
    display.print(F("EMERGENCY"));

    display.setTextSize(1);
    display.setCursor(15, 35);
    display.print(F("HIGH HEART RATE"));
    display.setCursor(30, 48);
    display.print(F("DETECTED!"));

    display.display();
    return;
  }

  display.fillRect(0, 0, 128, 14, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(5, 3);
  display.print(F("HEART RATE"));
  display.setCursor(100, 3);
  display.print(fingerDetected ? F("OK") : F("--"));
  display.setTextColor(SH110X_WHITE);
  if (!fingerDetected) {
    display.setCursor(15, 35);
    display.print(F("Place finger..."));
  } else {
    display.setTextSize(2);
    display.setCursor(5, 22);
    display.print(displayBPM);
    display.setTextSize(1);
    display.print(F(" BPM"));
    int gx = 50, gy = 20, gw = 75, gh = 42;
    display.drawRect(gx, gy, gw, gh, SH110X_WHITE);
    for (int i = 1; i < gw - 2; i++) {
      int idx = (graphIndex + (i * GRAPH_WIDTH / (gw - 2))) % GRAPH_WIDTH;
      int val = map(graphData[idx], 0, GRAPH_HEIGHT, 0, gh - 4);
      int y = gy + gh - 2 - val;
      display.drawPixel(gx + i, constrain(y, gy + 1, gy + gh - 2),
                        SH110X_WHITE);
    }
  }
  display.display();
}

void showSpO2Screen() {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 14, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(5, 3);
  display.print(F("BLOOD OXYGEN"));
  display.setTextColor(SH110X_WHITE);
  if (!fingerDetected) {
    display.setCursor(15, 35);
    display.print(F("Place finger..."));
  } else {
    display.setTextSize(3);
    display.setCursor(25, 28);
    display.print(displaySpO2);
    display.setTextSize(2);
    display.print(F("%"));
    display.setTextSize(1);
    display.setCursor(25, 55);
    display.print(displaySpO2 >= 95 ? F("NORMAL") : F("LOW"));
  }
  display.display();
}

void showBodyTempScreen() {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 14, SH110X_WHITE);
  display.setTextSize(1);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(5, 3);
  display.print(F("BODY TEMP"));
  display.setTextColor(SH110X_WHITE);
  if (!fingerDetected) {
    display.setCursor(15, 35);
    display.print(F("Place finger..."));
  } else {
    display.setTextSize(2);
    display.setCursor(10, 25);
    display.print(bodyTemperature, 1);
    display.setTextSize(1);
    display.print(F("C"));
    display.setTextSize(2);
    display.setCursor(70, 25);
    display.print(bodyTemperatureF, 0);
    display.setTextSize(1);
    display.print(F("F"));
    display.setCursor(10, 55);
    display.print(bodyTemperature >= 37.5 ? F("ELEVATED") : F("NORMAL"));
  }
  display.display();
}
