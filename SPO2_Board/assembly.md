# Assembly of SPO2 Board
## Soldering
### DB 9 Connector
Firmly press the connector into the holes and solder all pins. Add solder to the mechanical retaining hooks.
### Wires
Attach color coded wires to the IO pads.

It is not necessary to push the wires into the holes and you can simply strip a short piece of insulation and solder the wires onto the pad.

PAD | Function | Thing Plus | Feather
---|---|---|---
VCC     | Power                     | 3V3         | 3V3 
Start   | Reset active low          | 04   GPIO14 | 14 
RDY     | ADC Data Ready            | 12   GIPO12 | 13
CS0     | Chip Select               | 6    GPIO32 | 6
MOSI    | Master Out Slave In       | PICO GPIO23 | MO
MISO    | Master In Salve Out       | POCI GPIO19 | MI
SCK     | Serial Clokc              | SCK  GPIO18 | SCK
D ALM   | Photo Diode Fault, Output | 08   GPIO15 | 12
LED ALM | LED Cable Fault, Output   | 10   GPIO33 | 10
DIAG_E  | Diagnostics End, Output   | 11   GPIO27 | 11
PWDN    | Power Down, active low    | A1   GPIO25 | 5
GND     | Power                     | GND         | GND

