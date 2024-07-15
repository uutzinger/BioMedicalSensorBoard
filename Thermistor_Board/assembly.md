# Assembly of Thermistor Board

## Soldering

Attach color coded wires to the IO pads. E.g. red for power, black or green for ground and blue or white for digital input/output and yellow for analog wires. 

You can insert the wires into the holes (perpendicular) or or you can attach a short piece of the wires on top of the pad (perpendicular).

Suggested connecctions for the Sparkfun Thing Plus (USB-C) and the Adafruit Feather ESP32-S3 are given below.

### Connections

PAD      | Function    | Thing Plus  | Feather
---      |---          |---          |---
**A5**   | analog CH1A | A5 ADC1-CH7 | D9  ADC1-CH8
**A4**   | analog CH1B | A4 ADC1-CH0 | D10 ADC1-CH9
**A3**   | analog CH2A | A3 ADC1-CH3 | D11 ADC2-CH0
**A2**   | analog CH2B | A2 ADC1-CH6 | D12 ADC2-CH1
**A1**   | analog CH3A | 06 ADC1-CH4 | D5  ADC1-CH4
**A0**   | analog CH3B | 10 ADC1-CH5 | D6  ADC1-CH5
**GND**  | Ground      | GND         | GND
**3.3V** | Power       | 3V3         | 3V3

Require connections are in **bold**.

### Pinouts
[Feather ESP32 S3 pinout](https://cdn-learn.adafruit.com/assets/assets/000/110/811/original/adafruit_products_Adafruit_Feather_ESP32-S3_Pinout.png)

[Sparkfun ESP32 Thing Plus C pinout](https://cdn.sparkfun.com/assets/3/9/5/f/e/SparkFun_Thing_Plus_ESP32_WROOM_C_graphical_datasheet2.pdf)

## Calibration

The resistors are configured the following way:

<img src="../assetts/Wheatstone.svg" alt="drawing" height="300"/>

made with https://www.circuit-diagram.org/editor

The Wheatstone bridge resistors should be measured for greater accuracy.

- TP1 - TP3 R1
- TP1 - TP2 R2
- TP2 - TP3 R3

The thermistor resitance is

$R_{Thermistor} = \frac{R_3 (V_{in} R_2 - V_{diff} (R_1+R_2))}{Vin R1 + Vdiff (R_1+R_2)}$

Where $V_{diff} = V_1 - V_2$

$V_1$ and $V_2$ are measured with a microcontroller's ADC converter and they correspond to A0-A1, A2-A3 and A4-A5. ESP ADC is of low quality and linarization and averaging in software is needed.

After measuring the three resistors of each of the channels we need to close all jumpers in the circuit. There are 11 jumpers. 