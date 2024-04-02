# Air Quality

## Goal
To provide accurate readings for
- Particulate Matter
- CO2 concentration
- Temperature'
- Humidity
- Pressure

and to estimate 
- VOC
- NOX
- CO

concentration.

Values acceptable for human environment are:

| Gas | Normal    | Threshold | Excessive |  |
|---|---|---|---|---|
| CO2 | <1000ppm | 1000-5000      | > 50000 |
| rH  | 45-55 | 25-45 & 55-65  | <15 & > 80 |
| VOC | <220ppm | <660ppm | >2200ppm |
| PM2 | <13 | <55 | >150 | measured in ug/m3
| PM10 | <55 | <155 | >255 | measured in ug/m3
| $T_{body}$ | 36.4 -37.2 | <38.3 | >41.5 | 97.5 - 99F
| $T_{room}$ |  20-25| <20 & 25-30| >30 | 68 - 77F
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
SEN5X family of sensors from Sensirion. They have a >10 years lifetime. Because they use a fan they have 24dB noise at a distance of 0.2m.  PM1 (0.3-1micron), PM2.5 (0.3-2.5micron), PM4(0.3-4micron), PM10 (0.3-10micron). They also measure Humidity +/-4.5% and Temp +- 0.45C

- **SEN50 Particulate Matter** $22.2
- SEN54 Particulate Matter, rel Humidity +/4.5%, Temperature +/-0.45C, VOC Index, $28.1
- SEN55 Particulate Matter, rel Humidity, Temperature, VOC Index, NOx Index, $32.6

## CO2
Sensirion makes accurate compact photo acoustic CO2 sensor. The sensor is negatively impacted by vibrations. Its not clear if the fan of the particulate matter sensor impacts this sensor.

- **SCD40** 400-2000ppm, +/- 50ppm [datasheet](Airquality\datasheets\SCD4x_Ver1.4_Feb2023.pdf) $29.7
- SCD41 400-5000ppm, high accuracy, low power [datasheet](Airquality\datasheets\SCD4x_Ver1.4_Feb2023.pdf) $37.1
- SCD42 400-2000ppm, +/-75ppm [datasheet](Airquality\datasheets\CD_DS_SCD42_Datasheet_D1.pdf) N.A.

They also measure Humidity +/-6% and Temperature +/-0.8C

## Pressure
Bosch makes high quality pressure sensors with an in air resolution of 25 cm. There are also under water sensors for higher pressure. Water pressure gives very accurate depth sensing.

- **BMP581**, 30 -125 kPa, ultra low noise of 0.1PaRMS, Temp -40 to 85C [datasheet](Airquality\datasheets\bst_bmp581_ds004-2950309.pdf) $4.12

## Humidity and Temperature

### Bosch
Bosch makes fully integrated airquality sensor but their CO2 is usually an electric estimate and they don't measure particulates.

- BME688 Pressure 0.6hPa, Humidity +/-3% Temperature +/-0.5C

### High Accuracy Temperature and Humidity Sensors
Many sensors need temperature compensation and some humidity compensation. The relative Humidity and Temperature sensors are not very accurate but there are a few higher quality sensors available such as:

- SHT40, Humidity +/- 1.8%, Temperature +/-0.2, $2.5
- SHT41, Humidity +/- 1.8%, Temperature +/-0.2, $3.4
- SHT43, Humidity +/- 1.8%, Temperature +/-0.2, $5.1
- **SHT45**, Humidity +/- 1%,   Temperature +/-0.1, $6.4

## NOX & VOC
Sensirion MEMS sensor consumes 3mA to measure VOC and NOX. It takes 10 seconds to detect a change in VOC and 4 mins for NOx. Sampling is typically once per second.

- **SGP41 sensor**, VOC Index 1-500, NOX Index 1-500, $8.96 [datasheet](Airquality\datasheets\Sensirion_Gas_Sensors_Datasheet_SGP41.pdf)

## Carbon Monoxide, Amonia, Nitrogenoxide
Its difficult to find accurate and inexpensive Carbon Monoxide sensors. It is possible to calibrate it in house by measuring known levels. For example, there should be no Carbon Monoxide in the atmosphere.

- **Amphenol SGX Sensortech, MiCS-6814 MEMS sensor** $13
[datasheet](Airquality\datasheets\1143_Datasheet-MiCS-6814-rev-8,pdf)

This sensor has 3 elements each being heated and consuming approximately 30mA. The sensor's resistance is measured and relates to RED (CO), OX (NOx), NH3 concentration.
Nominal resistance for NH3 is 10-1500kOhm, for NO2 0.8 to 20kOhm and CO 100 to 1500kOhm. Factory calibration is needed to measure absolute values since the nominal resistance varies significantly.

| Sensor | Nominal $R_{0_{min}} $ | Nominal $R_{0_{max}} $ | $C_{min}$ | $C_{max}$ | $R_S/R_0 min$ | $R_S/R_0 max$| low concentration
|---|---|---|---|---|---|---|---|
| CO| 100k| 1500k| 1ppm| 1000ppm| 4 | 0.01 | high resistance
| NH3| 10k| 1500lk| 1ppm | 300ppm| 0.08 | 30 | low resistance
| NOx| 0.8k| 10k| 0.05ppm | 10ppm| 0.8 | 0.07 | high resistance

Because of these variations its necessary to adjust a voltage divider for each sensor so that the maximum sensitivity is achieved with a microcontroller's internal ADC. It usually is more important to detect low quantities than to measure accurately high levels of analyte. The ESP32 AD converter has an internal attenuator and the minimum accurate measurement is 0.1V and maximum 2.4V.

The example **heater** circuit in the datasheet is for a 5V supply design and puts the CO and NH3 heater in series. For 3.3V operation the heaters need to be connected independently, each with its own serial resistor. Current and nominal resistance of the heater can be obtained from the datasheet and the required serial resistor computed as following:

| Gas | $I_{heat}$ [mA]| $R_{heat}$ [Ohm]| $V$ [V] | $R_{serial_{5V}}$ [Ohm]| $R_{serial_{3.3V}}$ [Ohm] |
|---|---|---|---|---|---|
| CO  | 32 | 74 | 2.4 | 82  | 29  |
| NOx | 26 | 66 | 1.7 | 126 | 61  |
| NH3 | 30 | 72 | 2.2 | 95  | 38  |

## Neopixel
To indicate status we use RGB LEDs controlled with 800kpbs serial signal. These LEDs come in different form factor usually 5x5mm but 1x1 ans 2x2 are available with similar performance. Since each color can draw up to 5mA the current for one element is 15mA if white color is selected. With 12 LEDs and all emitting white, this would result in 200mA current consumption on the supply line. The serial signal is 3.3V compliant and the LED should work with 3.3V supply also, although 5V is recommended.
[datasheet](Airquality\datasheets\2301111010_XINGLIGHT-XL-2020RGBC-WS2812B_C5349955.pdf)

