# Biomedical Sensor Board

## Arduino Code

### Supported Hardware

- ESP32 micro controller.
- AD8232, Sparkfun ECG sensor, https://www.sparkfun.com/products/12650
- MAX1704X, Battery level sensor, integrated into Feather and Thing Plus https://github.com/adafruit/Adafruit_MAX1704X  
- OLED Display SSD1306, https://github.com/adafruit/Adafruit_SSD1306
- ESPNow
- Audio Tools for continuous analog input, https://github.com/pschatzmann/arduino-audio-tools

### Software Implementation:

For each sensor a separate driver is created. Sensor driver are divided into **initialization** and **update** section.

Currently we have:
- ADC (continuous analog conversion)
- Battery (to read battery status)
- Config (for configuration at run time)
- EPSNow (for wireless transmission)
- OLED (for display driver)
- Print (for printing customization)

The updates occur at pre-determined **intervals** in the main loop. The intervals are hardcoded. 

**Settings** of the system are stored in EEPROM and can be modified at runtime without compiling the code. To change the setting the user can obtain a menu by sending ZZ? to the serial console.

The code also measure **time consumption** in each subroutine and you can report that timing. The code keeps a local max time over about 1 second and an all time max time.  This allows to investigate if subroutines take too long to complete. 

For **analog measurements**,  continuous ADC of ESP32 is used. It is advised to sample between 20k and 80k samples per second and to downsample with averaging. For example recording at 44100 samples per second and averaging 8 samples results in 5k samples per second.

**ESPNow** is used to send data to a base station.

In general EPSNow max payload is 250 bytes and the code reserves 240 bytes for data and 10 bytes for header. Since data is int16_t (2 bytes) we can record 120 samples at once. If we record with 5k samples per second we will fill the buffer every 20 ms and we need to make sure the main loop is completed in 20ms. This also will result in the EPSNow message being send 50 times per second, which is a reasonable interval.

The **structure** being sent is defined in ESPNow.h. It consists of a header with sensor ID, number of data points per channel and number of channels. Data is in an array of 240 bytes. The structure contains a checksum calculated with XOR and an end signature. 

To compute the **checksum**, one needs to set the checksum field to 0, then calculate the checksum and update the structure afterwards.

Data received at the ESP **base station** is either printed to serial port in text or binary mode.

In **text mode** each line contains Sensor ID, data_channel1, data_channel2 ... Newline
In **binary mode** each ESPNow packet is printed directly to serial directly byte by byte but its enclosed with start and end sequency of 2 bytes so that the serial receiver can identify the start and end.

Throughout the program **yieldOS** functions were placed. It measures the time since the last function call and executes either delay() or yield() function so that the system can update its background tasks (e.g. WiFi).

### Disclaimer
This software is provided as is, no warranty on its proper operation is implied. Use it at your own risk. See License.md.
 
2024 Spring:    First release

## Python Code 

To decode the binary data from the serial port the following Python code would be needed:

```
import struct

# ESPNow Message Structure
#############################
START_SIGNATURE = b'\xFF\xFD'
STOP_SIGNATURE  = b'\xFD\xFF'
# Message Format is ittle endian: 
#   id 1 byte (ECG, Temp, Sound, Ox, Imp etc)
#   num data points per channel, 1 byte
#   num channels, 1 byte
#   120 short integers, 2 bytes per data point
#   checksum, 1 byte
MESSAGE_FORMAT = '<BBB120hB'
MESSAGE_SIZE = struct.calcsize(MESSAGE_FORMAT)

def calculate_checksum(data):
    checksum = 0
    for byte in data:
        checksum ^= byte
    return checksum

def find_signature(ser, signature):
    while True:
        if ser.read(1) == signature[0:1]:
            if ser.read(1) == signature[1:2]:
                return True

def read_message(ser):
    if not find_signature(ser, START_SIGNATURE):
        return None

    # Read message data
    data = ser.read(MESSAGE_SIZE)
    if len(data) != MESSAGE_SIZE:
        return None  # Incomplete message

    # Read and validate the stop signature
    if ser.read(len(STOP_SIGNATURE)) != STOP_SIGNATURE:
        return None

    # unpack the data
    message = struct.unpack(MESSAGE_FORMAT, data)
    sensor_id, numData, numChannels, *data_array, received_checksum = message

    # Check the checksum
    modified_message = message[:-1] + (0,)
    repacked_message = struct.pack(MESSAGE_FORMAT, *modified_message)
    calculated_checksum = calculate_checksum(repacked_data)

    if calculated_checksum != received_checksum:
        return None
    else 
        return sensor_id, numData, numChannels, data_array, received_checksum
```
