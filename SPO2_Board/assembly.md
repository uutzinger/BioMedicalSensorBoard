# Assembly of SPO2 Board
## Soldering
### DB 9 Connector
Firmly press the connector into the holes and solder all pins. Add solder to the mechanical retaining hooks.
### Wires
Attach color coded wires to the IO pads.

It is not necessary to push the wires into the holes and you can simply strip a short piece of insulation and solder the wires onto the pad.

Suggested connecctions for the Sparkfun Thing Plus (USB-C) and the Adafruit Feather ESP32-S3.

PAD | Function | Thing Plus | Feather
---|---|---|---
**VCC**   | Power                     | 3V3         | 3V3 
Start     | Reset active low          | 04   GPIO14 | 10 GPIO10
**RDY**   | ADC Data Ready            | 12   GIPO12 | 13 GPIO13
**CS0**   | Chip Select               | 08   GPIO15 | 6 GPIO6
**MOSI*** | Master Out Slave In       | PICO GPIO23 | MO GPIO35
**MISO*** | Master In Salve Out       | POCI GPIO19 | MI GPIO37
**SCK**   | Serial Clokc              | SCK  GPIO18 | SCK GPIO36
D ALM     | Photo Diode Fault, Output | 06   GPIO32 | 12 GPIO12
LED ALM   | LED Cable Fault, Output   | A0   GPIO26 | 9 GPIO9
DIAG_E    | Diagnostics End, Output   | 11   GPIO27 | 11 GPIO11
**PWDN**  | Power Down, active low    | A1   GPIO25 | 5 GPIO5
**GND**   | Power                     | GND         | GND

Pin 10 is reserved for system button.
Require connections are in **bold**.

*The SPO2 Aanalog Front End uses the old terminology MOSI and MISO while appropriate terminology is POCI and PICO for "peripheral out controller in" and "perpherial in controller out".

- [Thing Plus C Pinout](https://cdn.sparkfun.com/assets/3/9/5/f/e/SparkFun_Thing_Plus_ESP32_WROOM_C_graphical_datasheet2.pdf)
- [ESP32 S3 Pinout](https://learn.adafruit.com/assets/110811)

![Thing Plus C Pinout](..\assets\ThingPlusC_PinOut.png)

![Adafruit Feather ESP32 S3](../assets/adafruit_products_Adafruit_Feather_ESP32-S3_Pinout.png)

[Biometric Cables Pulse Ox Probe](https://www.biometriccables.in/collections/spo2-1/products/oximax-comptiable-spo2-pulse-oximeter-probe-1-0mtr)