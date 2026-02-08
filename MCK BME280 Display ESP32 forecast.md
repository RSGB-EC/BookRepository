# BME280 Interface – Software & Hardware Specification (Updated)

**Document ID:** BME-SPEC-001
 **Revision:** C
 **Target Platform:** ESP32-C3 MakerGo Super Mini (Espressif Arduino Core)
 **Sensor:** BME280 (I2C)
 **Display:** OLED 0.96" SSD1306 (I2C)
 **Arduino Libraries (selected):**

- **Adafruit_BME280**
- **Adafruit_SSD1306** (with **Adafruit_GFX** dependency)

------

## Purpose

The system shall measure barometric pressure, relative humidity, and temperature using a BME280 sensor and provide a cyclic display of these values on a 0.96" OLED screen.

The system shall store a barometric pressure reading every 5 mins in a cyclic store containing a maximum of 3 hours of readings. The barometric pressure trend shall be used to light 3 LEDs to provide an indication of the weather forecast.

------

## System Overview

The system consists of:

- BME280 environmental sensor
- ESP32-C3 super mini microcontroller board
- 0.96" SSD1306-based OLED display
- 3 LEDs; Red, Amber and Green
- Firmware developed using Arduino IDE with Espressif (Arduino-compatible) core
- Firmware uses Adafruit_BME280 and Adafruit_SSD1306 libraries

------

## Functional Requirements

**FR-300**
 The system **shall** read barometric pressure, relative humidity, and temperature from the BME280 sensor.

**FR-310**
 The system **shall** display:

- Temperature in °C
- Relative humidity in %
- Pressure in hPa

**FR-320**
 The system **shall** cycle sequentially through temperature → humidity → pressure, changing the displayed parameter every 5 seconds and **shall show a label and unit** for the currently displayed parameter.

**FR-330**
 The display timing **shall** be implemented using non-blocking, millis()-based timing.

**FR-340**
 The system **shall** update sensor readings at least once per second. Sensor readings may be updated independently of display cycling and stored for display use.

**FR-345 (NEW)**
 The system **shall** define “valid sensor data” as:

- readings are **not NaN**, and
- pressure is within **300–1100 hPa**, humidity within **0–100 %**, temperature within **-40 to +85 °C**.
   If any displayed parameter is invalid, the system shall treat sensor data as unavailable for FR-350/ER-620 purposes.

**FR-350**
 The system **shall** display an error message if valid sensor data is unavailable.

**FR-355 (NEW)**
 When a sensor error message is being displayed, the system **shall** pause normal display cycling and shall reattempt sensor reads periodically while keeping non-blocking timing.

**FR-360**
 All peripherals **shall** operate at 3.3V logic levels.

**FR-370**
 On power-up or reset, firmware shall initialize peripherals and begin normal operation without user input.

**FR-380**
 The system shall store a pressure reading every 5 mins in a cyclic store containing a maximum of 36 readings (3 hours worth) and each stored pressure sample shall be the **average pressure measured over the preceding 5-minute interval** and shall overwrite the oldest sample with a new reading once more than 36 5 min samples have been collected.

**FR-385 (NEW)**
 The 5-minute interval average shall be computed from the 1 Hz (or faster) pressure updates collected during that interval. If insufficient valid readings exist within an interval to compute an average, that interval sample shall be marked invalid.

**FR-390**
 The system shall implement a moving average trend calculation to determine the pressure slope to create a basic weather forecast.

`P_recent` = average of last 30 minutes (last 6 samples)
 `P_old` = average of first 30 minutes in your buffer (oldest 6 samples)

```
ΔP = P_recent - P_old
```

`Δt = 3.0 hours` (since oldest is ~3 hours ago)
 `slope = ΔP / Δt` (hPa/hour)

`slope <= -1.0 hPa/hour` → **Rapidly falling** → “Stormy / rain likely” -> light RED LED
 `-1.0 < slope <= -0.3` → **Falling** → “Worsening / possible rain” -> light RED and AMBER LED
 `-0.3 < slope < +0.3` → **Steady** → “Stable” -> Light AMBER LED
 `+0.3 <= slope < +1.0` → **Rising** → “Improving” -> Light AMBER and GREEN LED
 `slope >= +1.0` → **Rapidly rising** → “Clearing / fair” -> light Green LED

**FR-395 (NEW)**
 Trend calculations shall use only **valid** stored pressure samples. If any required sample in the window is invalid, trend data shall be considered insufficient (FR-400).

**FR-400**
 When there is insufficient trend data available to make a forecast all three LEDs shall be lit where insufficient data shall be defined as **fewer than 12 valid pressure samples** in the cyclic buffer.

------

## Hardware Interface Requirements

**HR-400**
 Firmware **shall** report initialization success or failure for each peripheral via Serial Monitor at 115200 baud.

**HR-405 (NEW)**
 Firmware **shall** print I2C configuration and device discovery results at boot, including:

- SDA pin, SCL pin, I2C bus speed
- Detected I2C addresses for OLED and BME280 (or “not found”)

**HR-410**
 The ESP32-C3 onboard 3.3V regulator shall supply power to the BME280 and OLED modules.

**HR-420**
 I2C bus speed shall default to 100kHz unless otherwise specified.

**HR-425 (NEW)**
 If I2C communication errors are detected (timeouts / repeated NACKs), firmware shall retain 100kHz operation and shall not increase bus speed automatically.

**HR-430**
 The system shall interface to 3 LEDs via GPIO pins.

**HR-435 (NEW)**
 LED control shall allow multiple LEDs to be simultaneously ON per FR-390. LED polarity (active-high or active-low) shall be documented in DR-710.

------

### I2C Bus

**HR-4100**
 The system **shall** use a single hardware I2C bus shared by the BME280 and OLED.

**HR-4110**
 Firmware **shall** support multiple devices on the same I2C bus.

**HR-4120**
 Firmware **shall** initialise the OLED before the BME280.

**HR-4130 (NEW)**
 Firmware shall initialise I2C explicitly using `Wire.begin(SDA, SCL)` and shall not rely on implicit board defaults.

------

### BME280 Interface

**HR-4200**
 The BME280 **shall** interface to the MCU using hardware I2C.

**HR-4210**
 The BME280 I2C address **shall** be configurable between 0x76 and 0x77.

**HR-4220 (UPDATED)**
 Firmware **shall** use the **Adafruit_BME280** Arduino library for I2C communication.

**HR-4230 (NEW)**
 Firmware shall log the chosen BME280 address at startup and shall report an explicit error if the sensor is not detected at the configured address.

------

### OLED Interface

**HR-4300**
 The OLED **shall** interface to the MCU using hardware I2C.

**HR-4310 (UPDATED)**
 Firmware **shall** use the **Adafruit_SSD1306** Arduino library (and Adafruit_GFX dependency).

**HR-4320**
 The OLED I2C address **shall** default to 0x3C and be configurable if required.

**HR-4330 (NEW)**
 OLED resolution shall be explicitly configured in firmware and documented in DR-720 (e.g., 128×64 or 128×32). Firmware shall report a clear error if OLED initialization fails.

------

### MCU Pin Mapping

**HR-4400**
 I2C pins **shall** use GPIO 8 (SDA) and GPIO 9 (SCL) as the I2C pins.

**HR-4405**
 GPIO pins for the LEDs shall be documented in the wiring diagram deliverable.

**HR-4410**
 Actual pin assignments **shall** be documented in the wiring diagram deliverable.

**HR-4415 (NEW)**
 If alternate I2C pins are required due to hardware variance, they shall be documented in DR-710 and the firmware configuration.

------

## Software Environment

**SR-500**
 Firmware **shall** be developed using Arduino IDE.

**SR-510**
 Target board **shall** be set to ESP32-C3 Super Mini.

**SR-520**
 ESP32 Board Support Package by Espressif Systems **shall** be installed via Arduino Boards Manager.

**SR-530**
 Selected libraries and tested versions **shall** be documented.

**SR-535 (NEW)**
 The documented library list shall include:

- Adafruit_BME280 version
- Adafruit_SSD1306 version
- Adafruit_GFX version
- ESP32 Arduino core package version

------

## Error Handling & Diagnostics

**ER-600**
 If BME280 initialization fails, firmware **shall** report the error via Serial and display a sensor error message on the OLED.

**ER-610**
 If OLED initialization fails, firmware **shall** report the error via Serial.

**ER-615 (NEW)**
 If OLED initialization fails, the firmware shall continue running without OLED output and shall continue to provide diagnostics via Serial.

**ER-620**
 If sensor readings return invalid values, firmware **shall** display an error message.

**ER-630 (NEW)**
 Firmware shall avoid blocking retry loops; all retries (sensor re-init/read) shall be performed using non-blocking timing.

------

## Deliverables

**DR-700**
 Complete Arduino-compatible source code.

**DR-710**
 Text-based wiring diagram showing all electrical connections and pin assignments using GPIO 8 (SDA) and GPIO 9 (SCL), and documenting LED GPIO pins and LED polarity.

**DR-720**
 List of required libraries and tested versions, including Adafruit_BME280, Adafruit_SSD1306, Adafruit_GFX, and ESP32 Arduino core package version, plus OLED resolution setting.

**DR-730**
 Brief README describing build and upload procedure, including any ESP32-C3 boot/upload button steps if required, plus a simple bring-up checklist (I2C scan + OLED init + BME280 init).
