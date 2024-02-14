# Tissue Impedance

## Analog Devices ICS
- [AD5933 12-bit impedance converter (used in instructable example)](https://www.analog.com/en/products/ad5933.html)
- [User Guide with reference schematics](https://www.analog.com/media/en/technical-documentation/user-guides/UG-364.pdf)

## Maxim ICS
- [Max 30001G](https://www.maximintegrated.com/en/products/analog/data-converters/analog-front-end-ics/MAX30001G.html)
- [Maxim 30009](https://www.maximintegrated.com/en/products/sensors/MAX30009.html)

## General Circuit for AD5933

![Circuit](Impedance_Board\circuit.svg)

From [1] and [2]

## AD5933 Evaluation Board

The evaluation board reference design includes an analog front end to measure low impedances.

On the excitation side it has a passive high pass filter with R_HP, C_HP and an OpAmp buffer.

On the measurement side it has an amplifier with V_DD/2 on the positive input and feedback resistor R_A_FB.

The input to the AD5933 has R_IN of 20K as well as a R_AD5933_FB of 20K.

### Analog Frontend for Bioimpedance

For bioimpedance measurements using the AD5933 the following analog front end elements are needed: a high-pass filter (HPF), a voltage-to-current converter (VCCS), and an instrumentation amplifier.

The HPF is a first order system composed by a 100 kΩ resistor and a 10 nF capacitor to remove the DC components of Vout. 

The HPF output is connected to VCCS, which consists of an operational amplifier and two resistors (R_in = 1 kΩ; R_feedback = 10 kΩ). 

Because the impedance is supposed to vary from tens to hundreds of ohms, R_feedback = 10 kΩ ensures that current flows mainly through Z_Load, which is connected in parallel to R_feedback. 

The unity gain instrumentation amplifier receives, through its non-inverting input, the voltages on the electrodes connected to Z_Load. 

Since the ADC converter of the AD5933 is unipolar, a potential (VDD/2) is added as reference voltage to the instrumentation amplifier. 

The DSP of the AD5933 calculates the real and imaginary parts of Z_Load, which are read through an inter-integrated circuit (I2C) protocol 

### Electrical Components

| Design        | C_HP | R_HP  | RFB  | R_VCC_current | R_VCC_protectFB | R_INA_GAIN    | R_REFGEN | R_AD_IN | R_AD_FB | VCC pos
|---            |---   |---    |---   |---            |---              |---            |---       |---      |---      |---
| Eval [1]      | 47nF | 49.9k | 200k | N.A.          | N.A.            | N.A.          | 49.9k    | 20k     | 20k     |
| Datasheet [3] | 47nF | N.S.  | N.S. | N.A.          | N.A.            | N.A.          | N.S.     | 20k     | 20k     |
| Munoz [2]     | 10nF | 100k  | N.A  | 1k            | 10k             | Gain=1        | 1k       | 1k      | 1k      |
| Instru Bio [4]| 10nF |  10k  |      | 1k            |  1k             | Gain=10, 5.5k | VDD/2    | 1k      | 1k      | GND
| Instru BIA [5]| 10nF | 100k  |      | 1M            |  1M             | Gain=1.5,100k | VDD/2    | 1k      | 1k      | GND
| Instru BIA [6]| 10nF | 100k  |      | 285k          |  1M             |               | VDD/2    |         |         | VDD/2
| Instru Thor[7]| 1.2nF|  10k  |      | 1M            |  1M             | Gain=1.5,100k | 1k       | 1k      | 1k      | GND
| Instru Thor[8]| 100nF|  20k  |      | 100k          |  10k            | Gain=1.5,100k | 3.2/1k   | 1k      | 1k      | GND
| UA 2023       | 10nF | 100k  | N.A. |  82k          |  82k            | 1k            | 20k      | 20k     | 20k     | VDD/2
| UA 2024       |      |       |      |               |                 |               |          |         |         | 

- Network Analyzer AD5933
- Operation Amplifier(s) AD8608 or AD8606 or AD8605
- Instrumentation Amplifier AD826

#### High Pass
- 1 / (2 Pi R C), should attenuate 60Hz, need xxHz..xxkHz
- 10nF, 100k fc=160Hz

#### R_current R_protect
- If input is 198mV, 1k current will be 198 micro Amps (?), 1M 2micro Amp (?), 10 micro Amps
- Range 1 is 1V if operating at 3.3V., for 10 micro Amps need 110k Analog Devices: "Bio-Impedance Circuit Design for Body Worn Systems"

### R_Gain 
- G = 1 + (49.4k/R_Gain)

### Impedance Estimation

### References
[Analog Devices: Bio-Impedance Circuit Design for Body Worn Systems"](https://www.analog.com/en/resources/analog-dialogue/articles/bioimpedance-circuit-design-challenges.html)

1) [Analog Devices AD5933 Evaluation Board, User Guide](https://www.analog.com/media/en/technical-documentation/user-guides/UG-364.pdf)
2) [Munoz et al](https://doi.org/10.1016%2Fj.ohx.2022.e00274)
3) [AD5933 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/AD5933.pdf)
4) [Instructables Body Composition](https://www.instructables.com/Determining-Body-Composition-using-Arduino)
5) [Instructables BIA 1](https://www.instructables.com/Body-Composition-using-BIA/)
6) [Instructables BIA 2](https://www.instructables.com/Bio-Impedance-Analysis-BIA-With-the-AD5933/)
7) [Instructables Thor 1](https://www.instructables.com/Thoracic-Impedance-1/)
8) [Instructables THor 2](https://www.instructables.com/Thoracic-Impedance/)
9) [IEEE Tutorial](https://ieeexplore.ieee.org/document/9529213)
10) [Uwe Pliquett, Andreas Barthel, 2012 J. Phys.: Conf. Ser. 407 012019](https://iopscience.iop.org/article/10.1088/1742-6596/407/1/012019)

## Skin Equivalent

### Model1 (2014) 

Bogónez-Franco, Paco, Problems encountered during inappropriate use of commercial bioimpedance devices in novel applications, 7 th International Workshop on Impedance Spectroscopy 2014
https://www.researchgate.net/publication/269571754_Problems_encountered_during_inappropriate_use_of_commercial_bioimpedance_devices_in_novel_applications

R-rC and for total body composition with R=2k2, r=150k, C=150nF

There is also Respiratory Rate model and Lung Composition.

### Model 2

[J Ferreira et al 2010 J. Phys.: Conf. Ser. 224 012011](https://iopscience.iop.org/article/10.1088/1742-6596/224/1/012011/pdf)
5-100kHz
R//rC with R 917, r 665 C 3.4nF

