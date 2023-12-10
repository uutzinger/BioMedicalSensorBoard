# Graphical User Interface for Serial Communication
![Serial Monitor](assets/serial_96.png)

## Description
Serial interface to send and receive text from the serial port similar to the Arduino IDE.

It includes a serial plotter to display numbers. Up to 4 numbers can be extracted from a received line of text and displayed as traces.
One can zoom, save and clear this display. Individual lines of text are identified through a selection of end of line characters.

This framework visualize signals and text at high data rates. QT is used to signal and time the application. Serial data handling occurs in its own thread.

Urs Utzinger
2022, 2023

![Serial Monitor](assets/SerialMonitor.png)
![Serial Monitor](assets/SerialPlotter.png)

## Installation
- pip3 install PyQT
- pip3 install numpy
- pip3 install pyserial

The main program is ```main_window.py```. It depends on the files in the asset and helper folder.

## Modules
### User Interface
QT Designer can be used to load and modify```mainWindow.ui``` in the assets folder.

### Main
The main program loads the user interface and adjust its size to specified with and height. This compensates for high resolution screens. All signal connections to slots are created in the main program. It spawns a new thread for the serial data handling. 
Plotting occurs in the main thread as it interacts with the user interface.

### Serial Helper
The serial helper contains three classes. QSerialUI handles the interaction between the user interface and QSerial. It remains in the main thread and emits signals to which QSerial subscribes. QSerial runs on its own thread and sends data to QSerialUI with signals. PSerial interfaces with the pySerial module. It provides a unified interface to the serial port or the textIOWrapper around the serial port.

The serial helper allows to open, close and change serial port by specifying the baud rate and port. It allows reading and sending byte arrays, a line of text or multiple lines of text. It allows selecting text encoding and end of line characters handling.

The serial helper uses 3 continuous timers. One to periodically check for new data on the receiver line. Once new data is arriving the timer interval is reduced. A second timer that emits the amount of characters received and transmitted once a second. These 2 timers are setup after QSerial is moved to its own thread. A third timer trims the received text displayed in the display window once a minute to not exceed a pre defined amount.

### Plotter Helper
The plotter helper extracts numbers from lines of text and appends them to a numpy array. The data array is organized so that new data is appended. The maximum size of that data array is predetermined. A signal trace is a column in the data array and the number of columns is adjusted depending on the numbers present in the line of text but can not exceed 4.

The plotter helper provides a plotting interface using pyqtgraph. Data is plotted where the newest data is on the right (chart) and the amount of data shown is selected through an adjustable slider. Vertical axis is auto scaled based on the data available in the buffer.

A timer is used to update the chart 20 times per second.

Plotting occurs in the main thread as interactions with the User Interface will need to occur there.