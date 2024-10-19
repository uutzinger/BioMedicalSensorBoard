
# Software

Code included with this project is using the Arduino programming environment. 
Several comprehensive libraries as well as test programs for all sensors were developed for this project. 

## Data Visualization

- SerialUI: https://github.com/uutzinger/SerialUI
    - Python: https://www.python.org/downloads/
    - Python Packages: `pyqt5, pyqtgraph, pyopengl, numpy, pyserial, markdown`

SerialUI has similar functions as Arduino IDE serial monitor and serial plotter. However it has arguably better plotting and data visualization capabilities.

## Arduino Libraries
| Chip          | Manfacturer            |  Brick | Source | Example Program | Limitations
| ---           | ---                    | --- | --- | --- | --- |
| AFE44X        | Texas Instrumente      | Pulseoxi | [Utzinger](https://github.com/uutzinger/Arduino_AFE44XX) | Yes | No $SP_{O_2}$ yet |
| MAX30001G     | Maxim / Analog Devices | Bioimpedance and ECG | Utzinger | No | in progress |
| ES8336        | Everest                | Stethoscope | [Audio Driver Schatzman](https://github.com/pschatzmann/arduino-audio-driver) | Yes | tested |
| ES8336        | Everest                | Stethoscope | [Audio Tools, Schatzman](https://github.com/pschatzmann/arduino-audio-tools) | Yes | tested |
| MPRLS0300YG   | Honeywell              | Pressure |  [Sparkfun](https://github.com/sparkfun/SparkFun_MicroPressure_Arduino_Library) | No | to be worked on |
| ICM20948      | TDK                    | IMU | [Sparkfun](https://github.com/uutzinger/Arduino_ICM-20948) | Yes | need to decide on DMP or custom fusion (AHRS Madgwick) |
| BMP581        | Bosch                  | Pressure | [Sparkfun](https://github.com/uutzinger/Arduino_BMP581) | Yes | sometimes fails to start in SPI mode
| WS2812B       | World Semi             | Color LED strip | [Neopixel](https://github.com/uutzinger/Arduino_NeoPixel) | Yes | figure out individual pixel animation |
| SavitzkyGolay | Signal Proc            | Filter | [Utzinger](https://github.com/uutzinger/SavitzkyGolayFilter)| Yes | need to test with PulseOxi data |
| SHT45-AD1B    | Sensirion              | Humidity, Temperature | [Sensirion](https://github.com/uutzinger/Arduino_SHT) | Yes |  |
| SCD41D        | Sensirion              | CO2 | [Sensirion](https://github.com/uutzinger/Arduino_SCD4x) | Yes |  |  
| SGP41D        | Sensirion              | eVOC, eNOx | [Sensirion](https://github.com/uutzinger/Arduino_SGP41) | Yes | uses Gas Index |
| SEN5X         | Sensirion              | Particulate Matter | [Sensirion](https://github.com/uutzinger/Arduino_SEN5x) | Yes | |
| MICS 6814     | SGX Sensortech         | CO, NH3, NO2 | Utzinger | No | to be worked on (needs manual calibration and soldering)| 
| SSD1306       | Solomon Systems        | LCD display | [Adafruit](https://github.com/uutzinger/Arduino_SSD1306) | Yes |  |
| MAX1740X      | Maxim / Analog Devices | Battery monitor | [Sparkfun](https://github.com/uutzinger/Arduino_MAX1704x) | Yes | need to look into how to detect that the system is on battery |
| LC709203F     | ON Semiconductor       | Battery monitor | [Adafruit](https://github.com/uutzinger/Arduino_LC709203F) | No | not available in newer boards from Adafruit and Sparkfun|
| ESP ADC       | Espressif              | Temperature | [Audio Tools, Schatzman](https://github.com/uutzinger/arduino-audio-tools) | Yes | 6 channel recording with averaging and binning |
| Gas Index     | Sensirion              | Gas Index | [Sensirion Gas Index](https://github.com/Sensirion/arduino-gas-index-algorithm) | Yes | |
| Arduino Core  | Sensirion              | Arduino Core | [Sensirion Arduino Core](https://github.com/Sensirion/arduino-core) | Yes | |
| ESP DeeSleep  | Espressif | Deep Sleep | | Yes | |


## Integrated Development Environment

- [Arduino IDE](https://www.arduino.cc/en/software)
- [Arduino EPS32 Extension from Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html)

Arduino ESP32 is comprehensive and supports the ESP32 internal peripherals as listed [here](https://docs.espressif.com/projects/arduino-esp32/en/latest/libraries.html)

### Arduino IDE Programmer Settings

When programming an ESP32 care must be taken on the proper memory and communication settings. During development the microcontroller will also need to be manually rebooted.

Sparkfun feather boards are more easily manageable as they have less options to configure in the IDE. Adafruit ESP32 boards come in a variety of memory configurations and since Adafruit promotes microPython their boot loader settings vary and are by default not set for the Arduino IDE.

Reboot of the ESP32 is accomplished in software by toggling serial control lines. However this is not reliable and sometimes powercycling is necessary.

If the microcontroller board can not be programmed a manual boot loader reset might be necessary (see below).

Programming support on Windows Platform appears to be more refined than on Linux platform.

Once the microcontroller is mounted inside the MediBrick and the battery is connected, manual reset and reboot are not longer accessible.

**Adafruit ESP32-S3 Feather with 2MB of PSRAM**

Reset to boot loader: 

- Push boot button and hold it. Push and release reset button. Release boot button.
- If this does not work, switch to a different USB port on your computer.

The default programming settings are not working well with Arduino IDE and each time the board is selected the following will need to bet chosen:

- USB CDC on Boot: Enabled
- CPU frequency 240MHz
- Code Debug Level: None
- USB DFU on boot: disabled
- Erase all Flash: disabled
- Events run on Core 1
- PSRAM: OPI-PSRAM
- Flash Mode: QIO 80 MHz
- Flash Size: 4Mb
- Partition Scheme: Default 4Mb with spiffs
- Upload mode: UART0
- USB mode: Hardware CDC

**Sparkfun ESP32 C WROOM**

Reset boot to loader: 

- With power cycle: Push and hold the boot button. Connect the board to a computer through the USB-C connection. Release the boot button.

- Without power cycle: Push and hold boot button. Press and release the RST button. Release the boot button. (not working on Linux)
- After programming is completed, reboot the MCU.
    - Press the RST button.
    - If that does not work Power cycle the board.

If you want to use QWIIC: GPIO 0 controls the power output from the XC6222 LDO regulator to the Qwiic connector. Users must toggle GPIO 0 high to enable power for the Qwiic 
