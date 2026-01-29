/*
  BME280 + 0.96" SSD1306 OLED (I2C) on STM32 NUCLEO-32 L432KC (STM32duino)

  Features:
  - Reads temperature (°C), humidity (%RH), pressure (hPa)
  - Non-blocking display rotation every 5 seconds (millis-based)
  - Sensor sampling at least once per second
  - Serial diagnostics at 115200
  - Graceful error handling (sensor/OLED init failure)

  Libraries (Arduino Library Manager):
  - Adafruit BME280 Library
  - Adafruit Unified Sensor
  - Adafruit GFX Library
  - Adafruit SSD1306

  Wiring (I2C shared bus):
  - BME280: 3V3, GND, SDA, SCL
  - OLED:   3V3, GND, SDA, SCL
  Notes:
  - Use 3.3V logic/power.
  - I2C pins: use STM32duino default I2C pins for NUCLEO-32 L432KC.
*/

#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ------------------------ Configuration ------------------------

// I2C addresses
// BME280 can be 0x76 or 0x77 depending on SDO pin / module wiring.
#define BME280_I2C_ADDR   0x76

// SSD1306 commonly 0x3C; some modules are 0x3D.
#define OLED_I2C_ADDR     0x3C

// OLED geometry (most 0.96" modules are 128x64; some are 128x32)
#define SCREEN_WIDTH      128
#define SCREEN_HEIGHT     64

// Timing
static const uint32_t SENSOR_INTERVAL_MS = 1000; // >=1 Hz sampling
static const uint32_t SCREEN_INTERVAL_MS = 5000; // rotate every 5s

// Sea level pressure for altitude calculation (not required here)
// #define SEALEVELPRESSURE_HPA (1013.25)

// ------------------------ Globals ------------------------

Adafruit_BME280 bme; // I2C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

enum ScreenMode : uint8_t {
  SCREEN_TEMP = 0,
  SCREEN_HUM  = 1,
  SCREEN_PRES = 2,
  SCREEN_COUNT = 3
};

bool bmeOk = false;
bool oledOk = false;

float lastTempC = NAN;
float lastHumPct = NAN;
float lastPresHpa = NAN;
uint32_t lastGoodReadMs = 0;

ScreenMode screenMode = SCREEN_TEMP;

uint32_t lastSensorMs = 0;
uint32_t lastScreenMs = 0;

// ------------------------ Helpers ------------------------

static void serialBanner() {
  Serial.println();
  Serial.println(F("=== BME280 + SSD1306 OLED Demo (STM32duino) ==="));
  Serial.print(F("BME280 I2C address: 0x"));
  Serial.println(BME280_I2C_ADDR, HEX);
  Serial.print(F("OLED  I2C address: 0x"));
  Serial.println(OLED_I2C_ADDR, HEX);
  Serial.println(F("Serial: 115200"));
}

static void oledPrintCentered(const __FlashStringHelper* line1,
                              const __FlashStringHelper* line2 = nullptr,
                              const __FlashStringHelper* line3 = nullptr) {
  if (!oledOk) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Simple vertical layout
  display.setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;

  // Draw up to 3 lines
  int y = 16;

  auto drawLine = [&](const __FlashStringHelper* s, int yPos) {
    if (!s) return;
    display.getTextBounds((const __FlashStringHelper*)s, 0, yPos, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - (int)w) / 2;
    display.setCursor(x < 0 ? 0 : x, yPos);
    display.print(s);
  };

  drawLine(line1, y);
  drawLine(line2, y + 14);
  drawLine(line3, y + 28);

  display.display();
}

static void oledInit() {
  // Some SSD1306 libs want VCC mode. Most I2C modules use SSD1306_SWITCHCAPVCC.
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    oledOk = true;
    Serial.println(F("[OK] OLED initialized"));
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("OLED OK"));
    display.display();
  } else {
    oledOk = false;
    Serial.println(F("[FAIL] OLED init failed (check wiring/address)"));
  }
}

static void bmeInit() {
  // Use explicit address so it matches requirement of configurability.
  if (bme.begin(BME280_I2C_ADDR)) {
    bmeOk = true;
    Serial.println(F("[OK] BME280 initialized"));
  } else {
    bmeOk = false;
    Serial.println(F("[FAIL] BME280 init failed (check wiring/address)"));
  }
}

static bool readSensor() {
  if (!bmeOk) return false;

  // Adafruit_BME280 returns floats; may yield NAN if something is wrong.
  float t = bme.readTemperature();               // °C
  float h = bme.readHumidity();                  // %
  float p = bme.readPressure() / 100.0F;         // Pa -> hPa

  bool valid = isfinite(t) && isfinite(h) && isfinite(p);

  if (valid) {
    lastTempC = t;
    lastHumPct = h;
    lastPresHpa = p;
    lastGoodReadMs = millis();

    Serial.print(F("T="));
    Serial.print(lastTempC, 2);
    Serial.print(F(" C, H="));
    Serial.print(lastHumPct, 1);
    Serial.print(F(" %, P="));
    Serial.print(lastPresHpa, 1);
    Serial.println(F(" hPa"));
  } else {
    Serial.println(F("[WARN] Invalid BME280 reading"));
  }

  return valid;
}

static void drawScreenTemp() {
  if (!oledOk) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Temperature"));

  display.setTextSize(2);
  display.setCursor(0, 22);

  if (isfinite(lastTempC)) {
    display.print(lastTempC, 1);
    display.print((char)247); // degree symbol-ish; may vary by font
    display.print(F("C"));
  } else {
    display.print(F("--.- C"));
  }

  display.setTextSize(1);
  display.setCursor(0, 54);
  if (bmeOk) {
    display.print(F("BME280 OK"));
  } else {
    display.print(F("BME280 FAIL"));
  }

  display.display();
}

static void drawScreenHum() {
  if (!oledOk) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Humidity"));

  display.setTextSize(2);
  display.setCursor(0, 22);

  if (isfinite(lastHumPct)) {
    display.print(lastHumPct, 1);
    display.print(F(" %"));
  } else {
    display.print(F("--.- %"));
  }

  display.setTextSize(1);
  display.setCursor(0, 54);
  if (bmeOk) {
    display.print(F("BME280 OK"));
  } else {
    display.print(F("BME280 FAIL"));
  }

  display.display();
}

static void drawScreenPres() {
  if (!oledOk) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Pressure"));

  display.setTextSize(2);
  display.setCursor(0, 22);

  if (isfinite(lastPresHpa)) {
    display.print(lastPresHpa, 1);
    display.println(F(" hPa"));
  } else {
    display.print(F("----.-"));
    display.println(F(" hPa"));
  }

  display.setTextSize(1);
  display.setCursor(0, 54);
  if (bmeOk) {
    display.print(F("BME280 OK"));
  } else {
    display.print(F("BME280 FAIL"));
  }

  display.display();
}

static void updateDisplay() {
  if (!oledOk) return;

  // If sensor is not OK, show an error screen (still rotate not needed)
  if (!bmeOk) {
    oledPrintCentered(F("SENSOR ERROR"), F("BME280 init fail"), F("Check wiring"));
    return;
  }

  // If we haven't had a good read recently, show data error
  const uint32_t now = millis();
  if (lastGoodReadMs == 0 || (now - lastGoodReadMs) > (SENSOR_INTERVAL_MS * 5UL)) {
    oledPrintCentered(F("DATA ERROR"), F("No valid reading"), F("Check sensor"));
    return;
  }

  switch (screenMode) {
    case SCREEN_TEMP: drawScreenTemp(); break;
    case SCREEN_HUM:  drawScreenHum();  break;
    case SCREEN_PRES: drawScreenPres(); break;
    default:          drawScreenTemp(); break;
  }
}

static void rotateScreen() {
  screenMode = (ScreenMode)((screenMode + 1) % SCREEN_COUNT);
}

// ------------------------ Arduino ------------------------

void setup() {
  Serial.begin(115200);
  // Some STM32 boards need a moment for Serial to come up (optional).
  delay(50);

  serialBanner();

  Wire.begin(); // use default I2C pins from STM32duino core

  // Init peripherals
  oledInit();
  bmeInit();

  // Initial UI
  if (oledOk) {
    if (bmeOk) {
      oledPrintCentered(F("Initializing..."), F("BME280 + OLED"), F("Starting"));
    } else {
      oledPrintCentered(F("SENSOR ERROR"), F("BME280 init fail"), F("See Serial"));
    }
  }

  // Force immediate first read and draw
  lastSensorMs = 0;
  lastScreenMs = 0;
}

void loop() {
  const uint32_t now = millis();

  // Sensor sampling (>= 1 Hz)
  if (now - lastSensorMs >= SENSOR_INTERVAL_MS) {
    lastSensorMs = now;

    if (bmeOk) {
      readSensor();
    }
  }

  // Screen rotation every 5 seconds (non-blocking)
  if (now - lastScreenMs >= SCREEN_INTERVAL_MS) {
    lastScreenMs = now;
    rotateScreen();
  }

  // Refresh display frequently (lightweight, avoids sluggish UI)
  // You can throttle this if desired; SSD1306 redraw is fast enough for simple screens.
  updateDisplay();
}
