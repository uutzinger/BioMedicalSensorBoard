# Air Quality

## Goal
To provide accurate readings for
- Particulate Matter
- CO2 concentration
- Temperature'
- Humidity
- Pressure

and to estimate concentration of
- Volatile Organic Compounds (VOC)
- Nitrogen Oxide (NOx)
- Carbon Monoxide (CO)

Values acceptable for human environment are:

| Gas | Normal    | Threshold | Excessive |  |
|---|---|---|---|---|
| CO2 | <1000ppm | 1000-5000      | > 50000 |
| rH  | 45-55 | 25-45 & 55-65  | <15 & > 80 |
| VOC | <220ppm | <660ppm | >2200ppm |
| PM2 | <13 | <55 | >150 | measured in ug/m3
| PM10 | <55 | <155 | >255 | measured in ug/m3
| $T_{body}$ | 36.4 -37.2 | <38.3 | >41.5 | 97.5 - 99F (normal)
| $T_{room}$ |  20-25| <20 or 25-30| >30 | 68 - 77F (normal)
| dP/24hrs    | | >5mbar | | pressure change

References for:
- [CO2](https://www.dhs.wisconsin.gov/chemical/carbondioxide.htm)
- [VOC 1](https://atmotube.com/air-quality-essentials/standards-for-indoor-air-quality-iaq)
- [VOC 2](https://www.advsolned.com/how-tvoc-affects-indoor-air-quality-effects-on-wellbeing-and-health/)
- [PM](https://atmotube.com/air-quality-essentials/particulate-matter-pm-levels-and-aqi)

Low humidity impacts the tear film of the eyes but also reduces virulence of airborne pathogens.

Large pressure changes can cause headaches in susceptible humans.

Cooking, heating and room occupancy affect CO2.

Presence of VOCs and Amonia is undesired.

Carbon Monoxide can lead to poisoning.

## Particulate Matter
Small particles affect the airways and can cause a variety of lung diseases. They are most commonly measured through light scattering on a steady stream of air. Scattering on particles has been studied and well described my [Gustav Mie](https://doi.org/10.1016/j.jqsrt.2009.02.022) and is a technique also used in liquid environments such as flow cytometry. The scattering phase function is affected by wavelength and particle size. Therefore to assess different particle sizes the angular distribution of a scattered collimated beam is measured. To estimate concentration to ratio between scattered and non scattered light is measured.

## CO2

Carbon dioxide can be measured electrochemically, optically and acoustically. Most inexpensive sensors use an electrochemical sensor. However for more accurate measurements a NIR absorption band of CO2 is targeted and the light transmission or acoustic wave created by pulsed light is measured.

## Pressure

Pressure is commonly recorded with a deforming membrane sealing a gas chamber. Strain gauges measure the deformation. Barometric sensors have a limited pressure range (around 1000mbar) where as fluid pressure sensors measure a larger pressure range (10atm). Altitude can be assessed to 10 centimeters where as submersion depth in water can be assessed with millimeter accuracy using these sensors.

## Humidity and Temperature
To be written

## NOx & VOC
Nitrogen Oxides in the air arise due to high temperature combustion and emissions from soil. In the atmosphere it results in acidic rain. It is used to relax smooth muscle cells to widen blood vessels(vasodilator).

Volatile Organic Compounds are human made chemicals used as solvents and refrigerants. They negatively impact the central nervous system, damage internal organs or are irritants of the airways. Under normal conditions they are not present in the air.

A variety of MEMS sensors can measure electrical equivalents of atmospheric gases through electro chemical reactions where the sensor element is heated and the presence of gases reduces its resistance. Usually there is cross sensitivity within one element with other gases.

A typical Sensirion MEMS gas sensor consumes 3mA to measure VOC and NOx. It takes 10 seconds to detect a change in VOC and about 4 mins for NOx. Sampling is typically once per second.

## Carbon Monoxide, Amonia, Nitrogen Oxides

Its difficult to find accurate and inexpensive Carbon Monoxide sensors. It is possible to calibrate less expensive sensors by measuring known levels in a test chamber. For example, there should be no Carbon Monoxide in the atmosphere indicating the sensor background level.

Carbon Monoxide is a poisonous gas because it more readily binds to hemoglobin than oxygen. 

Nitrogen Oxides and Methane should be avoided indoors.
