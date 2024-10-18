# Impedance Solutions 

## IEC 60601 
IEC 60601 limits 100 µA maximum dc-leakage current through the body under normal conditions and 500 µA maximum under worst-case, single-fault conditions. An impedance circuit should not exceed that current.

## Analog Devices AFE
- [AD5933 12-bit impedance converter ($18)](https://www.analog.com/en/products/ad5933.html) evaluation kit available ($68)
- [User Guide with reference schematics](https://www.analog.com/media/en/technical-documentation/user-guides/UG-364.pdf)

## Maxim AFE
- Biopotential
  - ECG (waveform)
  - R-R (heart rate)
- Bioimpedance
  - Resp (respiration)
  - GSR (galvanic skin response, stress level)
  - EDA (electro dermal activity)

- [Max 30001G ($10)](https://www.maximintegrated.com/en/products/analog/data-converters/analog-front-end-ics/MAX30001G.html) for Biopotential and Bioimpedance, evaluation kit available ($113)
- [Maxim 30009](https://www.maximintegrated.com/en/products/sensors/MAX30009.html) for Bioimpedance, evaluation kit available ($212)

## AFE Summary

| PART | Features             | Resolution | Remarks |
|---   |---                   |---         |---      |
|AD5933|Single channel 3 wire | 12bit, 1Msps, 100kHz excitation| First Gen, 1024 point FFT, 1kOhm - 1MOhm
|AD5940/41 | 2 channel, 4 wire | 16bit, 800ksps, 200kHz | Impedance spectroscopy
|ADuC355 | 2 channel, 4 wire | 16bit, 800ksps, 200kHz | Impedance Spectroscopy, 26MHz ARM Cortex M3 MCU
| MAX30002 | single channel, BioZ, 2 or 4 wire | 17bit| 
| MAX30009 | single channel, BioZ, 2 or 4 wire | 16 kHz - 806 kHz |
| MAX30131/132/134 | 1-,2-,4- channel | 12 bit |
| MAX30001G | single channel, 2 or 4 wire | BIOZ: 1-100kHz xx bit, ECG: xxbit | Impedance and Potential (ECG)

## References

For general reading: [Analog Devices: Bio-Impedance Circuit Design for Body Worn Systems"](https://www.analog.com/en/resources/analog-dialogue/articles/bioimpedance-circuit-design-challenges.html)

1) [Analog Devices AD5933 Evaluation Board, User Guide](https://www.analog.com/media/en/technical-documentation/user-guides/UG-364.pdf)
2) [Munoz et a, 2022, A low-cost, portable, two-dimensional bioimpedance distribution estimation system based on the AD5933 impedance converterl](https://doi.org/10.1016%2Fj.ohx.2022.e00274)
3) [AD5933 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/AD5933.pdf)
4) [Instructables: Body Composition](https://www.instructables.com/Determining-Body-Composition-using-Arduino)
5) [Instructables: BIA 1](https://www.instructables.com/Body-Composition-using-BIA/)
6) [Instructables: BIA 2](https://www.instructables.com/Bio-Impedance-Analysis-BIA-With-the-AD5933/)
7) [Instructables: Thor 1](https://www.instructables.com/Thoracic-Impedance-1/)
8) [Instructables: THor 2](https://www.instructables.com/Thoracic-Impedance/)
