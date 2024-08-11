# Assembly of ECG and BioZ Brick

## Soldering

### 3.5mm Connector
Firmly press the 3.5mm connector into the holes and solder all three pins.

### Wires
Attach color coded wires to the IO pads. E.g. red for power, black or green for ground and blue or white for digital input/output.

You can insert the wires into the holes (perpendicular) or or you can attach a short piece of the wires on top of the pad (perpendicular).

Suggested connecctions for the Sparkfun Thing Plus (USB-C) and the Adafruit Feather ESP32-S3 are below:

PAD       | Function                  | Thing Plus  | Feather
---       |---                        |---          |---
**VCC**   | Power                     | 3V3         | 3V3 
**GND**   | Power                     | GND         | GND
**CS0**   | Chip Select               | 08   GPIO15 | 6
**SDI***  | Master Out Slave In       | PICO GPIO23 | MO
**SDO***  | Master In Salve Out       | POCI GPIO19 | MI
**SCL**   | Serial Clock              | SCK  GPIO18 | SCK
**INTB**  | Interrupt Output          | 12   GIPO12 | 12 GPIO12
INT2B     | Interrupt 2 Output        | 11   GIPO27 | 13 GPIO13

Pin 10 (Feather, Thing Plus)  is reserved for system button.
Require connections are in **bold**.

*The MAX30001G Aanalog Front End uses the MOSI and MISO on diagrams showing a microcontroller. POCI and PICO should be used for "peripheral out controller in" and "perpherial in controller out".

## Board Configuration

There are many configurations we can select. We can operate the board to measure ECG and Impedance or both. We can operate the board to use only two leads, four leads or 6 leads. We can configure the board to measure impedance of a calibration resistor (100 Ohm).

There are many internal configurations, for example we can attach an oscilator to the ECG channel. 

### Jumpers

The following jumpers are available to reduce the numnber of leadsto attach to a subject:

- $ECG_N$ to $BI_N$
- $ECG_P$ to $BI_P$
- $DRV_N$ to $BI_N$
- $DRV_P$ to $BI_P$

The unblance filters for Input and Offset can be bypassed:

- $VCM_{UB}$: $VCM$ unbalance bypass
- $EN_{UB}$: $ECG_N$ unbalance bypass
- $EP_{UB}$: $ECG_P$ unbalance bypass

We can also disable input:

- $ECG_P$ to $ECG_N$ (for BIOZ measurements only operation)
- $BI_P$ to $BI_N$ (for ECG measurements only operation)

We can calibrate BIOZ with a 100 Ohm resistor:

- $R_N$, $R_P$ connect $BI_N$, $BI_P$ to 100R18

When the calibration resistor is in place we can no longer measure BIOZ on subjects.

If $V_CM$ is used for ECG right leg bias, internal lead bias needs to be disabled. Resistor should be 200kOhm, and $V_CM$ bypassed.

For specific hardware configuration setup the following jumpers should be set:

### Configuration Options
Configuration       | $ECG_P$ to $ECG_N$ | $DRV_N$ to $BI_N$ | $DRV_P$ to $BI_P$ | $ECG_N$ to $BI_N$ | $ECG_P$ to $BI_P$ | $R_N$ | $R_P$ | $VCM_{UB}$ | $EN _{UB}$ | $EP_{UB}$ |
------------------|--------------------|-------------------|-------------------|-------------------|-------------------|-------|-------|------------|------------|-----------|
(1) Default       |                    | X                 | X                 |                   |                   | X     | X     |            | X          | X         | 
(2) 100 Ohm Test  |                    | X                 | X                 |                   |                   | X     | X     |            |            |           | 
(3) ECG, R to R   |                    |                   |                   |                   |                   |       |       | *          | ?          | ?         | 
(4) ECG & Resp 2  |                    | X                 | X                 | X                 | X                 |       |       | *          | ?          | ?         | 
(5) ECG & Resp 4  |                    |                   |                   | X                 | X                 |       |       | *          | ?          | ?         | 
(6) GSR           | ?                  | X                 | X                 |                   |                   |       |       | ?          |            |           | 
(7) BIOZ 2        | ?                  | X                 | X                 | X                 | X                 |       |       | ?          |            |           | 
(8) BIOZ 4        | ?                  |                   |                   | X                 | X                 |       |       | ?          |            |           | 

### Lead Connections

(1) **Default**
100 Ohm test for Driver and Impedance, No BIOZ leads.
ECG Measurement Leads on ECG connector.

(2) **No Leads**, 100 Ohm Test configuration. 
The default BioZ frequency is FMSTR/64 = 500Hz, To measure 100 Ohm increase frequency or change internal lower filter cut off.

(3) **ECG** Measurement Leads on ECG connector.

(4) **2 Leads ECG**
Measurement Leads on ECG connector.

(5) **4 Leads ECG**
Driver Leads to Driver connector.
Measurement Leads to ECG or Impedance connector.

(6) **GSR**
Measurement Leads on Impedance connector.

Low current frequency generator at 125, 250 or 500Hz.

BIOZ analog high pass needs to be below current frequency. 

220nF blocking filter between $DRV_P$ and $DRV_N$.

(7) **2 Leads Bio Z**
Leads to Driver connector or Impedance connector.

(8) **4 Leads Bio Z**
Driver Leads to Driver connector.
Measurement Leads to Impednace connector.

(*) Might need to replace R23 with 200kOhm and enable VCM bypass.

#### Pin Outs
- [Thing Plus C Pinout](https://cdn.sparkfun.com/assets/3/9/5/f/e/SparkFun_Thing_Plus_ESP32_WROOM_C_graphical_datasheet2.pdf)
- [ESP32 S3 Pinout](https://learn.adafruit.com/assets/110811)

![Thing Plus C Pinout](..\assets\ThingPlusC_PinOut.png)
![Adafruit Feather ESP32 S3](../assets/adafruit_products_Adafruit_Feather_ESP32-S3_Pinout.png)