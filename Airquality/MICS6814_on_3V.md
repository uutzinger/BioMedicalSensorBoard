# MiCS 6814
The datasheet of the MiCS6814 shows a simplified circuit diagram for 5V operation where the CO and NH3 sensor are connected in series to the power supply for a simpler circuit. We do not use that circuit.

The internal connections of the sensor are illustrated here.
The horizontal connection is the heater and vertical is the sensor. The sensors are read with a voltage divider and the curret for the heater is limitted through a resistors conencted in series.

<img src="./MiCS6814.svg" alt="drawing" height="300"/>

If you look at the [PIMORONI sensor](https://shop.pimoroni.com/products/mics6814-gas-sensor-breakout?variant=39296409305171) schematic, you will see that some of the connections are inverted. For example E and C are flipped as well as F and D.

For 3.3V operation I computed the heater serial resistor as following:

| Element | Voltage [V] | Current [A] | Power [W] | $R_{pre}$ [Ohm]
| ---     | ---         | ---         | ---       | ---
| $CO$    | 2.4         | 0.032       | 0.076     | **28.1**  
| $NO_x$  | 1.7         | 0.026       | 0.042     | **61.5**
| $NH_3$  | 2.2         | 0.030       | 0.066     | **36.6**

- $CO$ Heater: $(3.3[V]-2.4[V])/.032[A] = 28.1 [Ohm]$

- $NO_x$ Heater: $(3.3[V] - 1.7[V])/0.026[A] = 61.5[Ohm]$

- $NH_3$ Heater: $(3.3[V] - 2.2[V])/0.03[A] = 36.6[Ohm]$

## Sensing Voltage Divider

The sensors have a large variation of basline resistance after they are manufactured. The sensor sensitivity is also varying over an oder of manitude. Therefore its difficult to predict an appropriate resistor.

Ideally the sensor is operated in a clean environment and the sensor resistance is measured and then the voltage divider resistors are adjusted so that we have best readings at low concentrations. One optioin is to choose the voltage divider resistor so that is has the same value as the sensor in clean air. 

Using the circuit above and the data sheet:
- The CO reading will decrease if concentration increases.
- The NO_x reading will increase if the concentration increases.
- The NH_3 reading will decrease if concentration increases.

### Sensor Response

From the graphs on the data sheet we can calculate the values the sensor resistor might be at small and large gas concentrations. The graphs show only one possible sensitivity.

|Gas    |Conc [ppm] | $R/R_0$ | $R_s$ min | $R_s$ max 
|---    |---        | ---     | ---       | ---
|$CO$   | 1         | 3.2     | 320k      | 4800k
|$CO$   | 1000      | 0.01    |   1k      |   15k

$R_0$ max = 1500k
$R_0$ min = 100k

The carbon monoxide sensor is also responding to Hydrogen, Ethanol and Ammonia.

|Gas    |Conc [ppm] | $R/R_0$ | $R_s$ min | $R_s$ max
|---    |---        | ---     | ---       | ---
|$NO_2$ | 0.01      | 0.06    |  48       | 1.2k
|$NO_2$ | 6         | 40      |  32k      | 800k

$R_0$ max = 20k
$R_0$ min = 0.8k

The NO_x sensor is weakly responding to NO and Hydrogen.

|Gas    |Conc [ppm] | $R/R_0$ | $R_s$ min | $R_s$ max
|---    |---        | ---     | ---       | ---
|$NH_3$ | 1         | 0.8     | 8k        | 1200k
|$NH_3$ | 100       | 0.065   | 650       | 97.5k

$R_0$ max = 1500k
$R_0$ min = 10k

The NH_3 sensor also responds to Ethanol, Hydrogen, Propane and Iso-butane.

For CO the resistor should be between 320k and 4.8M Ohm. (1.2M selected)

For NO2, the resistor sould be between 48 and 1.2k Ohm. (30k selected)

For NH3, the resistor should be between 8k and 1200k Ohm. (47k selected)

