# Assembly of Stethoscope Board

## Soldering

Attach color coded wires to the IO pads. E.g. red for power, black or green for ground and blue or white for digital input/output and yellow for analog wires. 

You can insert the wires into the holes (perpendicular) or or you can attach a short piece of the wires on top of the pad (perpendicular).

Suggested connecctions for the Sparkfun Thing Plus (USB-C) and the Adafruit Feather ESP32-S3 are given below.

### Connections

PAD       | Function        | Thing Plus    | Feather
---       |---              |---            |---
**GND**   | Ground          | GND           | GND   
**3.3V**  | Power           | 3V3           | 3V3
**SDA**   | SDA / CDATA     | SDA  / GPIO21 | SDA  / GPIO3
**SCL**   | SCL / CCLK      | SCL  / GPIO22 | SCL  / GPIO4
**DOUT**  | Data Out        | POCI / GPIO19 | MISO / GPIO37
**LRCLK** | WS/Word Clock   | A5   / GPIO35 | A5   / GPIO8 / ADC1-CH7
**DIN**   | Data In / DSDIN | PICO / GPIO23 | MOSI / GPIO35
**SLCK**  | Bit Clock       | SCK  / GPIO18 | SCK  / GPIO36
**MCLK**  | Master Clock    | LED  / GPIO13 | A4   / GPIO14 / ADC2-CH3
**3.3V**  | Power           | 3V3           | 3V3

Require connections are in **bold**.

### Pinouts
[Feather ESP32 S3 pinout](https://cdn-learn.adafruit.com/assets/assets/000/110/811/original/adafruit_products_Adafruit_Feather_ESP32-S3_Pinout.png)

[Sparkfun ESP32 Thing Plus C pinout](https://cdn.sparkfun.com/assets/3/9/5/f/e/SparkFun_Thing_Plus_ESP32_WROOM_C_graphical_datasheet2.pdf)
