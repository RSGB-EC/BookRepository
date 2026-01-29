# RF Power Meter – Software & Hardware Specification

**Document ID:** RPM-SPEC-001
 **Revision:** A
 **Target Platform:** NUCLEO-32 L432KC
 **Sensor:** AD8318 RF Log Detector
 **Display:** ILI9341 2.8" TFT

------

## 1. Purpose

The system shall measure RF input power using an AD8318 logarithmic detector and display the measured power in dBm on an ILI9341-based TFT display.

------

## 2. System Overview

The system consists of:

- AD8318 RF detector module
- STM32 NUCLEO-32 L432KC microcontroller board
- ILI9341-based 2.8" TFT display
- Firmware developed using Arduino IDE

------

## 3. Functional Requirements

### 3.1 RF Signal Acquisition

**FR-001**
 The system shall accept an RF input signal through the AD8318 RF detector module.

**FR-002**
 The AD8318 output voltage shall be connected to microcontroller analog input pin A0.

**FR-003**
 The firmware shall sample the AD8318 output voltage using the MCU ADC.

**FR-004**
 The ADC resolution shall be configured to 12 bits.

**FR-005**
 The ADC reference voltage shall be 3.3 V.

**Acceptance Criteria**

- ADC readings range from 0 to 4095.
- `analogReadResolution(12)` is invoked during initialization.

------

### 3.2 Sampling and Filtering

**FR-006**
 The system shall acquire exactly 500 ADC samples per measurement cycle.

**FR-007**
 The firmware shall apply a trimmed mean filter by:

a) Sorting the 500 samples in ascending order
 b) Discarding the lowest 10% (50 samples)
 c) Discarding the highest 10% (50 samples)
 d) Averaging the remaining 400 samples

**Acceptance Criteria**

- Total retained samples = 400
- Computed value equals arithmetic mean of retained samples

------

### 3.3 Conversion to dBm

**FR-008**
 The filtered ADC value shall be converted to RF power using:

```
dBm = (0.0326 × ADCValue) – 94.937
```

**FR-009**
 The computed dBm value shall be rounded to the nearest 0.2 dBm.

**FR-010**
 The rounded result shall be stored in a variable named:

```
float dBmDisplayValue
```

**Acceptance Criteria**

- A unit test using known ADC inputs produces expected dBm outputs.
- Rounding uses half-up rounding (e.g., 0.1 → down, 0.11 → up).

------

### 3.4 Display Output

**FR-011**
 The system shall display the dBm value numerically on the TFT display.

**FR-012**
 The firmware shall call:

```
DrawdBmCircles();
```

using the global variable `dBmDisplayValue`.

**FR-013**
 The TFT interface shall use the Adafruit_ILI9341 library.

**Acceptance Criteria**

- Display updates within one measurement cycle.
- Displayed value matches `dBmDisplayValue`.

------

## 4. Hardware Interface Requirements

### 4.1 Display Connections

**HR-001**
 The ILI9341 display shall use hardware SPI.

**HR-002**
 Pin assignments:

| Function | Arduino Pin  |
| -------- | ------------ |
| DC       | D9           |
| CS       | D10          |
| MOSI     | Hardware SPI |
| MISO     | Hardware SPI |
| SCK      | Hardware SPI |

------

### 4.2 Sensor Connection

**HR-003**
 AD8318 output shall connect to A0.

**HR-004**
 AD8318 supply voltage shall not exceed 3.3 V.

**Acceptance Criteria**

- Measured voltage at A0 never exceeds 3.3 V.

------

## 5. Software Environment

**SR-001**
 Firmware shall be developed using Arduino IDE.

**SR-002**
 Target board shall be set to NUCLEO-32 L432KC.

------

## 6. Performance Requirements

**PR-001**
 One complete measurement cycle (500 samples + processing + display update) shall complete within 250 ms.

**PR-002**
 Displayed dBm value shall be stable within ±0.2 dBm when input RF level is constant.

------

## 7. Deliverables

**DR-001**
 Complete Arduino source code.

**DR-002**
 Wiring diagram showing all electrical connections.

------

## 8. Verification Matrix (Excerpt)

| Requirement | Verification Method |
| ----------- | ------------------- |
| FR-006      | Code inspection     |
| FR-007      | Unit test           |
| FR-008      | Calculation test    |
| HR-001      | Hardware inspection |
| PR-001      | Timing measurement  |