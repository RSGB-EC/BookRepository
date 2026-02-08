/*
  BME280 + SSD1306 OLED + 3-LED Forecast (ESP32-C3, Arduino)

  Libraries:
    - Adafruit_BME280
    - Adafruit_SSD1306
    - Adafruit_GFX (dependency)
    - Wire

  Implements spec Rev C:
    - OLED init BEFORE BME280 init
    - Non-blocking millis() timing
    - Sensor reads >= 1 Hz
    - 5-minute pressure interval average stored into cyclic buffer (36 samples = 3 hours)
    - Trend slope based on oldest 6 vs newest 6 samples (hPa/hr)
    - LED forecast states; all LEDs ON if insufficient trend data (<12 valid samples)
    - Serial diagnostics at 115200 incl. I2C scan + addresses
    - OLED shows labels + units; cycles every 5 seconds
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include <math.h>

// -------------------- User Configuration --------------------
static const uint32_t SERIAL_BAUD = 115200;

// I2C pins per spec (default). Keep override-friendly.
static const int PIN_I2C_SDA = 8;
static const int PIN_I2C_SCL = 9;

// I2C clock per spec
static const uint32_t I2C_CLOCK_HZ = 100000;

// I2C addresses
static const uint8_t OLED_ADDR_DEFAULT = 0x3C;     // configurable
static const uint8_t BME_ADDR_DEFAULT  = 0x76;     // 0x76 or 0x77 configurable

// OLED geometry: set correctly for your panel
static const int OLED_WIDTH  = 128;
static const int OLED_HEIGHT = 64;

// LED pins (set to your wiring)
static const int PIN_LED_RED   = 2;
static const int PIN_LED_AMBER = 3;
static const int PIN_LED_GREEN = 4;

// LED polarity (document in wiring): true = active-high, false = active-low
static const bool LED_ACTIVE_HIGH = true;

// Timing
static const uint32_t SENSOR_READ_PERIOD_MS  = 1000;    // >= 1 Hz
static const uint32_t DISPLAY_CYCLE_MS       = 5000;    // change page every 5s
static const uint32_t PRESSURE_STORE_PERIOD_MS = 5UL * 60UL * 1000UL; // 5 minutes

// Retry behavior (non-blocking) if sensor read/init fails
static const uint32_t SENSOR_RETRY_PERIOD_MS = 5000; // reattempt every 5s

// Validity ranges (Rev C FR-345)
static const float TEMP_MIN_C = -40.0f;
static const float TEMP_MAX_C =  85.0f;
static const float HUM_MIN_PCT = 0.0f;
static const float HUM_MAX_PCT = 100.0f;
static const float PRES_MIN_HPA = 300.0f;
static const float PRES_MAX_HPA = 1100.0f;

// Trend thresholds (hPa/hour)
static const float SLOPE_RAPID_FALL = -1.0f;
static const float SLOPE_FALL       = -0.3f;
static const float SLOPE_STEADY_LO  = -0.3f;
static const float SLOPE_STEADY_HI  =  0.3f;
static const float SLOPE_RISE       =  1.0f;

// Pressure buffer
static const int PRESSURE_BUF_LEN = 36;  // 3 hours @ 5 min
static const int WINDOW_SAMPLES   = 6;   // 30 minutes @ 5 min
static const int MIN_VALID_FOR_FORECAST = 12; // per Rev C FR-400
// ------------------------------------------------------------

// OLED + BME objects
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
Adafruit_BME280 bme;

// State
enum class Page : uint8_t { TEMP = 0, HUM = 1, PRES = 2 };
static Page currentPage = Page::TEMP;

static bool oledOK = false;
static bool bmeOK  = false;

static uint8_t oledAddr = OLED_ADDR_DEFAULT;
static uint8_t bmeAddr  = BME_ADDR_DEFAULT;

// Latest readings
static float lastTempC = NAN;
static float lastHumPct = NAN;
static float lastPresHpa = NAN;
static bool  lastReadValid = false;

// Non-blocking timers
static uint32_t tLastSensorRead = 0;
static uint32_t tLastDisplaySwap = 0;
static uint32_t tLastPressureStoreTick = 0;
static uint32_t tLastSensorRetry = 0;

// 5-minute accumulation
static double presSum = 0.0;
static uint32_t presCount = 0;

// Pressure cyclic buffer with validity
static float presBuf[PRESSURE_BUF_LEN];
static bool  presValid[PRESSURE_BUF_LEN];
static int   presHead = 0;       // next write index
static int   presFilled = 0;     // number of samples written (<=36)

// Forecast info
static float lastSlope = NAN;

static inline void ledWrite(int pin, bool on) {
  bool level = LED_ACTIVE_HIGH ? on : !on;
  digitalWrite(pin, level ? HIGH : LOW);
}

static void setAllLEDs(bool red, bool amber, bool green) {
  ledWrite(PIN_LED_RED, red);
  ledWrite(PIN_LED_AMBER, amber);
  ledWrite(PIN_LED_GREEN, green);
}

static void initLEDs() {
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_AMBER, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  setAllLEDs(false, false, false);
}

static bool isValidTemp(float c) {
  return !isnan(c) && c >= TEMP_MIN_C && c <= TEMP_MAX_C;
}
static bool isValidHum(float p) {
  return !isnan(p) && p >= HUM_MIN_PCT && p <= HUM_MAX_PCT;
}
static bool isValidPres(float hpa) {
  return !isnan(hpa) && hpa >= PRES_MIN_HPA && hpa <= PRES_MAX_HPA;
}

static void i2cScan() {
  Serial.println(F("I2C scan:"));
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print(F("  - Found 0x"));
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (!found) Serial.println(F("  (none found)"));
}

static void renderTextCentered(const String &line1, const String &line2 = "", const String &line3 = "") {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  int16_t x1, y1;
  uint16_t w, h;

  int y = 0;
  const int lineH = 12;

  auto drawCenteredLine = [&](const String& s, int yPos, uint8_t size) {
    display.setTextSize(size);
    display.getTextBounds(s, 0, yPos, &x1, &y1, &w, &h);
    int x = (OLED_WIDTH - (int)w) / 2;
    display.setCursor(x < 0 ? 0 : x, yPos);
    display.print(s);
  };

  // Simple layout: title small, value large, optional footer small
  if (line3.length() == 0) {
    drawCenteredLine(line1, 0, 1);
    drawCenteredLine(line2, 22, 2);
  } else {
    drawCenteredLine(line1, 0, 1);
    drawCenteredLine(line2, 16, 2);
    drawCenteredLine(line3, 52, 1);
  }

  display.display();
}

static void renderError(const String &msg) {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("ERROR"));
  display.println(msg);
  display.display();
}

static bool tryInitOLED() {
  // Try default address first, then 0x3D
  uint8_t addrsToTry[2] = { OLED_ADDR_DEFAULT, (uint8_t)0x3D };
  for (int i = 0; i < 2; i++) {
    uint8_t addr = addrsToTry[i];
    // Adafruit_SSD1306 begin signature: begin(vccstate, i2caddr, reset, periphBegin)
    if (display.begin(SSD1306_SWITCHCAPVCC, addr, true, true)) {
      oledAddr = addr;
      oledOK = true;
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println(F("OLED OK"));
      display.print(F("Addr: 0x"));
      if (addr < 16) display.print('0');
      display.println(addr, HEX);
      display.display();
      return true;
    }
  }
  oledOK = false;
  return false;
}

static bool tryInitBME() {
  // Try configured address; if fails, try the other common one.
  uint8_t addrsToTry[2] = { bmeAddr, (uint8_t)(bmeAddr == 0x76 ? 0x77 : 0x76) };
  for (int i = 0; i < 2; i++) {
    uint8_t addr = addrsToTry[i];
    if (bme.begin(addr)) {
      bmeAddr = addr;
      bmeOK = true;

      // Optional: configure sampling (moderate, stable)
      // Adafruit_BME280 defaults are usually fine; set explicitly if desired:
      bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                      Adafruit_BME280::SAMPLING_X2,   // temp
                      Adafruit_BME280::SAMPLING_X16,  // pressure
                      Adafruit_BME280::SAMPLING_X1,   // humidity
                      Adafruit_BME280::FILTER_X16,
                      Adafruit_BME280::STANDBY_MS_500);

      return true;
    }
  }
  bmeOK = false;
  return false;
}

static void printBootDiagnostics() {
  Serial.println();
  Serial.println(F("=== BME280 + SSD1306 Forecast (ESP32-C3) ==="));
  Serial.print(F("I2C SDA: ")); Serial.println(PIN_I2C_SDA);
  Serial.print(F("I2C SCL: ")); Serial.println(PIN_I2C_SCL);
  Serial.print(F("I2C clock (Hz): ")); Serial.println(I2C_CLOCK_HZ);
  i2cScan();
}

static void bufferStorePressureSample(bool valid, float hpa) {
  presBuf[presHead] = hpa;
  presValid[presHead] = valid;
  presHead = (presHead + 1) % PRESSURE_BUF_LEN;
  if (presFilled < PRESSURE_BUF_LEN) presFilled++;
}

static int countValidSamples() {
  int count = 0;
  for (int i = 0; i < presFilled; i++) {
    // buffer indices: oldest at (presHead - presFilled)
    int idx = (presHead - presFilled + i);
    while (idx < 0) idx += PRESSURE_BUF_LEN;
    idx %= PRESSURE_BUF_LEN;
    if (presValid[idx]) count++;
  }
  return count;
}

// Get index of oldest sample currently in buffer
static int oldestIndex() {
  int idx = presHead - presFilled;
  while (idx < 0) idx += PRESSURE_BUF_LEN;
  return idx % PRESSURE_BUF_LEN;
}

static bool computeAverageWindow(int startIdx, int count, float &outAvg) {
  double sum = 0.0;
  int validCount = 0;
  int idx = startIdx;
  for (int i = 0; i < count; i++) {
    if (presValid[idx]) {
      sum += presBuf[idx];
      validCount++;
    } else {
      return false; // per FR-395: if any required sample invalid => insufficient
    }
    idx = (idx + 1) % PRESSURE_BUF_LEN;
  }
  if (validCount != count) return false;
  outAvg = (float)(sum / (double)count);
  return true;
}

static bool computeSlopeHpaPerHour(float &outSlope) {
  // Need at least 12 valid samples in buffer (FR-400)
  if (countValidSamples() < MIN_VALID_FOR_FORECAST) return false;
  if (presFilled < MIN_VALID_FOR_FORECAST) return false;

  // For slope definition, we use oldest 6 and newest 6 samples of the *current buffer*.
  // Old window: first 6 samples from oldest
  // Recent window: last 6 samples ending at newest
  if (presFilled < (WINDOW_SAMPLES * 2)) return false;

  int idxOldStart = oldestIndex();

  // Newest sample index is (presHead - 1)
  int newestIdx = presHead - 1;
  if (newestIdx < 0) newestIdx += PRESSURE_BUF_LEN;

  // Recent start = newestIdx - 5
  int idxRecentStart = newestIdx - (WINDOW_SAMPLES - 1);
  while (idxRecentStart < 0) idxRecentStart += PRESSURE_BUF_LEN;
  idxRecentStart %= PRESSURE_BUF_LEN;

  float pOld = NAN, pRecent = NAN;
  if (!computeAverageWindow(idxOldStart, WINDOW_SAMPLES, pOld)) return false;
  if (!computeAverageWindow(idxRecentStart, WINDOW_SAMPLES, pRecent)) return false;

  float dP = pRecent - pOld;
  float dT = 3.0f; // hours per spec (oldest ~3 hours ago in full buffer)
  // Note: When buffer isn't full, "oldest" isn't 3 hours ago; but we still require >=12 samples only.
  // For best fidelity, one could compute dT from (presFilled-1)*5min. Spec fixes dT=3.0h, so we follow spec.
  outSlope = dP / dT;
  return true;
}

static void updateForecastLEDs() {
  float slope;
  bool ok = computeSlopeHpaPerHour(slope);
  if (!ok) {
    // insufficient trend data => all ON
    setAllLEDs(true, true, true);
    lastSlope = NAN;
    return;
  }
  lastSlope = slope;

  if (slope <= SLOPE_RAPID_FALL) {
    setAllLEDs(true, false, false);
  } else if (slope > SLOPE_RAPID_FALL && slope <= SLOPE_FALL) {
    setAllLEDs(true, true, false);
  } else if (slope > SLOPE_STEADY_LO && slope < SLOPE_STEADY_HI) {
    setAllLEDs(false, true, false);
  } else if (slope >= SLOPE_STEADY_HI && slope < SLOPE_RISE) {
    setAllLEDs(false, true, true);
  } else { // slope >= +1.0
    setAllLEDs(false, false, true);
  }
}

static String fmtFloat(float v, int decimals) {
  if (isnan(v)) return String("N/A");
  return String(v, decimals);
}

static void renderCurrentPage() {
  if (!oledOK) return;

  if (!lastReadValid) {
    renderError("Sensor data invalid");
    return;
  }

  switch (currentPage) {
    case Page::TEMP: {
      String title = "Temperature";
      String val = fmtFloat(lastTempC, 1) + " C";
      renderTextCentered(title, val);
      break;
    }
    case Page::HUM: {
      String title = "Humidity";
      String val = fmtFloat(lastHumPct, 1) + " %";
      renderTextCentered(title, val);
      break;
    }
    case Page::PRES: {
      String title = "Pressure";
      String val = fmtFloat(lastPresHpa, 1) + " hPa";
      String footer;
      if (!isnan(lastSlope)) {
        footer = "slope " + fmtFloat(lastSlope, 2) + " hPa/hr";
      } else {
        footer = "slope N/A";
      }
      renderTextCentered(title, val, footer);
      break;
    }
  }
}

static void nextPage() {
  uint8_t p = (uint8_t)currentPage;
  p = (p + 1) % 3;
  currentPage = (Page)p;
}

static void handleSensorRead(uint32_t now) {
  if (!bmeOK) {
    lastReadValid = false;
    // Non-blocking retry for init
    if (now - tLastSensorRetry >= SENSOR_RETRY_PERIOD_MS) {
      tLastSensorRetry = now;
      Serial.println(F("[BME] Not initialized; retrying init..."));
      if (tryInitBME()) {
        Serial.print(F("[BME] OK at 0x"));
        if (bmeAddr < 16) Serial.print('0');
        Serial.println(bmeAddr, HEX);
      } else {
        Serial.println(F("[BME] Init failed"));
      }
    }
    return;
  }

  if (now - tLastSensorRead < SENSOR_READ_PERIOD_MS) return;
  tLastSensorRead = now;

  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure() / 100.0f; // Pa -> hPa

  bool vt = isValidTemp(t);
  bool vh = isValidHum(h);
  bool vp = isValidPres(p);

  lastTempC = t;
  lastHumPct = h;
  lastPresHpa = p;

  lastReadValid = (vt && vh && vp);

  if (!lastReadValid) {
    Serial.print(F("[BME] Invalid reading: T="));
    Serial.print(isnan(t) ? NAN : t);
    Serial.print(F(" H="));
    Serial.print(isnan(h) ? NAN : h);
    Serial.print(F(" P="));
    Serial.println(isnan(p) ? NAN : p);
  }

  // Accumulate pressure for 5-min average if valid pressure
  if (vp) {
    presSum += (double)p;
    presCount++;
  }

  // Update LEDs on every sensor read (fine, cheap)
  updateForecastLEDs();
}

static void handlePressureStoreTick(uint32_t now) {
  // Initialize tick baseline on first run
  if (tLastPressureStoreTick == 0) {
    tLastPressureStoreTick = now;
    return;
  }

  if (now - tLastPressureStoreTick < PRESSURE_STORE_PERIOD_MS) return;

  // Advance tick; keep it stable even if loop delayed
  while (now - tLastPressureStoreTick >= PRESSURE_STORE_PERIOD_MS) {
    tLastPressureStoreTick += PRESSURE_STORE_PERIOD_MS;
  }

  bool intervalValid = (presCount > 0);
  float avg = NAN;
  if (intervalValid) {
    avg = (float)(presSum / (double)presCount);
  }

  bufferStorePressureSample(intervalValid && isValidPres(avg), avg);

  Serial.print(F("[BUF] Stored 5-min avg: "));
  if (intervalValid) Serial.print(avg, 2);
  else Serial.print(F("INVALID"));
  Serial.print(F(" hPa | filled="));
  Serial.print(presFilled);
  Serial.print(F(" validCount="));
  Serial.println(countValidSamples());

  // Reset interval accumulation
  presSum = 0.0;
  presCount = 0;

  // Update LEDs after adding a new sample
  updateForecastLEDs();
}

static void handleDisplay(uint32_t now) {
  if (!oledOK) return;

  // If sensor invalid, show error and pause cycling (FR-355)
  if (!lastReadValid) {
    renderError("Sensor error");
    return;
  }

  if (now - tLastDisplaySwap >= DISPLAY_CYCLE_MS) {
    tLastDisplaySwap = now;
    nextPage();
  }
  renderCurrentPage();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(50);

  initLEDs();

  // I2C init explicitly (HR-4130)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_CLOCK_HZ);

  printBootDiagnostics();

  // Init OLED first (HR-4120)
  Serial.println(F("[OLED] Initializing..."));
  if (tryInitOLED()) {
    Serial.print(F("[OLED] OK at 0x"));
    if (oledAddr < 16) Serial.print('0');
    Serial.println(oledAddr, HEX);
    Serial.print(F("[OLED] Resolution: "));
    Serial.print(OLED_WIDTH);
    Serial.print('x');
    Serial.println(OLED_HEIGHT);
  } else {
    Serial.println(F("[OLED] Init FAILED"));
    oledOK = false;
  }

  // Then init BME280
  Serial.println(F("[BME] Initializing..."));
  if (tryInitBME()) {
    Serial.print(F("[BME] OK at 0x"));
    if (bmeAddr < 16) Serial.print('0');
    Serial.println(bmeAddr, HEX);
  } else {
    Serial.println(F("[BME] Init FAILED"));
    bmeOK = false;
  }

  // Show boot status on OLED if available
  if (oledOK) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("Boot"));
    display.print(F("OLED: ")); display.println(oledOK ? F("OK") : F("FAIL"));
    display.print(F("BME:  ")); display.println(bmeOK ? F("OK") : F("FAIL"));
    display.print(F("SDA/SCL: "));
    display.print(PIN_I2C_SDA);
    display.print(F("/"));
    display.println(PIN_I2C_SCL);
    display.display();
  }

  // Initialize buffer validity
  for (int i = 0; i < PRESSURE_BUF_LEN; i++) {
    presBuf[i] = NAN;
    presValid[i] = false;
  }

  // Initialize timers
  uint32_t now = millis();
  tLastSensorRead = now;
  tLastDisplaySwap = now;
  tLastPressureStoreTick = now;
  tLastSensorRetry = now;

  // Initialize initial page render
  currentPage = Page::TEMP;

  // Initialize LEDs based on current (insufficient) state
  updateForecastLEDs();
}

void loop() {
  uint32_t now = millis();

  handleSensorRead(now);
  handlePressureStoreTick(now);
  handleDisplay(now);

  // Yield / avoid ultra-tight loop (helps watchdog friendliness)
  delay(1);
}
