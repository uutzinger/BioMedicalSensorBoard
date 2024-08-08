# Dependencies
## Libraries

| Chip          | Manfacturere           |  Type | Source | Example Program | Limitations
| ---           | ---                    | --- | --- | --- | --- |
| AFE44X        | Texas Instrumente      | Pulseoxi | Utzinger | Yes | No $SP_{O_2}$ yet |
| MAX30001G     | Maxim / Analog Devices | Bioimpedance and ECG | Utzinger | No | **in progress** |
| ES8336        | Everest                | Stethoscope | [audiodriver Schatzman](https://github.com/uutzinger/arduino-audio-driver) | No | **to be worked on** |
| MPRLS0300YG   | Honeywell              | Pressure |  [Adafruit](https://github.com/uutzinger/Arduino_MPRLS) | No | **to be worked on** |
| ICM20948      | TDK                    | IMU | [Sparkfun](https://github.com/uutzinger/Arduino_ICM-20948) | Yes | need to decide on DMP or custom fusion (AHRS Madgwick) |
| BMP581        | Bosch                  | Pressure | [Sparkfun](https://github.com/uutzinger/Arduino_BMP581) | Yes | sometimes fails to start in SPI mode
| WS2812B       | World Semi             | Color LED strip | [Neopixel](https://github.com/uutzinger/Arduino_NeoPixel) | Yes | figure out individual pixel animation |
| SavitzkyGolay | Signal Proc            | Filter | [Utzinger](https://github.com/uutzinger/SavitzkyGolayFilter)| Yes | need to test with PulseOxi data |
| SHT45-AD1B    | Sensirion              | Humidity, Temperature | [Sensirion](https://github.com/uutzinger/Arduino_SHT) | Yes |  |
| SCD41D        | Sensirion              | CO2 | [Sensirion](https://github.com/uutzinger/Arduino_SCD4x) | Yes |  |  
| SGP41D        | Sensirion              | eVOC, eNOx | [Sensirion](https://github.com/uutzinger/Arduino_SGP41) | Yes | uses Gas Index |
| SEN5X         | Sensirion              | Particulate Matter | [Sensirion](https://github.com/uutzinger/Arduino_SEN5x) | Yes | |
| MICS 6814     | SGX Sensortech         | CO, NH3, NO2 | Utzinger | No | **to be worked on** (needs manual  calibration and soldering)| 
| SSD1306       | Solomon Systems        | LCD display | [Adafruit](https://github.com/uutzinger/Arduino_SSD1306) | Yes |  |
| MAX1740X      | Maxim / Analog Devices | Battery monitor | [Sparkfun](https://github.com/uutzinger/Arduino_MAX1704x) | Yes | need to look into how to detect that system is on battery |
| LC709203F     | ON Semiconductor       | Battery monitor | [Adafruit](https://github.com/uutzinger/Arduino_LC709203F) | No | not available in newer boards from Adafruit and Sparkfun|
| ESP ADC       | Espressif              | Temperature | [Audio Tools, Schatzman](https://github.com/uutzinger/arduino-audio-tools) | Yes | 6 channel recording with averaging and binning |
| Gas Index     | Sensirion              | Gas Index | [Sensirion Gas Index](https://github.com/Sensirion/arduino-gas-index-algorithm) | Yes | |
| Arduino Core  | Sensirion              | Arduino Core | [Sensirion Arduino Core](https://github.com/Sensirion/arduino-core) | Yes | |
| ESP DeeSleep  | Espressif | Deep Sleep | | Yes | |
