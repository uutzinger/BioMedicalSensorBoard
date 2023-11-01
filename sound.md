# Sound Recording

## Digital Stethoscope
- [Texas Instruments Overview](https://www.ti.com/solution/digital-stethoscope)
- [IEEE Paper with Arduino](https://ieeexplore.ieee.org/document/8994674)

## Audio Library for Arduino and ESP
- [arduino-audio-tools](https://github.com/pschatzmann/arduino-audio-tools)
- [audiotools review](https://youtu.be/a936wNgtcRA?si=jpXgP3CTCKrXq8GU)

## Sound ICS
- [SGTL5000](https://www.nxp.com/products/audio-and-radio/audio-converters/ultra-low-power-audio-codec:SGTL5000)
- [ES8388](https://pcbartists.com/wp-content/uploads/2022/07/ES8388-module-datasheet-July-2021.pdf)
- [VS1053](https://cdn-shop.adafruit.com/datasheets/vs1053.pdf)
- [WM8960](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/imx-processors/52419/1/WM8960.pdf)
  
## Sound boards
- [Teensy audio board (SGTL5000)](https://www.pjrc.com/store/teensy3_audio.html)
- [Audiopants ESP32 based on Teensy Audio (SGTL5000)](https://noties.space/headphones-help)
- [Audiopants github](https://github.com/chipperdoodles/audiopants)
- [audioinjector](https://www.audioinjector.net/)
check what chip and hardware they are using, this was made for Raspberry Pi, its likely analog microphone input to I2S which would work also for microcontrollers.
- [I2S ADC Audio I2S Capture Card Module](https://github.com/pschatzmann/arduino-audio-tools/wiki/External-ADC#i2s-adc-audio-i2s-capture-card-module)
- [ES8388 Audio Codec Module](https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Boards#es8388-audio-codec-module)
- [VS1053 Audio Codec Modules](https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Boards#vs1053-audio-codec-modules)
- [PI WM8960 Hat](https://github.com/pschatzmann/arduino-audio-tools/wiki/Audio-Boards#pi-wm8960-hat)
   
## Microphone Amplifier breakout board
[Sparkfun](https://learn.sparkfun.com/tutorials/sound-detector-hookup-guide?_gl=1*1kkdu25*_ga*MzMyODI5MDguMTY5NDE5MzI3NQ..*_ga_T369JS7J9N*MTY5NDQ4ODY0MC4zLjEuMTY5NDQ4ODc2NS40NC4wLjA.&_ga=2.97641820.250989333.1694488641-33282908.1694193275)
[LMV321](http://cdn.sparkfun.com/datasheets/Sensors/Sound/LMV324.pdf?_gl=1*101dz5s*_ga*MzMyODI5MDguMTY5NDE5MzI3NQ..*_ga_T369JS7J9N*MTY5NDQ4ODY0MC4zLjEuMTY5NDQ4ODY2MC40MC4wLjA.)

[Adafruit MAX9814](https://www.adafruit.com/product/1713)

## Regular Microphones (need amplifier and ADC)

| Model | Sensitivity dB | SNR dB |
| ----- | ----------- | --- |
| PUI Audio AOM-5244L-HD-R (just mic) | -46 | 60 | 
| PUI Audio AOM-5054 (just mic)       | -54 |    | 

## Regular MEMS Microphones (need amplifier and ADC)

| Model | Sensitivity dB | SNR dB |
| ----- | ----------- | --- |
| Knowles MQM-32325 | -58 | seems to be hearing aide quality |
| Knowles MM25-33663 | -57 | |
| PUI Audio AMM-3742 | -42 | |
| PUI Audio AMM-2742 | -42 | |

Knowles KAS-33100-003 Evaluation Kit uses Cirrus Logic Stereo Codec. Interfaces with USB to PC and not to microcontroller though.
Can also use SparkFun Analog MEMS Microphone Breakout - ICS-40180 for amplification but still need ADC for digitial conversion. 

## I2S Microphones (MEMS)
MEMS are small and require low power.

| Model                 | Sensitivity dB FS | SNR dbA | Equivalent Noise dBA | Dyanmic Range dB |
| --------------------- | ----------- | ------ | ---------------- | ------------- |
| Knowles SPH0645LM4H-B | -26 | 65 | 33 | |
| TDK/InvenSense ICS-43434 | -26 | 65 | 29 | 91 |
| TDK/InvenSense INMP 441 | -26 | 61 |
| Analog Devices ADMP441 obsolete| -26 | 65 | | |
| STMicroelectronics MP34DT05-A | -26 | 64 | | |
