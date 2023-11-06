# Microcontroller

## EPS 8266 based
- released in 2014
- L106 32-bit RISC microprocessor core based on the Tensilica
- 32kBytes RAM
- IEEE 802.11 b/g/n Wi-Fi
- I2C, I2S, SPI
- Low quality A/D
- $3.50

## ESP32 based 
- released in 2016
- about 100 times faster than ESP8266
- Xtensa dual-core (or single-core) 32-bit LX6 microprocesso
- Wi-Fi: 802.11 b/g/n
- Bluetooth: v4.2 BR/EDR and BLE (shares the radio with Wi-Fi)
- I2C, SPI, I2S
- A2DP (Bluetooth Classic)
- Low quality A/D
- $5.50
  
### ESP32-C3
- released in 2020
- 32-bit RISC-V-based single-core
- 400kBytes
- Bluetooth Low Energy 21dB
- WiFi 2.4GHz 54Mbit/s
- 22 GPIO pins
- Low quality A/D
- Does not support A2DP (Bluetooth Classic)
- I2S performance might be low
- $6.50
  
### ESP32-S3
- released 2020
- Xtensa dual-core 32-bit, 240MHz
- 512kBytes
- Bluetooth Low Energy 20dB (smaller is better)
- WiFi 2.4GHz 54Mbit/s
- 45 GPIO pins
- Low quality A/D
- Does not supprt A2DP (Bluetooh Classic)
- Likely will BLE Audio will not be usable
- Does not yet support I2S
- Arduino-Audio-Tools does not support AudioStream?
- $11.50
  
### ESP32-C6
- released in 2023
- Not yet supported by Arduino IDE
- RISC-V dual core 32-bit 160MHz
- WiFi 6
- Bluetooth LE 5
- Zigbeee
- 8 Mbytes
- Has I2S peripheral
- BLE Audio not supported currently
- $10
  
## Teensy 4.x based
- ARM Cortex-M7 at 600 MHz
- I2C, SPI, I2S, Ethernet
- Precision A/D
- Fastest Microcontroller, about 7 x ESP32

### Teensy 4.0
- Micro USB
- 2Mbytes
- $23.80

### Teensy 4.1
- Micro USB
- FlashCard
- 8Mbytes
- $31.50

## Boards

### SparkFun Thing Plus - ESP32 WROOM
- USB type C  or micro
- Single Core
- 16Mytes SPI Flash
- Lipoly battery connector
- Built-in battery charging when powered over USB-C/micro
- STEMMA Q connector for i2c devices
- $24.95

### Adafruit ESP32-S3 Feather
- USB type C or Lipoly battery
- Built-in battery charging when powered over USB-C
- STEMMA Q connector for i2c devices
- $17.50

### Adafruit QT Py ESP32-S3 WiFi Dev Board with STEMMA QT
- USB type C
- STEMMA Q connector for i2c devices
- Tiny
- will not work with I2S (sound)
- $12.50

### Arduino Nano ESP32 (S3)
- USB-C
- $20
