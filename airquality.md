# Air Quality

## Goal
To provide accurate readings for
- Particulate Matter
- CO2 concentration
- Temperature'
- Humidity
- Pressure

and to estimate concentration of
- VOC
- NOX
- CO

Values acceptable for human environment are:

| Gas | Normal    | Threshold | Excessive |  |
|---|---|---|---|---|
| CO2 | <1000ppm | 1000-5000      | > 50000 |
| rH  | 45-55 | 25-45 & 55-65  | <15 & > 80 |
| VOC | <220ppm | <660ppm | >2200ppm |
| PM2 | <13 | <55 | >150 | measured in ug/m3
| PM10 | <55 | <155 | >255 | measured in ug/m3
| $T_{body}$ | 36.4 -37.2 | <38.3 | >41.5 | 97.5 - 99F
| $T_{room}$ |  20-25| <20 or 25-30| >30 | 68 - 77F
| dP/24hrs    | | >5mbar | |

References for:
- [CO2](https://www.dhs.wisconsin.gov/chemical/carbondioxide.htm)
- [VOC](https://atmotube.com/air-quality-essentials/standards-for-indoor-air-quality-iaq)
- [VOC](https://www.advsolned.com/how-tvoc-affects-indoor-air-quality-effects-on-wellbeing-and-health/)
- [PM](https://atmotube.com/air-quality-essentials/particulate-matter-pm-levels-and-aqi)

Low humidity impacts the tear film of the eyes but also reduces virulence of airborne pathogens.
Large pressure changes can cause headaches in susceptible humans.
Cooking, heating and room occupancy affect CO2.
Presence of VOCs and Amonia is undesired. Carbon Monoxide can lead to poisoning.

## Particulate Matter
Small particles affect the airways and can cause a variety of lung diseases. They are most commonly measured through light scattering on a steady stream of air. Scattering on particles has been studied and well described my Mie and is a technique also used in liquid evironment such as flow cytometry. The scattering phase function is affected by wavelength and particle size. Therefore to assess different particle sizes the angular distribution of a scatted collimated beam is measured and for concentration to ratio between scattered and non scatterd light.

SEN5X family of sensors from Sensirion is platform of compact particel sensors. They have a >10 years lifetime. Because they use a fan to create a steady stream of air in the sensor chamnber, they create 24dB noise at a distance of 0.2m.  Common used parameters are PM1 (0.3-1 $\mu$ partciles), PM2.5 (0.3-2.5 $\mu$), PM4(0.3-4 $\mu$), PM10 (0.3-10 $\mu$). They also measure Humidity and Temperature with an accuracty of $+/-4.5\%$ and $+- 0.45\degree C$.

There are the following models available:

- **SEN50 Particulate Matter** approx. $22.2
- SEN54 Particulate Matter, rel Humidity, Temperature, VOC Index, approx. $28.1
- SEN55 Particulate Matter, rel Humidity, Temperature, VOC Index, NOx Index, approx. $32.6

## CO2

Carbon dioxide can be measured electrochemically, aoptically and acoustically. Most inexpensive sensors use an electrochmical sensor. However for more accurate measurements a NIR absorption band of CO2 is targeted and the light transmission or acoustic wave produced created by pulsed light is measured.

Sensirion makes accurate compact photo acoustic CO2 sensor. The sensor is negatively impacted by vibrations. Its not clear if in conjuction with a particualte sensor the fan of the particulate matter sensor impacts this sensor.

There are the following models available:

- **SCD40** 400-2000ppm, +/- 50ppm [datasheet](Airquality\datasheets\SCD4x_Ver1.4_Feb2023.pdf) approx. $29.7
- SCD41 400-5000ppm, high accuracy, low power [datasheet](Airquality\datasheets\SCD4x_Ver1.4_Feb2023.pdf) approx. $37.1
- SCD42 400-2000ppm, +/-75ppm [datasheet](Airquality\datasheets\CD_DS_SCD42_Datasheet_D1.pdf) no lonber available.

They also measure Humidity $+/-6/%$ and Temperature $+/-0.8 \degree C$ as the sensor signal is affected by those quantities.

## Pressure
Pressure is commonly recorded with a deforming memrance sealing a gas chamber. Strain gauges measure the deformation. Barometric sensors have a limitted pressure range where as fluid pressure sensors meassure a large pressure range. Altitude can be assessd to the 10th of centimeters where as submersion depth in water can be assessed with milimiter accuracy using these sensors.

Bosch makes high quality pressure sensors with an in air resolution of 25 cm..

- **BMP581**, 30 -125 kPa, ultra low noise of 0.1PaRMS, Temperature range of -40 to 85C [datasheet](Airquality\datasheets\bst_bmp581_ds004-2950309.pdf) approx. $4.12

## Humidity and Temperature

### Bosch
Bosch makes fully integrated airquality sensor that is commonly used for indor airqulity assessment. But their CO2 is usually an electric estimate and the integrated circuits don't measure particulates.

- BME688 Pressure 0.6hPa, Humidity $+/-3\%$ Temperature $+/-0.5 \degree C$

### High Accuracy Temperature and Humidity Sensors
Many sensors need temperature compensation and some require humidity compensation and therefore they are integrated into other sensor. Those relative Humidity and Temperature sensors are not very accurate but there are a few higher quality sensors available such as:

- SHT40, Humidity $+/- 1.8\%$, Temperature $+/-0.2 \degree C$, approx. $2.5
- SHT41, Humidity $+/- 1.8\%$, Temperature $+/-0.2 \degree C$, approx. $3.4
- SHT43, Humidity $+/- 1.8\%$, Temperature $+/-0.2 \degree C$, approx. $5.1
- **SHT45**, Humidity $+/- 1\%$,   Temperature $+/-0.1 \degree C$, approx. $6.4

## NOX & VOC
Nitrox Oxide in the air is due to high temperature combustion and emission from soil. In the atmosphere it results in acidic rain. It is used to relax smooth muscle cells to widen blood vessels in the lung.

Volatile Organic Compounds are human made chemicals used as solvents and refrigerants. They negatively impact the central nervous system, damage internal organs or are irritants of the airways. Under normal conditions they are not present in the air.

A variety of MEMS sensors can measure electrical equivalents of atmospheric gases through electro chemical reactions where the sensor element is heated and the presence of gases reduces its resistance. Usually there is cross sensitivity within one element with other gases.

A typical Sensirion MEMS gas sensor consumes 3mA to measure VOC and NOX. It takes 10 seconds to detect a change in VOC and about 4 mins for NOx. Sampling is typically once per second.

- **SGP41 sensor**, VOC Index 1-500, NOX Index 1-500, $8.96 [datasheet](Airquality\datasheets\Sensirion_Gas_Sensors_Datasheet_SGP41.pdf)

## Carbon Monoxide, Amonia, Nitrogenoxide

Its difficult to find accurate and inexpensive Carbon Monoxide sensors. It is possible to calibrate less expensive sensors by measuring known levels in a test chamber. For example, there should be no Carbon Monoxide in the atmosphere indicating the sensor background level.

Carbon Monpoxide is a poisenos gas where as Nitroc Oxide and Methane should be avoided indoors.

Amphenol produces an multi element MEMS gas sensor.

- **Amphenol SGX Sensortech, MiCS-6814 MEMS sensor** $13
[datasheet](Airquality\datasheets\1143_Datasheet-MiCS-6814-rev-8,pdf)

This sensor has 3 elements each being heated, consuming approximately 30mA. The sensor's resistance is measured and relates to chemical reduction (CO), oxidation (NOx), and NH3 concentration.

Nominal resistance for NH3 is 10-1500kOhm, for NO2 0.8 to 20kOhm and CO 100 to 1500kOhm. Factory calibration is needed to measure absolute values since the nominal resistance varies significantly for each sensor.

| Sensor | Nominal $R_{0_{min}} $ | Nominal $R_{0_{max}} $ | $C_{min}$ | $C_{max}$ | $R_S/R_0 min$ | $R_S/R_0 max$| low concentration
|---|---|---|---|---|---|---|---|
| CO| 100k| 1500k| 1ppm| 1000ppm| 4 | 0.01 | high resistance
| NH3| 10k| 1500lk| 1ppm | 300ppm| 0.08 | 30 | low resistance
| NOx| 0.8k| 10k| 0.05ppm | 10ppm| 0.8 | 0.07 | high resistance


The Amphenol sensor does not have an integrated analog to digital converted and the resistoance will need to measured using a resistor network such as simple voltage divider. Its necessary to adjust a voltage divider for each sensor so that the maximum sensitivity is achieved with a microcontroller's internal ADC. It usually is more important to detect low quantities than to measure accurately high levels of the analyte. The ESP32 AD converter has an internal attenuator and the minimum accurate measurement is 0.1V and the maximum 2.4V. It requires linearization and averaging to obtain an accurate value. 

The example **heater** circuit in the datasheet of this sensor is for a 5V supply design and puts the CO and NH3 heater in series. For 3.3V operation, the heaters need to be connected independently, each with its own serial resistor. Current and nominal resistance of the heater can be obtained from the datasheet and the required serial resistor is as following:

| Gas | $I_{heat}$ [mA]| $R_{heat}$ [Ohm]| $V$ [V] | $R_{serial_{5V}}$ [Ohm]| $R_{serial_{3.3V}}$ [Ohm] |
|---|---|---|---|---|---|
| CO  | 32 | 74 | 2.4 | 82  | 29  |
| NOx | 26 | 66 | 1.7 | 126 | 61  |
| NH3 | 30 | 72 | 2.2 | 95  | 38  |

## Neopixel

To indicate status we use RGB LEDs controlled with 800kpbs serial signal. These LEDs come in different form factor usually 5x5mm but 1x1 ans 2x2 are available with similar performance. Since each color can draw up to 5mA the current for one element is 15mA if white color is selected. With 12 LEDs and all emitting white, this would result in 200mA current consumption on the supply line. The serial signal is 3.3V compliant and the LED should work with 3.3V supply also, although 5V is recommended.
[datasheet](Airquality\datasheets\2301111010_XINGLIGHT-XL-2020RGBC-WS2812B_C5349955.pdf)

