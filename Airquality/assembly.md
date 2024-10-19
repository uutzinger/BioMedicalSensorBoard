# Assembly of Airquality Board

<a href="..\assets\pictures\Environment_Top_Open_with_Plugins.jpg" target="_blank">
  <img src="..\assets\pictures\Environment_Top_Open_with_Plugins.jpg" style="width: 600px;">
</a>

## QWIIC Connector

PAD       | Function       | QWIIC pin | Sparkfun       | Adafruit
---       |---             |---        |---             |--- 
**GND**   | Power          | 1         | GND            | GND
**3.3**   | Power          | 2         | set GPIO0 high | I2C Power, enabled by setting PIN_I2C_POWER
**SDA**   | Serial Data    | 3         | GPIO21         | GPIO3 ADC1/CH2
**SCL**   | Serial Clock   | 4         | GPIO22         | GPIO4 ADC1/CH3

## Soldering

Attach color coded wires to the IO pads. E.g. red for power, black or green for ground and blue or white for digital input/output and yellow for analog wires. 

It is not necessary to push the wires into the holes and you can simply strip a short piece of insulation and solder the wires onto the pad.

Suggested connecctions for the Sparkfun Thing Plus (USB-C) and the Adafruit Feather ESP32-S3.

### Button

The button is configured to pull a pin high. One button pin is connected to 3.3V and the other to the input pin on the microcontroller. In software that pin is pulled low.

PAD       | Function        | Thing Plus    | Feather
---       |---              |---            |---
**3.3V**  | Button          | 3V3           | 3V3
**Button**| Button          | 9 or A0       | 12

### 6pin SEN5X Connector

This connector is for the Particula Matter sensor from Sensirion. I use the following premade [Sparkfun Cable](https://cdn.sparkfun.com/assets/8/7/b/c/8/ACCA-3479_Model__1_.pdf)

PAD        | Function                    | 6 pin
---        |---                          |---
**3.3-5V** | Power, 5V from AP3692 boost | 1
**GND**    | Power, GND                  | 2
**SDA**    | Serial Data                 | 3
**SCL**    | Serial Clock                | 4
**SEL**    | Interface Selection         | 5
N.C.       | Do not connect              | 6 

The SCL and SDA pins are connected to the board's QWIIC connector.

### LED Strip

PAD       | Function                  | Thing Plus  | Feather
---       |---                        |---          |---
**3.3-5** | Power                     | 3V3         | 3V3 
**GND**   | Power                     | GND         | GND
**DIN**   | Data In                   | SCK GPIO18  | A0 GPIO18

### SGX MiCS 6814 Sensor

PAD       | Function                  | Thing Plus  | Feather
---       |---                        |---          |---
**VCC**   | Power                     | 3V3         | 3V3 
**GND**   | Power                     | GND         | GND
**EN**    | Enable Regulator / Power  | 06   GPIO32 | D6
**NH3***  | Methane                   | A0 ADC2/CH9 | D11 ADC2/CH0
**CO***   | Carbon Monoxide           | A1 ADC2/CH8 | D12 ADC2/CH1
**NO2**   | Nitric Oxide              | 04 ADC2/CH6 | D13 ADC2/CH2

For tuning of the resistors and finalizing the connections refer to [MiCS6814](MICS6814_on_3V.md).
There are preset resistors as well as jumpers for each sensor. Turn on the power to MiCS and close a jumper. Measure the voltage on the sensor test point. If the voltage is outside the expected range you will need to replace the resistor.

### BMP 581 Interrupt

PAD       | Function                  | Thing Plus  | Feather
---       |---                        |---          |---
INT       | Interrupt                 | 10   GPIO35 | D10


Require connections are in **bold**.

## Pinouts

- [Thing Plus C Pinout](https://cdn.sparkfun.com/assets/3/9/5/f/e/SparkFun_Thing_Plus_ESP32_WROOM_C_graphical_datasheet2.pdf)
- [ESP32 S3 Pinout](https://learn.adafruit.com/assets/110811)

<a href="../assets/ThingPlusC_PinOut.png" target="_blank"> 
  <img src="../assets/ThingPlusC_PinOut.png" style="width: 500px;">
</a>

<a href="../assets/adafruit_products_Adafruit_Feather_ESP32-S3_Pinout.png" target="_blank">
  <img src="../assets/adafruit_products_Adafruit_Feather_ESP32-S3_Pinout.png" style="width: 500px;">
</a>