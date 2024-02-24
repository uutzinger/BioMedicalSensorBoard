# Sound Recording

## Digital Stethoscope
- [Texas Instruments Overview](https://www.ti.com/solution/digital-stethoscope)
- [IEEE Paper with Arduino](https://ieeexplore.ieee.org/document/8994674)

## Audio Library for Arduino and ESP
- [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools) with [audiotools review](https://youtu.be/a936wNgtcRA?si=jpXgP3CTCKrXq8GU)
- [arduino-audio-driver](https://github.com/pschatzmann/arduino-audio-driver)

## Sound ICS
Suggested ICS to work with micro controllers.

- [SGTL5000](https://www.nxp.com/products/audio-and-radio/audio-converters/ultra-low-power-audio-codec:SGTL5000)
- [ES8388](https://pcbartists.com/wp-content/uploads/2022/07/ES8388-module-datasheet-July-2021.pdf)
- [VS1053](https://cdn-shop.adafruit.com/datasheets/vs1053.pdf)
- [WM8960](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/imx-processors/52419/1/WM8960.pdf)
  
## Sound boards
Sound boards developed for micro controllers, using i2s:

- [Teensy audio board (SGTL5000)](https://www.pjrc.com/store/teensy3_audio.html) $14.40
- [Audiopants ESP32 based on Teensy Audio (SGTL5000)](https://noties.space/headphones-help)
- [Audiopants github](https://github.com/chipperdoodles/audiopants)
- [audioinjector](https://www.audioinjector.net/) Produces several audio boards for raspberry pi. They likely also work for micro controllers.
- [I2S ADC Audio I2S Capture Card Module](https://github.com/pschatzmann/arduino-audio-tools/wiki/External-ADC#i2s-adc-audio-i2s-capture-card-module) $12
- [ES8388 Audio Codec Module](https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Boards#es8388-audio-codec-module) $17
- [VS1053 Audio Codec Modules](https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Boards#vs1053-audio-codec-modules) $5
- [PI WM8960 Hat](https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Boards#pi-wm8960-hat) $16
   
## Microphone Amplifier breakout board
- [Sparkfun](https://learn.sparkfun.com/tutorials/sound-detector-hookup-guide) with [LMV321](http://cdn.sparkfun.com/datasheets/Sensors/Sound/LMV324.pdf) $12
- [Adafruit MAX9814](https://www.adafruit.com/product/1713) $8

## Regular Microphones 

They need an amplifier and a ADC.

| Model | Sensitivity dB | SNR dB |
| ----- | ----------- | --- |
| [PUI Audio AOM-5244L-HD-R (just mic)](https://puiaudio.com/product/microphones/AOM-5244L-R) | -44 | 60 | 
| [PUI Audio AOM-5054 (jsut mic)](https://puiaudio.com/products/anm-5054l-2-r)      | -54 | 55 | 

## Regular MEMS Microphones 

They need an amplifier and an ADC.

| Model | Sensitivity dB | SNR dB SBL | SNR dB A
| ----- | ----------- | --- | --- |
| [Knowles MQM-32325](https://www.knowles.com/docs/default-source/model-downloads/mqm-32325-000.pdf) | -58 | 26.5 |
| [Knowles MM25-33663](https://www.knowles.com/docs/default-source/default-document-library/mm25-33663-000.pdf) | -57 | 25 | 69
| [PUI Audio AMM-3742](https://puiaudio.com/product/microphones/amm-3742-t-r) | -42 | | 58 |
| [PUI Audio AMM-2742](https://puiaudio.com/product/microphones/AMM-2742-T-R) | -42 | | 59 |

Knowles KAS-33100-003 Evaluation Kit uses Cirrus Logic Stereo Codec. Interfaces with USB to PC and not to microcontroller though.
Can also use SparkFun Analog MEMS Microphone Breakout - ICS-40180 for amplification but still need ADC for digital conversion.

## I2S MEMS Microphones
MEMS are small and require low power.

| Model                 | Sensitivity dB FS | SNR dbA | Equivalent Noise dBA | Dyanmic Range dB |
| --------------------- | ----------- | ------ | ---------------- | ------------- |
| Knowles SPH0645LM4H-B | -26 | 65 | 33 | |
| TDK/InvenSense ICS-43434 | -26 | 65 | 29 | 91 |
| TDK/InvenSense INMP 441 | -26 | 61 |
| Analog Devices ADMP441 obsolete| -26 | 65 | | |
| STMicroelectronics MP34DT05-A | -26 | 64 | | |
