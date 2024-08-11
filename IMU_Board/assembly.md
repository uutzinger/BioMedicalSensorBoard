# Assembly of IMU Board

Two sensor, the 9 axis IMU ICM20948 and barometric pressure sensor BMP581 need to be connected. Both are communicating through SPI.
The board contains a 1.8V voltage buck to power the IMU.

## Soldering

Attach color coded wires to the IO pads. E.g. red for power, black or green for ground and blue or white for digital input/output and yellow for analog wires. 

You can insert the wires into the holes (perpendicular) or or you can attach a short piece of the wires on top of the pad (perpendicular).

Suggested connecctions for the Sparkfun Thing Plus (USB-C) and the Adafruit Feather ESP32-S3 are given below.

### Connections

PAD       | Function              | Thing Plus  | Feather
---       |---                    |---          |---
**INT5**  | BMP 581 Interrupt     | 12 GIPO12   | 12 GPIO12   
**CS5**   | BMP 581 Chip Select   | 08 GPIO15   | 6
**SDO**   | SPI PO                | POCI GPIO19 | MI
**SDI**   | SPI PI                | PICO GPIO23 | MO
**SCL**   | SPI SCL               | SCK GPIO18  | SCK
**CS2**   | ICM 20948 Chip Select | 10 GPIO33   | 11 GPIO11
**INT2**  | ICM 20948 Interrupt   | 11 GPIO27   | 13 GPIO13
**VCC**   | Power                 | 3V3         | 3V3 
**GND**   | Power                 | GND         | GND

Require connections are in **bold**.

### Pinouts
- [Thing Plus C Pinout](https://cdn.sparkfun.com/assets/3/9/5/f/e/SparkFun_Thing_Plus_ESP32_WROOM_C_graphical_datasheet2.pdf)
- [ESP32 S3 Pinout](https://learn.adafruit.com/assets/110811)

![Thing Plus C Pinout](..\assets\ThingPlusC_PinOut.png)
![Adafruit Feather ESP32 S3](../assets/adafruit_products_Adafruit_Feather_ESP32-S3_Pinout.png)