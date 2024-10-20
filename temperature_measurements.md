# Temperature

Temperature regulation is essential for mamalian organisms as well as culture of cells and tissues. The physiologic temperature range is narrow with about $37\degree C$. Relevant environmental temperatures are between $-/+50\degree C$.

Temperature can be measured using thermoelectric effect using thermo couples. Most common is the use thermistors which change their resistance based on temperature either in negative or positive direction. The negative thermistor with room temperature resistance specified at $20\degree C$ is a common thermistor.

For medical applications the sensor is encapsulated and electrically isolated so that it can be exposed to body fluids and disinfected.

## Full bridge circuit to measure Thermistors accurately

### Wheatstone Bridge

For accurate readings, a Wheatstone bridge is needed to record the resistance of a thermistor. This is a full bridge with 4 resistors. Such bridge will be insensitive to temperature changes at the location of R1, R2 and R3 as long as they are kept close to each other.

<img src="assets/Wheatstone.svg" alt="drawing" height="300"/>

made with https://www.circuit-diagram.org/editor

The thermistor resitance is

$R_{Thermistor} = \frac{R_3 (V_{in} R_2 - V_{diff} (R_1+R_2))}{Vin R1 + Vdiff (R_1+R_2)}$

Where $V_{diff} = V_1 - V_2$

$V_1$ and $V_2$ can be measured with a microcontroller's ADC converter. ESP ADC is of low quality and linarization and averaging in software is needed.

To implement the formula above to caculate the thermistor resistance with integer mathematics we need to use 64 bit math because resistors are 10,000 (Ohms) and $V_{in}$ is 3,300 (milli Volts) resulting in numbers larger than 32 bits.

```
int32_t R_thermistor = int32_t ( ( uint64_t(R3) * uint64_t(Vin*R2 - Vdiff*(R1+R2)) ) / uint64_t(Vin*R1 + Vdiff*(R1+R2)) );
```

### Steinhart-Hart Equation

<a href="https://www.thinksrs.com/downloads/programs/therm%20calc/ntccalibrator/ntccalculator.html" target="_blank"> <img src="assets/Steinhart.png"  height="300px"></a> 


The equation provides a formula to model the resistance of the thermistor based on 3 calibration values A, B and C. These are manufacturer provided and material constants but they vary for each type of thermistor. The equation is an approximation of the semiconductors resistance model.

$\frac{1}{T} = A + B ln(R) + C (ln(R))^3$

To solve the Steinhart-Hart Equation we need to use floats as we have to compute a logarithm. We will provide integer result in $100 x  Centigrades$. On some micro controllers, log and float math takes resources. Its better to average and reduce noise on the ADC readings and then applying the floating point math to the data.

```
float lnR = log(float(R_thermistor));                // natural logarithm
float Temp = 1.0/(A + B * lnR + C * (lnR*lnR*lnR));
Temp = (Temp-273.15);                               // Kelvin to Centigrade
return int16_t(Temp*100.0);                         // convert temperature e.g. 29.12 to 2912
```

### Wheatstone Bridge measured with precision ADC

When high precision measurements are needed we can use a differential AD converter:

<img src="assets/LTC2471-8586.png" alt="drawing" height="300"/>

Product [LTC2473](https://www.analog.com/en/products/ltc2473.html) has [LTC2473 data sheet](https://www.analog.com/media/en/technical-documentation/data-sheets/24713fb.pdf) and example [Arduino Software](https://github.com/analogdevicesinc/Linduino).

This converter is an inexpensive 16 bit 800 samples per second converter.
