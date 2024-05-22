This is a description for the Arduino Code.

The ESP32 measures the voltage difference from the PCB through Pins A0 and A1, then uses those values to calculate the resistance of the thermistor using Wheatstone Bridge equations. The code then calculates the temperature of the thermistor using the thermistor resistance and constants provided by the manufacturer. Because the thermistor is sensitive to subtle changes, the code then averages 100 of the readings together in order to filter out noise from the signal. The averaged reading is then sent to the receiver and to the OLED at 1 to 2 readings per second.

The code is also built to allow the module to be put into deepsleep, where holding the button on the side of the module for 5 seconds will turn the module off, preventing the module from reading, sending, or receiving data, but does allow the module to still be charged. Pressing the button again will wake the module from deepsleep.

The module can be calibrated using the GUI. Entering resistor values measured while building the device will allow for the sensor to be recalibrated. The sensor module will use the new resistor values until the module is turned off. New values can be sent at any time using the GUI.
