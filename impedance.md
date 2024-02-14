# Tissue Impedance

## General

### Analog Front End for Bioimpedance

For bioimpedance measurements using the AD5933 the following analog front end elements are needed: a high-pass filter (HPF), a voltage-to-current converter (VCCS), and an instrumentation amplifier. 

The HPF is a first order system composed by a 100 kΩ resistor and a 10 nF capacitor to remove the DC components of Vout. 

The HPF output is connected to VCCS, which consists of an operational amplifier and two resistors (R_in = 1 kΩ; R_feedback = 10 kΩ). 

Because the impedance is supposed to vary from tens to hundreds of ohms, R_feedback = 10 kΩ ensures that current flows mainly through Z_Load, which is connected in parallel to R_feedback. 

The unity gain instrumentation amplifier receives, through its non-inverting input, the voltages on the electrodes connected to Z_Load. 

Since the ADC converter of the AD5933 is unipolar, a potential (VDD/2) is added as reference voltage to the instrumentation amplifier. 

The DSP of the AD5933 calculates the real and imaginary parts of Z_Load, which are read through an inter-integrated circuit (I2C) protocol 

### Impedance Estimation

### References

- [IEEE Tutorial](https://ieeexplore.ieee.org/document/9529213)
- [Juan D. Munoz et al, 3033 AD5933 Impedance Tomograph](https://doi.org/10.1016%2Fj.ohx.2022.e00274)
- [Uwe Pliquett, Andreas Barthel, 2012 J. Phys.: Conf. Ser. 407 012019](https://iopscience.iop.org/article/10.1088/1742-6596/407/1/012019)
- [Instructables Bioimpedance w Arduino](https://www.instructables.com/Determining-Body-Composition-using-Arduino)
- [Instructables Thoracic Impedance 1](https://www.instructables.com/Thoracic-Impedance-1/)
- [Instructables Thoracic Impedance 2](https://www.instructables.com/Thoracic-Impedance/)
- [Instructables Bioimpedance 1](https://www.instructables.com/Bio-Impedance-Analysis-BIA-With-the-AD5933/)
- [Instructables Bioimpedance 2](https://www.instructables.com/Body-Composition-using-BIA/)


Instructable solution was implemented and worked. OPAmps were changed to work with 3.3V.
First OpAmp is a buffer. Second is a current to voltage converter, third OpAmp is differential amplifier and 4th OpAmp is buffer.

## Skin Equivalent

Model1: 

Bogónez-Franco, Paco, Problems encountered during inappropriate use of commercial bioimpedance devices in novel applications, 7 th International Workshop on Impedance Spectroscopy 2014
https://www.researchgate.net/publication/269571754_Problems_encountered_during_inappropriate_use_of_commercial_bioimpedance_devices_in_novel_applications

R-rC with R=2k2, r=150k, C=150nF

Model 2:

[J Ferreira et al 2010 J. Phys.: Conf. Ser. 224 012011](https://iopscience.iop.org/article/10.1088/1742-6596/224/1/012011/pdf)


## Analog Devices ICS
- [AD5933 12-bit impedance converter (used in instructable example)](https://www.analog.com/en/products/ad5933.html)
- [User Guide with reference schematics](https://www.analog.com/media/en/technical-documentation/user-guides/UG-364.pdf)

## Maxim ICS
- [Max 30001G](https://www.maximintegrated.com/en/products/analog/data-converters/analog-front-end-ics/MAX30001G.html)
- [Maxim 30009](https://www.maximintegrated.com/en/products/sensors/MAX30009.html)
