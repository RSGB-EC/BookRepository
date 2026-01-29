# BME280 Interface – Software & Hardware Specification

**Document ID:** BME-SPEC-001
 **Revision:** B
 **Target Platform:** NUCLEO-32 L432KC (STM32duino Core)
 **Sensor:** BME280
 **Display:** OLED 0.96" (SSD1306)

------

## 1. Purpose

The system shall measure barometric pressure, relative humidity, and temperature using a BME280 sensor and provide a cyclic display of these values on a 0.96" OLED screen.

------

## 2. System Overview

The system consists of:

- BME280 environmental sensor
- STM32 NUCLEO-32 L432KC microcontroller board
- 0.96" SSD1306-based OLED display
- Firmware developed using Arduino IDE with STM32duino (Arduino-compatible) core

Firmware shall use Arduino framework APIs provided by the STM32duino core.

------

## 3. Functional Requirements

**FR-001**
 The system **shall** read barometric pressure, relative humidity, and temperature from the BME280 sensor.

**FR-002**
 The system **shall** display:

- Temperature in °C
- Relative humidity in %
- Pressure in hPa

**FR-003**
 The system **shall** cycle sequentially through temperature → humidity → pressure, changing the displayed parameter every 5 seconds.

**FR-004**
 The display timing **shall** be implemented using non-blocking, millis()-based timing.

**FR-005**
 The system **shall** update sensor readings at least once per second.

**FR-006**
 The system **shall** display an error message if valid sensor data is unavailable.

**FR-007**
 All peripherals **shall** operate at 3.3V logic levels.

------

## 4. Hardware Interface Requirements

**HR-010**
 Firmware **shall** report initialization success or failure for each peripheral via Serial Monitor at 115200 baud.

------

### 4.1 I2C Bus

**HR-100**
 The system **shall** use a single hardware I2C bus shared by the BME280 and OLED.

**HR-101**
 Firmware **shall** support multiple devices on the same I2C bus.

------

### 4.2 BME280 Interface

**HR-110**
 The BME280 **shall** interface to the MCU using hardware I2C.

**HR-120**
 The BME280 I2C address **shall** be configurable between 0x76 and 0x77.

**HR-130**
 Firmware **shall** use a BME280-compatible Arduino library supporting I2C communication.

------

### 4.3 OLED Interface

**HR-210**
 The OLED **shall** interface to the MCU using hardware I2C.

**HR-220**
 Firmware **shall** use an SSD1306-compatible OLED library.

**HR-230**
 The OLED I2C address **shall** default to 0x3C and be configurable if required.

------

### 4.4 MCU Pin Mapping

**HR-300**
 I2C pins **shall** use the default hardware I2C pins defined by the STM32duino core for the NUCLEO-32 L432KC.

**HR-310**
 Actual pin assignments **shall** be documented in the wiring diagram deliverable.

------

## 5. Software Environment

**SR-110**
 Firmware **shall** be developed using Arduino IDE.

**SR-120**
 Target board **shall** be set to NUCLEO-32 L432KC.

**SR-130**
 STM32 boards package by STMicroelectronics **shall** be installed via Arduino Boards Manager.

**SR-140**
 Selected libraries and tested versions **shall** be documented.

------

## 6. Error Handling & Diagnostics

**ER-110**
 If BME280 initialization fails, firmware **shall** report the error via Serial and display a sensor error message on the OLED.

**ER-120**
 If OLED initialization fails, firmware **shall** report the error via Serial.

**ER-130**
 If sensor readings return invalid values, firmware **shall** retain last valid reading or display an error message.

------

## 7. Deliverables

**DR-110**
 Complete Arduino-compatible source code.

**DR-120**
 Text-based wiring diagram showing all electrical connections and pin assignments.

**DR-130**
 List of required libraries and tested versions.

**DR-140**
 Brief README describing build and upload procedure.
