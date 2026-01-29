# RF Power Meter

## Project Goal

To create an RF power meter using an AD8318 module, Nucleo-32 L432KC microcontroller board and an ILI9341 based 2.8" display.

## Hardware

The RF signal to be measured shall be connected to the input the AD8318 module will output a voltage representing the RF power level into an analog input of an NUCLEO-32 L432KC board.

There shall be an ILI9341 based 2.8" screen for graphics and numeric display.

Use the hardware SPI plus D9 for DC and D10 for CS on the ILI9341.

Use analog input A0 for the AD8318 connection.

## Software

The software shall be created for the Arduino IDE and targeted at the Nucleo-32 L432KC microcontroller board.

The software shall take 500 samples of the AD8318 output and apply a Trimmed Mean algorithm to remove the top and bottom 10% of the readings and then calculate the average of the remaining sample values. The ADC shall be 12-bit resolution and 3V3 referenced. 

The calculated ADC value shall be converted into dBm using dBm = (0.0326 * ADCValue) - 94.937 and rounded to the nearest 0.2 dBm and placed in a variable called dBmDisplayValue which shall be type float.

The final dBm value shall be utilised by the attached routines for graphical display on the ILI9341. This will be achieved by calling routine DrawdBmCircles with the dBm value in the global variable dBmDisplayValue which is type float. The interface to the ILI9341 shall utilise the Adafruit_ILI9341library which is also used by the attached routine.

## Outputs

Produce full code and a wiring diagram.
