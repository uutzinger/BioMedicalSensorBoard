# AFE44XX Driver 

This is a comprehensive driver allowing to change most configuration settings of the Pulse Oximeter Analog Front End from Texas Instruments.

It is expected that the 
 - SPI interface is wired correctly.
 - Chip select line is wired
 - Dataready signal is wired
 - Power Down signal is wired

Other signals are optional.

Functions are privided to:
 - Set the Photo Diode Amplifier settings
 - Set the Transmitter LED current and voltage
 - Set the digital IO
 - Set the measurement timing

Currently the Heart Rate and Oxygenation algorihm is not yet implemented.
A layout how those algorithms will work once implemented is shown.

Urs Utzinger, May/June 2024

## Test Program
A program to change the AFE settings was developed. It takes the following commands. For explanation look into the descriptions below.
```
 ================================================================================
 | AFE4490 Texas Instruments Analog Front End for Clinical 2CH Pulse Oximeter   |  
 | 2024 Urs Utzinger                                                            |
 ================================================================================
 | ?: help screen                        | r: dump registers                    |
 | z: print sensor data                  | t: timing factor t1.0                |
 | d: run diagnostics                    | p: alarm pins p0                     |
 | a: apply settings                     | s: show settings                     |
 | .: toggle start/stop DAQ              | e: show timers                       |
 | m: set averages               m4      |                                      |
 ==Receiver==============================|==Transmitter==========================
 | Rg1: stage2 gain LED1,        Rg1-1   | TV:  ref voltage level, TV0          |
 | Rg2: stage2 gain LED2,        Rg23    | TC:  current range,     TC0          |
 | RC1: feedback capacitor LED1, RC15    |======================================|
 | RR1: feeback resistor LED1,   RR1200  | LP1: power level LED 1, LP1255       |
 | RFC: filter edge frequency,   RFC0    | LP2: power level LED 2, LP2255       |
 | RFB: ambient Current,         RFB1    |======================================|
 | RC2: feedback capacitor LED2, RC25    |                                      |
 | RR2: feeback resistor LED2,   RR2200  |                                      |
 ==Toggle================================|==Toggle===============================
 | x: 1 sameGain LED1&2 on/off   x1      | x: 5 RX on/off          x5           |
 | x: 2 tri state on/off         x2      | x: 6 TX on/off          x6           |
 | x: 3 h bridge on/off          x3      | x: 7 AFE on/off         x7           |
 | x: 4 bypass ADC on/off        x4      |                                      |
 ==Debug Level===========================|==Debug Level==========================
 | l: 0 none                     l0      |                                      |
 | l: 1 errors only              l1      |                                      |
 | l: 2 warnings                 l2      |                                      |
 | l: 3 info                     l3      |                                      |
 | l: 4 debug                    l4      | l: 99 continous data reporting       |
 ================================================================================
```

## Blockdiagram
<img src="./assets/AFE4400_Blockdiagram.png" alt="Block" width="400">

## Receiver
<img src="./assets/AFE4400_Detector.png" alt="Receiver" width="400">

### Receiver Gain

The transimpedance amplifier feedback resister $C_F$ and $R_F$ can be programmed independenly for light measurements from LED_1 or LED_2.

### Receiver Capacitor

Capacitor of 0..275pF can be selected, 5pF is default. The capacitance is created through a combination of 5 capacitors: 5, 15, 25, 50, 150 pF. Lowering the value increases noise.

### Receiver Feedback Resistor

Resistor of 0, 10, 25, 50, 100, 250, 500 and 1000 kOhm can be selected. 
500 kOhm is the default. Increasing the value increases the gain.

### Second Stage Gain

| Value | Gain
|-------|-----
| 0 | 1 |
| 1 | 1.5 | 
| 2 | 2 |
| 3 | 3 |
| 4 | 4 |

It is better to increase the gain of the transimpedance amplifier compared to increasing the second stage gain.

### Receiver Filter

A low pass filter is used before the DAC to prevent aliasing.

| Value | Filter |
|-------|--------|
| 0     | 500 Hz |
| 1     | 1000 Hz |

### Receiver Cancellation Current

A background of 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 micro Amp can be subtracted from the detector. When light is blocked to the photo diode, its reading should be zero. However there usually is small offset which can be cancelled.

### Transmitter
<img src="./assets/AFE4400_Driver.png" alt="Transmitterer" width="400">

### Transmitter Voltage and Current Range

AFE4400 has two settings for current range (0,1) for 0 and 50mA.

AFE4490 has three settings for curren range (0/2,1,3)

voltage range     |       | 1   | 0/2 |   3 |         |
------------------|-------|-----|-----|-----|---------|
Voltage           |       | 0.5 | 0.75| 1   | V       |
**current range** | **1** | 50  | 75  | 100 | milliAmp|
**current range** |**0/2**| 100 | 150 | 200 | milliAmp|
**current range** | **3** |  0  |  0  |  0  | milliAmp|

### Transmitter Power
After setting the power range, the LED power can be set for each LED to a value of 0..255 which is proportional to the max current delivered to the LED as shown above.

## Measurement Timing

A timing sequences is set that for each loop a reading at LED1 and LED2 is available as well as the corresponding ambient light levels. Ambient light level is subtracted from the readings at LED1 and LED2 in the AFE.

A stanard timing sequence from the datasheet is used and can be modified with a factor. If factor is 1.0, the default values are used. This results in a measurement sequence executing 500 times per second. For other Factor values, timing values are multplied with Factor but the reset pulse length and sampling offset is not altered.

PRPCOUNT defines the length of a measurement sequence and needs to be between 800 to 64000. One can assume that a longer sequence results in longer sampling and therefore less noise. 

$Measurements Per Second =   4Mhz / PRPCOUNT$

The measurement process is:
- Turn LED on
- Sample Signal 
- Turn LED off
- Reset ADC 
- ADC Conversion

To measure ambient, LED1 and LED2 a fixed sequence is used:
  1. Ambient LED2 (LED1&2 off)
  2. LED1
  3. Ambient LED1 (LED1&2 off)
  4. LED2
  5. Repeat

The default timing for $PRPCOUNT=8000$ is as following:

|             |        On|    Sample|   Convert| ADC Reset|
|-------------|----------|----------|----------|----------|
| LED2        | 6000-7999| 6050-7998|    4-1999|       0-3|
| LED1        | 2000-3999| 2050-3998| 4004-5999| 4000-4003|
| Ambient LED2|          |   50-1998| 2004-3999| 2000-2003|
| Ambient LED1|          | 4050-5998| 6004-7999| 6000-6003| 
    
This results in LED pulses of 0.5ms and 500 measuarements for both channels per second.