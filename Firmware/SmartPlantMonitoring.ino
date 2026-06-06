#define BLYNK_TEMPLATE_ID "TMPL3sivO2VkT"
#define BLYNK_TEMPLATE_NAME "Smart Plant Monitor"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// ─── OLED ────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS  0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ─── DHT11 ───────────────────────────────────────────────────
#define DHTPIN   13
#define DHTTYPE  DHT11
DHT dht(DHTPIN, DHTTYPE);

// ─── Soil Sensor ─────────────────────────────────────────────
#define SOIL_PIN  34

// ─── Relay ───────────────────────────────────────────────────
#define RELAY_PIN  15

// ─── Ultrasonic ──────────────────────────────────────────────
#define TRIG_PIN  12
#define ECHO_PIN  14

// ─── Buzzer ──────────────────────────────────────────────────
#define BUZZER_PIN  18

// ─── NEW: IR + Touch sensors ─────────────────────────────────
#define IR_PIN     27
#define TOUCH_PIN   4

// ─── Thresholds (unchanged) ──────────────────────────────────
int   soilThreshold       = 4000;
float waterLevelThreshold = 3.0;
float tankDepth           = 5.0;
float sensorOffset        = 2.0;

// ─── Relay polarity (unchanged) ──────────────────────────────
#define RELAY_ACTIVE_LOW  true
#define RELAY_ON   (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF  (RELAY_ACTIVE_LOW ? HIGH : LOW)

// ─── Global irrigation variables (unchanged) ─────────────────
int   soilValue;
float temperature;
float humidity;
float waterLevelCm;
int   waterLevelPercent;
bool  pumpRunning = false;

// ============================================================
//  EYE ANIMATION STATE
// ============================================================

// Emotions cycle on each touch
enum Emotion {
  EMO_NORMAL = 0,
  EMO_SMILE,
  EMO_CRY,
  EMO_LAUGH,
  EMO_CLOSED,
  EMO_WINK,
  EMO_COUNT
};

Emotion   currentEmotion  = EMO_NORMAL;
bool      eyesOpen        = false;   // controlled by IR
float     openAmount      = 0.0f;    // 0.0 = fully closed, 1.0 = fully open

// Blink state
unsigned long lastBlinkTime  = 0;
bool          inBlink        = false;
float         blinkProgress  = 0.0f;
unsigned long blinkStart     = 0;
const int     BLINK_DURATION = 200;  // ms for one blink cycle

// Tear drop animation
float   tearY[2]        = {0, 0};   // left & right tear Y offset
bool    tearActive[2]   = {false, false};
unsigned long tearStart[2] = {0, 0};

// Laugh bounce
float laughBounce       = 0.0f;
unsigned long laughTime = 0;

// Touch debounce
bool          lastTouchState  = false;
unsigned long lastTouchChange = 0;
const int     TOUCH_DEBOUNCE  = 80;

// IR debounce
bool          lastIRState     = false;
unsigned long lastIRChange    = 0;
const int     IR_DEBOUNCE     = 50;

// ─── Eye geometry (centered in each half of 128×64 screen) ───
// Left eye center:  (32, 32)   Right eye center: (96, 32)
// Full open radius: width=26, height=20

struct EyeParams {
  int cx;       // center X
  int cy;       // center Y
  int w;        // half-width
  int h;        // half-height (modulated by openAmount)
  bool closed;  // forced closed (wink right side)
};

// ============================================================
//  IRRIGATION FUNCTIONS — COMPLETELY UNCHANGED
// ============================================================

void readSoilMoisture() {
  long total = 0;
  for (int i = 0; i < 10; i++) {
    total += analogRead(SOIL_PIN);
    delay(10);
  }
  soilValue = total / 10;
  Serial.print("Soil Moisture: ");
  Serial.print(soilValue);
  if (soilValue > soilThreshold) {
    Serial.println(" --> DRY");
  } else {
    Serial.println(" --> WET");
  }
}

void readDHTSensor() {
  temperature = dht.readTemperature();
  humidity    = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT read failed!");
    temperature = 0;
    humidity    = 0;
    return;
  }
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" C Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
}

void readWaterLevel() {
  float totalDistance = 0;
  int   validReadings = 0;
  for (int i = 0; i < 5; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration > 0) {
      float dist = (duration * 0.0343) / 2.0;
      totalDistance += dist;
      validReadings++;
    }
    delay(50);
  }
  if (validReadings == 0) {
    Serial.println("Ultrasonic: FAILED - No echo!");
    waterLevelCm      = -1;
    waterLevelPercent = -1;
    return;
  }
  float distanceCm = totalDistance / validReadings;
  waterLevelCm = tankDepth - (distanceCm - sensorOffset);
  if (waterLevelCm < 0)         waterLevelCm = 0;
  if (waterLevelCm > tankDepth) waterLevelCm = tankDepth;
  waterLevelPercent = (int)((waterLevelCm / tankDepth) * 100);
  Serial.print("Raw Distance: ");
  Serial.print(distanceCm);
  Serial.print(" cm | Water Level: ");
  Serial.print(waterLevelCm);
  Serial.print(" cm (");
  Serial.print(waterLevelPercent);
  Serial.println("%)");
  if (waterLevelCm < waterLevelThreshold) {
    Serial.println("Water Status: LOW!");
  } else {
    Serial.println("Water Status: OK");
  }
}

void controlRelay() {
  bool soilIsDry  = (soilValue > soilThreshold);
  bool waterIsLow = (waterLevelCm >= 0 && waterLevelCm < waterLevelThreshold);
  Serial.println("-----------------------------");
  Serial.print("Soil Dry: ");   Serial.println(soilIsDry  ? "YES" : "NO");
  Serial.print("Water Low: ");  Serial.println(waterIsLow ? "YES" : "NO");
  if (soilIsDry && !waterIsLow) {
    digitalWrite(RELAY_PIN, RELAY_ON);
    pumpRunning = true;
    Serial.println("PUMP: ON");
  } else {
    digitalWrite(RELAY_PIN, RELAY_OFF);
    pumpRunning = false;
    if (waterIsLow) {
      Serial.println("PUMP: OFF (water too low)");
    } else {
      Serial.println("PUMP: OFF (soil is wet)");
    }
  }
  Serial.println("-----------------------------");
}

void checkWaterLevelAlert() {
  if (waterLevelCm < 0) return;
  if (waterLevelCm < waterLevelThreshold) {
    digitalWrite(BUZZER_PIN, HIGH); delay(200);
    digitalWrite(BUZZER_PIN, LOW);  delay(100);
    digitalWrite(BUZZER_PIN, HIGH); delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("BUZZER: LOW WATER ALERT!");
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ============================================================
//  EYE DRAWING HELPERS
// ============================================================

// Draw a filled ellipse manually (Adafruit_GFX has no fillEllipse)
void fillEllipse(int cx, int cy, int rx, int ry, uint16_t color) {
  if (ry <= 0) return;
  for (int y = -ry; y <= ry; y++) {
    float ratio = (float)y / (float)ry;
    int   xw    = (int)(rx * sqrt(1.0f - ratio * ratio));
    if (xw > 0) {
      display.drawFastHLine(cx - xw, cy + y, 2 * xw + 1, color);
    }
  }
}

// Draw outline ellipse
void drawEllipse(int cx, int cy, int rx, int ry, uint16_t color) {
  for (int i = 0; i < 360; i += 3) {
    float rad = i * 3.14159f / 180.0f;
    int   x   = cx + (int)(rx * cos(rad));
    int   y   = cy + (int)(ry * sin(rad));
    display.drawPixel(x, y, color);
  }
}

// Draw a single eye with full control over shape
void drawEye(int cx, int cy, float open, Emotion emo, bool forceClose, bool isLeft) {

  int eyeW = 24;  // half-width of eye box
  int eyeH = 18;  // max half-height
  int pupilR = 5;

  if (forceClose || open <= 0.02f) {
    // Fully closed — draw a horizontal line
    display.drawFastHLine(cx - eyeW, cy, eyeW * 2, SSD1306_WHITE);
    return;
  }

  int openH = max(1, (int)(eyeH * open));

  if (emo == EMO_SMILE) {
    // Happy curved / squinting eyes
    int sqH = max(1, (int)(openH * 0.55f));
    for (int x = -eyeW; x <= eyeW; x++) {
      float nx  = (float)x / eyeW;
      int   top = cy - sqH;
      int   bot = cy + (int)(sqH * (1.0f - nx * nx));
      if (top <= bot) {
        display.drawFastVLine(cx + x, top, bot - top + 1, SSD1306_WHITE);
      }
    }
    return;
  }

  if (emo == EMO_CRY) {
    // Sad eyes — angled inward (inner corners raised)
    int sadH = max(1, (int)(openH * 0.8f));
    for (int x = -eyeW; x <= eyeW; x++) {
      float nx    = (float)x / eyeW;
      float inner = isLeft ? nx : -nx;
      int   tiltT = cy - sadH + (int)(sadH * 0.4f * inner);
      int   tiltB = cy + sadH - (int)(sadH * 0.2f * inner);
      if (tiltT <= tiltB) {
        display.drawFastVLine(cx + x, tiltT, tiltB - tiltT + 1, SSD1306_WHITE);
      }
    }
    return;
  }

  if (emo == EMO_LAUGH) {
    // Wide open laughing — crescent shape
    int lH = max(1, (int)(openH * 0.7f));
    int bounce = (int)(laughBounce * 3);
    fillEllipse(cx, cy + bounce, eyeW, lH, SSD1306_WHITE);
    display.fillRect(cx - eyeW - 1, cy - lH - 1 + bounce, eyeW * 2 + 2, lH / 2, SSD1306_BLACK);
    display.drawFastHLine(cx - eyeW, cy + lH + bounce, eyeW * 2, SSD1306_WHITE);
    return;
  }

  // NORMAL / WINK — realistic eye with pupil and eyelashes
  fillEllipse(cx, cy, eyeW, openH, SSD1306_WHITE);

  // Pupil
  int pupilY = cy;
  fillEllipse(cx, pupilY, pupilR, min(pupilR, openH - 2), SSD1306_BLACK);

  // Highlight dot
  if (openH > 4) {
    display.drawPixel(cx - 2, pupilY - 2, SSD1306_WHITE);
  }

  // Upper eyelid shadow
  int lidH = max(1, openH / 4);
  display.fillRect(cx - eyeW, cy - openH, eyeW * 2, lidH, SSD1306_BLACK);

  // Eyelashes — 5 lines on top edge
  for (int l = -2; l <= 2; l++) {
    int lx  = cx + l * (eyeW / 3);
    int ly1 = cy - openH;
    int ly2 = ly1 - 3 - abs(l);
    display.drawLine(lx, ly1, lx, ly2, SSD1306_WHITE);
  }
}

// Draw tears for cry emotion
void drawTears() {
  unsigned long now = millis();
  for (int e = 0; e < 2; e++) {
    if (!tearActive[e] && (now - tearStart[e] > 600)) {
      tearActive[e] = true;
      tearY[e]      = 0;
      tearStart[e]  = now;
    }
    if (tearActive[e]) {
      int cx  = (e == 0) ? 32 : 96;
      int ty  = (int)(32 + 18 + tearY[e]);
      int tx  = cx + (e == 0 ? -5 : 5);
      display.fillCircle(tx, ty, 2, SSD1306_WHITE);
      tearY[e] += 1.2f;
      if (ty > SCREEN_HEIGHT + 4) {
        tearActive[e] = false;
        tearStart[e]  = now;
      }
    }
  }
}

// Update laugh bounce animation
void updateLaughBounce() {
  unsigned long now = millis();
  float t  = (now - laughTime) / 300.0f;
  laughBounce = sin(t * 3.14159f * 2.0f) * 0.5f + 0.5f;
}

// Smooth open/close driven by IR
void updateOpenAmount() {
  float target = eyesOpen ? 1.0f : 0.0f;
  float speed  = 0.06f;
  if (openAmount < target) {
    openAmount = min(openAmount + speed, target);
  } else if (openAmount > target) {
    openAmount = max(openAmount - speed, target);
  }
}

// Natural blink
void triggerBlink() {
  if (!inBlink && eyesOpen &&
      (currentEmotion == EMO_NORMAL || currentEmotion == EMO_WINK)) {
    inBlink     = true;
    blinkStart  = millis();
    blinkProgress = 0.0f;
  }
}

float blinkModifier() {
  if (!inBlink) return 1.0f;
  unsigned long elapsed = millis() - blinkStart;
  if (elapsed >= (unsigned long)BLINK_DURATION) {
    inBlink = false;
    return 1.0f;
  }
  float t = (float)elapsed / BLINK_DURATION;
  return 1.0f - sin(t * 3.14159f);
}

// ============================================================
//  MAIN DISPLAY FUNCTION
// ============================================================

void displayData() {

  // Auto-blink timer (every 3–5 s)
  static unsigned long nextBlink = 0;
  unsigned long now = millis();
  if (now >= nextBlink) {
    triggerBlink();
    nextBlink = now + random(3000, 5500);
  }

  // Laugh bounce update
  if (currentEmotion == EMO_LAUGH) updateLaughBounce();

  // Smooth open/close
  updateOpenAmount();

  float blinkMod = blinkModifier();
  float effectiveOpen = openAmount * blinkMod;

  // Render frame
  display.clearDisplay();

  // Divider line between eyes
  display.drawFastVLine(64, 10, 44, SSD1306_WHITE);

  bool leftClose  = (currentEmotion == EMO_CLOSED);
  bool rightClose = (currentEmotion == EMO_CLOSED) ||
                    (currentEmotion == EMO_WINK);   // wink = right closed

  drawEye(32, 32, effectiveOpen, currentEmotion, leftClose,  true);
  drawEye(96, 32, effectiveOpen, currentEmotion, rightClose, false);

  if (currentEmotion == EMO_CRY && eyesOpen) {
    drawTears();
  }

  // Emotion label at bottom
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  const char* labels[] = {"NORMAL","SMILE","CRY","LAUGH","CLOSED","WINK"};
  int lw = strlen(labels[currentEmotion]) * 6;
  display.setCursor((SCREEN_WIDTH - lw) / 2, 56);
  display.print(labels[currentEmotion]);

  display.display();
}

// ============================================================
//  SENSOR INPUT HANDLERS
// ============================================================

void handleIRSensor() {
  bool irState = (digitalRead(IR_PIN) == LOW);
  unsigned long now = millis();
  if (irState != lastIRState && (now - lastIRChange > IR_DEBOUNCE)) {
    lastIRState  = irState;
    lastIRChange = now;
    eyesOpen     = irState;
    Serial.print("IR: eyes ");
    Serial.println(eyesOpen ? "OPENING" : "CLOSING");
  }
}

void handleTouchSensor() {
  // INPUT_PULLUP: LOW = touched, HIGH = released
  bool rawTouch = (digitalRead(TOUCH_PIN) == LOW);
  unsigned long now = millis();

  if (rawTouch != lastTouchState) {
    if ((now - lastTouchChange) >= (unsigned long)TOUCH_DEBOUNCE) {
      lastTouchChange = now;
      lastTouchState  = rawTouch;

      if (!rawTouch) {
        // Rising edge: finger just lifted → advance emotion
        currentEmotion = (Emotion)((currentEmotion + 1) % EMO_COUNT);
        for (int i = 0; i < 2; i++) {
          tearActive[i] = false;
          tearStart[i]  = now;
        }
        laughTime = now;
        const char* names[] = {"NORMAL","SMILE","CRY","LAUGH","CLOSED","WINK"};
        Serial.print("TOUCH: emotion -> ");
        Serial.println(names[currentEmotion]);
      }
    }
  }
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  Serial.println("=== System Starting ===");

  // Relay – start OFF
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // IR + Touch
  pinMode(IR_PIN,    INPUT);
  pinMode(TOUCH_PIN, INPUT_PULLUP);   // PULLUP: LOW=touched, HIGH=released

  // Relay self-test (unchanged)
  Serial.println("Relay test: ON for 1 second...");
  digitalWrite(RELAY_PIN, RELAY_ON);
  delay(1000);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  Serial.println("Relay test: OFF");

  // DHT
  dht.begin();

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 FAILED – check wiring/address!");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 28);
  display.print("System Started");
  display.display();
  delay(2000);
  display.clearDisplay();
  display.display();

  // Seed random for blink timing
  randomSeed(analogRead(0));

  Serial.println("=== Setup Complete ===");
}

// ============================================================
//  LOOP — original irrigation timing preserved exactly
// ============================================================

unsigned long lastIrrigationCheck = 0;
const unsigned long IRRIGATION_INTERVAL = 2000;

void loop() {
  unsigned long now = millis();

  // Eye display runs every ~30 ms for smooth animation
  handleIRSensor();
  handleTouchSensor();
  displayData();

  // Irrigation logic runs every 2 seconds (original timing)
  if (now - lastIrrigationCheck >= IRRIGATION_INTERVAL) {
    lastIrrigationCheck = now;
    readSoilMoisture();
    readDHTSensor();
    readWaterLevel();
    controlRelay();
    checkWaterLevelAlert();
  }

  delay(30);  // ~33 fps for eye animation
}
