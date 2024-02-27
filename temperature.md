# Temperature Sensor

## Thermistors

### Amphenol
- [General](https://www.amphenol-sensors.com/en/product-spotlights/3468-temperature-sensing-for-medical-devices)
- [MA100BF103AN or MA100GG103AN](https://www.mouser.com/datasheet/2/18/Amphenol_04022020_AAS_920_321E-1826352.pdf)

### Smith Medical
- [Probes](https://www.smiths-medical.com/en-us/products/temperature-management/temperature-probes)

### Variohm
- [Probes](https://www.variohm.com/products/temperature-sensors/medical-temperature-probes)

## Breakout boards
- [Protocentral MAX30205](https://protocentral.com/product/protocentral-max30205-body-temperature-sensor-breakout-board/)

## ICS with integrated thermistor 
- [Max30205](https://www.analog.com/media/en/technical-documentation/data-sheets/max30205.pdf)

## Full bridge circuit for custom Thermistors

### Wheatstone Bridge
We will measure with Wheatstone bridge which is a full bridge. Such bridge will be insensitive to temeprature at the location of R1,R2 and R3 as long as they are kept close to each other.

<img src="./assetts/Wheatstone.svg" alt="drawing" height="300"/>

made with https://www.circuit-diagram.org/editor

Where

$ R_{Thermistor} = {R_3 (V_{in} R_2 - V_{diff} (R_1+R_2)) \over Vin R1 + Vdiff (R_1+R_2)} $

To implement this with integer software we need to use uint64_t because resistors are 10,000 (Ohms) and $V_{in}$ is 3,300 (milli Volts) resulting in numbers larger than 32 bits.

```
int32_t R_thermistor = int32_t ( ( uint64_t(R3) * uint64_t(Vin*R2 - Vdiff*(R1+R2)) ) / uint64_t(Vin*R1 + Vdiff*(R1+R2)) );
```
### Steinhart-Hart Equation

The Steinhart-Hart equation provides formula to model the Resistance of the thermistor based on 3 calibration values A,B,C. These are manufacturer provided and material constants.

$ {1\over T} = A + B ln(R) + C (ln R)^3$

To solve the Steinhart-Hart Equation we need to use floats as we have logarithm to compute. We will provide integer result in 100*Centigrade.

```
float lnR = log(float(R_termistor));                // natural logarithm
float Temp = 1.0/(A + B * lnR + C * (lnR*lnR*lnR));
Temp = (Temp-273.15);                               // Kelvin to Centigrade
return int16_t(Temp*100.0);                         // convert temperature e.g. 29.12 to 2912
```

### Custom Wheatstone Bridge measured with Analog Input on Microcontroller
With above formula we need to measure V at junction of thermistor and R3 and at junction of R1 and R2. The difference between them is $V_{diff}$

Analog input on Arduino and ESP microcontrollers are inaccurate and noisy. You will need to enable the calibration features and average many readings in order to obtain accuracy of 0.1C.

### Analog Devices
When high precision measurements are needed we use a differential 16bit AD converter with 800 samples per second such as:

<img src="./assetts/LTC2471-8586.png" alt="drawing" height="200"/>

Product [LTC2473](https://www.analog.com/en/products/ltc2473.html) has [LTC2473 data sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/24713fb.pdf) and example [Arduino Software](https://github.com/analogdevicesinc/Linduino).
