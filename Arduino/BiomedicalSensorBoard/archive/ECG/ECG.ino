//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ECG Sensor                                                                                                                       //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Supported Hardware:
//  - ESP32 micro controller.
//  - AD8232                                      Sparkfun ECG sensor, https://www.sparkfun.com/products/12650
//  - Adafruit_MAX1704X                           Battery level sensor, integrated into Feather and Thing Plus
//                                                https://github.com/adafruit/Adafruit_MAX1704X  
// Data display through:
//  - Adafruit_SSD1306                            OLED Display, https://github.com/adafruit/Adafruit_SSD1306
// Network related libraries:
//  - ESPNow                                      ESPNow
// Other Libraries:
//  - Audio Tools                                 Continuous Analog Input, https://github.com/pschatzmann/arduino-audio-tools
//  - ArduJSON                                    MessagePack data serialization, https://github.com/bblanchon/ArduinoJson.git
//
// Software implementation:
//  Sensor driver are divided into intialization and update section.
//  The updates are a pre determined intervals in the main loop.
//  The intervals at which the updates are called are hardcoded.
//  All settings of the system are stored in EEPROM and can be modified at runtime without compiling the code.
//  To change the setting the user can obtain a menu by sending ZZ? to the serial console.
//  The code also measure time consumption in each subroutine and you can report that timing.
//  The code keeps a local max time over about 1 second and an all time max time. 
//  This allows to investigate if subroutines take too long to complete. 
//
//  For ADC continous ADC on ESP32 is used. It is advised to sample between 20k and 80k samples per second
//  and to downsample with averaging. For example recording at 44100 samples per second and averaging
//  8 samples results in 5k samples per second.
//
// Commumnication:
//   ESPNow is used to send data to a base station.
//
//   In general EPSNow max payload is 250 bytes and the code reserves 240 bytes for data and 10 bytes for header. 
//   Since data is int16_t (2 bytes) we can record 120 samples at once. If we record with 5k samples per second 
//   we will fill the buffer every 20 ms and we need to make sure the main loop is completed in 20ms.
//   This also will result in the EPSNow message being send 50 times per second, which is a resonable interval.
//
//   The structure being sent is defined in ESPNow.h
//   It consists of a head for sensor ID, number of data points per channel and number of channels
//   Data is in an array of 240 bytes.
//   The structure contains a checksum caclulated with XOR and its end. 
//   To compute the checksum, one needs to set the cheksum field to 0, then calculate the checksum and update the structure.
//
//   Data received at the base station is either printed to serial port in text or binary mode.
//   
//   In text mode each line contains Sensor ID, data_channel1, data_channel2 ... Newline
//   In binary mode each ESPNow packet is printed directly to serial directly byte by byte 
//  but its endlosed with start and end sequency of 2 bytes so that the serial receiver can identify start and end.
//
// yieldOS:
//  Throughout the program yieldOS functions were placed. It measures the time since the last function call and executes 
//  delay or yield function so that the system can update its background tasks (e.g. WiFi).
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This software is provided as is, no warranty on its proper operation is implied. Use it at your own risk. See License.md.
// 
// 2024 Spring:    First release
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// To decode the binary data from the serial port the following Python code would be needed:
//
// import struct
//
// # ESPNow Message Structure
// #############################
// START_SIGNATURE = b'\xFF\xFD'
// STOP_SIGNATURE  = b'\xFD\xFF'
// # Message Format is ittle endian: 
// #   id 1 byte (ECG, Temp, Sound, Ox, Imp etc)
// #   num data points per channel, 1 byte
// #   num channels, 1 byte
// #   120 short integers, 2 bytes per data point
// #   checksum, 1 byte
// MESSAGE_FORMAT = '<BBB120hB'
// MESSAGE_SIZE = struct.calcsize(MESSAGE_FORMAT)
//
// def calculate_checksum(data):
//     checksum = 0
//     for byte in data:
//         checksum ^= byte
//     return checksum
//
// def find_signature(ser, signature):
//     while True:
//         if ser.read(1) == signature[0:1]:
//             if ser.read(1) == signature[1:2]:
//                 return True
//
// def read_message(ser):
//
//   if not find_signature(ser, START_SIGNATURE):
//     return None
//
//   # Read message data
//   data = ser.read(MESSAGE_SIZE)
//   if len(data) != MESSAGE_SIZE:
//     return None  # Incomplete message
//
//   # Read and validate the stop signature
//   if ser.read(len(STOP_SIGNATURE)) != STOP_SIGNATURE:
//     return None
//
//   # unpack the data
//   message = struct.unpack(MESSAGE_FORMAT, data)
//   sensor_id, numData, numChannels, *data_array, received_checksum = message
//
//   # Check the checksum
//   modified_message = message[:-1] + (0,)
//   repacked_message = struct.pack(MESSAGE_FORMAT, *modified_message)
//   calculated_checksum = calculate_checksum(repacked_data)
//
//   if calculated_checksum != received_checksum:
//     return None
//   else 
//     return sensor_id, numData, numChannels, data_array, recevied_checksum
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/************************************************************************************************************************************/
// Build Configuration
/************************************************************************************************************************************/
#define VER "0.0.1"
#define SENSORID 1                 // ESPNow message sensor ID

// Debug
// -----
#undef DEBUG     // disable "crash" DEBUG output for production
//#define DEBUG  // enabling "crash" debug will pin point where in the code the program crashed as it displays 
// a DBG statement when subroutines are called. Alternative is to install exception decoder or use both.

// Settings
// #define INITSETTINGS                  // create new eeprom settings, comment for production
#undef INITSETTINGS                // uncomment for production

// Yield
// -----
// At many locations in the program, yieldOS was inserted as functions can take more than 100ms to complete and the
// ESP operating system will need to take care of wireless and other system routines. Calling delay() or yield() gives
// OS opprotunity to catch up.
// yieldOS keeps track when it was called the last time and calls the yield function if the interval is excceded. 
#define MAXUNTILYIELD 50                // time in ms until yield should be called
// Options for yieldOS
//#define yieldFunction() delay(1)      // delay(1), wait at least 1 ms
#define yieldFunction() yield()         // just yield, runs operating system functions if needed
//#define yieldFunction()               // no yield, only if you are sure that system runs stable without yield

// Serial
// ------
#define BAUDRATE  500000                  // can go up to 2,000,000 but 500k is stable on ESP32

// ADC Configuration (for default values)
// -----------------
#define SAMPLERATE                 44100  // See Audio Tools
#define BITWIDTH                      12  // We want 12 bit data
#define CHANNEL ADC_CHANNEL_3             // Sparkfun Thing Plus CH3 is on pin A3, Adafruit S3 CH4 is on pin D5
#define ATTEN ADC_ATTEN_DB_11             // Set maximum input voltage 

// ECG Hardware Pin Configuration
// --------------------------
// You need to wire the ECG board to these pins.
#define SDN 16                        // ENABLE ECG is on pin pin
#define LO_MINUS 17                   // Electrode Disconnected is detected on pin 17
#define LO_PLUS 4                     // Electrode Disconnected is detected on pin 4

// I2C Condfiguration
// --------------------------
// We will use the follwing pins for I2C.
#define SDAPIN              23       // I2C Data
#define SCLPIN              22       // I2C Clock
#define I2CSPEED        400000       // Clock Rate
#define I2CCLOCKSTRETCH 200000       // Clock Stretch
#define OLEDADDR          0x3C       // SSD1306
#define BATTERYADDR       0x36       // MAX17048


/************************************************************************************************************************************/
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!You should not need to change variables and constants below this line!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Runtime changes to this software are enabled through the terminal.
// Permanent settings for the sensor and display devices can be changed in the 'src' folder in the *.h files.
// You would need an external editor to change the .h files.
/************************************************************************************************************************************/

/************************************************************************************************************************************/
// Includes
/************************************************************************************************************************************/

#include "ECG.h"
#include "Config.h"
#include "ESPNow.h"
#include "ADC.h"
#include "Battery.h"
#include "Print.h"
#include "OLED.h"

/************************************************************************************************************************************/
// Variables
/************************************************************************************************************************************/

char tmpStr[256];                                          // universal buffer for formatting texT

// Time Keeping
unsigned long currentTime;                                 // Populated at beginning of main loop
unsigned long tmpTime;                                     // temporary variable to measures execution times
unsigned long lastTime;                                    // last time we updated runtime
unsigned long lastSYS;                                     // determines desired frequency to update system information on serial port
unsigned long lastADC;
unsigned long lastBattery;
unsigned long lastSaveSettings;                            // last time we updated EEPROM, should occur every couple days
unsigned long lastESPNow;
unsigned long lastOLED;              

// Loop execution
unsigned long myLoop                 =        0;           // main loop execution time, automatically computed
unsigned long myLoopMin              =        0;           // main loop recent shortest execution time, automatically computed
unsigned long myLoopMax              =        0;           // main loop recent maximumal execution time, automatically computed
unsigned long myLoopMaxAllTime       =        0;           // main loop all time maximum execution time, automatically computed
float         myLoopAvg              =        0;           // average delay over the last couple seconds
float         myLoopMaxAvg           =        0;           // average max loop delay over the last couple seconds

// Yielding
unsigned long lastYield              =        0;           // keep record of time of last Yield event
unsigned long yieldTime              =        0;
unsigned long yieldTimeMin           =     1000;
unsigned long yieldTimeMax           =        0;
unsigned long yieldTimeMaxAllTime    =        0;

// Execution Times
unsigned long startUpdate            =        0;
unsigned long deltaUpdate            =        0;
// Current max update Times
unsigned long maxUpdateESPNow        =        0;
unsigned long maxUpdateOLED          =        0;
unsigned long maxUpdateBattery;
unsigned long maxUpdateINPUT         =        0; 
unsigned long maxUpdateOUTPUT        =        0; 
unsigned long maxUpdateSYS           =        0;
unsigned long maxUpdateRT            =        0;
unsigned long maxUpdateADC           =        0;
unsigned long maxUpdateEEPROM        =        0;
// Alltime max update Times
unsigned long AllmaxUpdateESPNow     =        0;
unsigned long AllmaxUpdateOLED       =        0;
unsigned long AllmaxUpdateINPUT      =        0; 
unsigned long AllmaxUpdateOUTPUT     =        0; 
unsigned long AllmaxUpdateSYS        =        0;
unsigned long AllmaxUpdateRT         =        0;
unsigned long AllmaxUpdateADC        =        0;
unsigned long AllmaxUpdateBattery    =        0;
unsigned long AllmaxUpdateEEPROM     =        0;

// Initialize Timing Intervals
// all times in milliSeconds
unsigned long intervalADC            =       10;           // up to 100 times per second, assumption is that updateADC will not complete until buffer is full.
unsigned long intervalESPNow         =       10;           // up to 100 times per second 
// intervalESPNow <= intervalADC otherwise we dont get the data out on time 
unsigned long intervalOLED           =     1000;           // up to once per second
unsigned long intervalBattery        =    30000;           // up twice per minute
unsigned long intervalSYS            =     1000;           // update sytem executing maximums every 1 sec
unsigned long intervalRuntime        =    60000;           // update every minute, how long the system has been running 
unsigned long intervalSettings       = 43200000;           // store runtime and settings every 12 hrs

// Serial
char serialInputBuff[64];                                  // we expect new line after 63 characters
int serialInputBuffIndex = 0;
bool serialReceived;                                       // keep track of serial input

// For Serial Transmission of Data:
const byte startSignature[2] = {0xFF, 0xFD};               // Start signature
const byte stopSignature[2]  = {0xFD, 0xFF};               // Stop signature

// Serial text lines
const char singleSeparator[]         = "--------------------------------------------------------------------------------";
const char doubleSeparator[]         = "================================================================================";
const char mON[]                     = "on";
const char mOFF[]                    = "off";
const char clearHome[]               = "\e[2J\e[H\r\n";
const char waitmsg[]                 = "Waiting 5 seconds, skip by hitting enter";

// Config 
extern const unsigned int eepromAddress;                   // 
extern unsigned long      lastSaveSettings;                // last time we updated EEPROM, should occur every couple days
extern Settings           mySettings;                      // the settings, check Config.h

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP GLOBAL
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// I2C
TwoWire              myWire = TwoWire(0);

// ECG Sensor Configuration
extern bool          adc_avail;
extern unsigned long lastADC;
extern int           stateLOMinus;
extern int           stateLOPlus;

// Battery Sensor Configuration
// defined in Battery.ino
extern bool          battery_avail;
extern float         batteryPercent;
extern float         batteryChargerate;     
Adafruit_MAX17048    maxlipo;

// OLED Display
// defined in OLED.ino
extern bool          oled_avail;
Adafruit_SSD1306     display(OLED_RESET);

// ESPNow
// defined in ESPNow.ino
extern bool          espnow_avail;
extern bool          ESPNowData_avail; 
extern char          hostName[32];
extern ESPNowMessage myESPNowDataIn;
extern esp_now_peer_info_t  peerInfo;

// On board analog to digital converter
// ADC.ino
AnalogAudioStream    analog_in; 

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP SYSTEM
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

  /************************************************************************************************************************************/
  // Read configuration from EEPROM
  /************************************************************************************************************************************/
  // EEPROM is a section on the flash memory of the ESP. When you compile a program you can reserve how much is used for program space
  // and whether you have a file system like on a regular flash drive where you can put web pages and documents and you can have a short
  // section used to store program specific settings. These settings are loaded here each time the program starts. We can change
  // the settings with commands form the serial port without needing to recompile the program. Send ZZ? for help.
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(eepromAddress, mySettings);
  // Inject hard coded values into the settings, when we run the program the first time.
  #ifdef INITSETTINGS 
    defaultSettings(); yieldTime += yieldOS(); 
  #endif

  /************************************************************************************************************************************/
  // Serial Interface
  /************************************************************************************************************************************/
  Serial.begin(mySettings.baudrate);
  serialTrigger(waitmsg, 5); // waits 5 seconds until serial is available and initialization continues, otherwise you can not see I2c scan

  /************************************************************************************************************************************/
  // Initialize I2C
  /************************************************************************************************************************************/
  // Searches for devices connected to I2C, this is my general I2C bus scanning routine to find any I2C devices
  // You can add other know addresses.
  snprintf(tmpStr, sizeof(tmpStr), "Scanning (SDA:SCL) - %u:%u", SDAPIN, SCLPIN); 
  if (mySettings.debuglevel > 1) {  R_printSerialln(tmpStr); }
  myWire.begin(SDAPIN, SCLPIN); // init I2C
  myWire.setClock(I2CSPEED); // init I2C
  // myWire.setClockStretchLimit(I2CCLOCKSTRETCH); // ESP32 does not have clockstretch settings apparently
  for (int address = 1; address < 127; address++ )  { // scan all 127 addresses
    myWire.beginTransmission(address); // check if something is conencted at this address
    int error = myWire.endTransmission();  // close connection
    if (error == 0) {
      // found a device
      snprintf(tmpStr, sizeof(tmpStr), "Found device - %d", address); 
      if (mySettings.debuglevel > 1) {  R_printSerial(tmpStr); }
      if        (address == OLEDADDR) {
        oled_avail  = true;  // OLED display Adafruit
        if (mySettings.debuglevel > 1) { R_printSerialln(" OLED"); }
      } else if (address == BATTERYADDR) {
        battery_avail  = true;  // Battery gauge
        if (mySettings.debuglevel > 1) { R_printSerialln(" Battery Gauge"); }
      } else {
        if (mySettings.debuglevel > 0) { R_printSerialln(" Unknown Device"); }
      }
    }
  }

  /************************************************************************************************************************************/
  // Initialize ADC
  /************************************************************************************************************************************/
  if (mySettings.useECG)     { adc_avail     = initializeADC();       yieldTime += yieldOS(); } // see ADC.ino and ADC.h

  /************************************************************************************************************************************/
  // Initialize Peripherals
  /************************************************************************************************************************************/
  if (mySettings.useBattery) { battery_avail = initializeBattery();   yieldTime += yieldOS(); } // see Battery.ino and Battery.h
  if (mySettings.useOLED)    { oled_avail    = initializeOLED();      yieldTime += yieldOS(); } // see OLED.ino and OLED.h 
  if (mySettings.useESPNow)  { espnow_avail  = initializeESPNow();    yieldTime += yieldOS(); } // see ESPNow.ino and ESPNow.h

} // END setup

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {

  bool ok;

  // Timing
  yieldTime = yieldOS();                                // yield to operating system
  currentTime = lastYield = millis();                   // initialize loop time, this is needed to make sure we update devices at know intervals.
  
  /**********************************************************************************************************************************/
  // Update Sensor Readings
  /**********************************************************************************************************************************/
  if (adc_avail && mySettings.useECG) { 
    if ( (currentTime - lastADC) >= intervalADC ) {
      startUpdate = millis(); // we want to know how long it took to obtain the sensor readings
      D_printSerial("D:U:ADC.."); // if we have program issues this shows where the program crashed but DBG needs to be defined
      lastADC = currentTime; // we need to keep track when we ran the ADC update
      ok = updateADC(); //<<<<<<<<<<<<<<<<<<<<--------------- ADC Update
      deltaUpdate = millis() - startUpdate; // this is how long it took to update the ADC readings
      if (maxUpdateADC    < deltaUpdate) { maxUpdateADC = deltaUpdate; } // This keeps track of longest update over last couple seconds
      if (AllmaxUpdateADC < deltaUpdate) { AllmaxUpdateADC = deltaUpdate; } // This keeps track of maximum time it ever took to update
      yieldTime += yieldOS(); // yield to Operating System
    }
  } 
  
  /**********************************************************************************************************************************/
  // Update ESPNow
  // This needs to be run as often as updateADC, otherwise we loose data
  // This will send the data we received from updateADC over ESPNow to the base station
  /**********************************************************************************************************************************/
  if (espnow_avail && mySettings.useESPNow) { 
    if ( (currentTime - lastESPNow) >= intervalESPNow ) {
      startUpdate = millis();
      D_printSerial("D:U:ESPN..");
      lastESPNow = currentTime;
      ok = updateESPNow(); //<<<<<<<<<<<<<<<<<<<<------------ ESPNow Update, deal with sending data
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateESPNow    < deltaUpdate) { maxUpdateESPNow = deltaUpdate; }
      if (AllmaxUpdateESPNow < deltaUpdate) { AllmaxUpdateESPNow = deltaUpdate; }
      yieldTime += yieldOS(); 
    }
  } 

  /**********************************************************************************************************************************/
  // Battery
  /**********************************************************************************************************************************/
  if (battery_avail && mySettings.useBattery) {
    if ( (currentTime - lastBattery) >= intervalBattery ) {
      D_printSerial("D:U:BAT..");
      startUpdate = millis(); // measure how long the update takes
      lastBattery = currentTime;
      ok = updateBattery(); //<<<<<<<<<<<<<<<<<<<<------------ update the battery
      // you might want to insert code here to refuse operation if the battery is charging and the ECG electrodes are attached
      // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      // Check if electrodes are connected, fast than digitalRead()
      stateLOMinus = gpio_get_level((gpio_num_t)LO_MINUS);
      stateLOPlus  = gpio_get_level((gpio_num_t)LO_PLUS);
      if (((stateLOMinus > 0) || (stateLOPlus > 0)) && (batteryChargerate > 0.0)) {
        snprintf(tmpStr, sizeof(tmpStr), "DANGER: Electrodes minus %s, plus %s and charging at %f", (stateLOMinus > 0) ? "connected" : "disconnected",  (stateLOPlus > 0) ? "connected" : "disconnected", batteryChargerate); 
        if (mySettings.debuglevel > 0) { R_printSerialln(tmpStr); }
      }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBattery    < deltaUpdate) { maxUpdateBattery = deltaUpdate; }
      if (AllmaxUpdateBattery < deltaUpdate) { AllmaxUpdateBattery = deltaUpdate; }
    }
  }

  /**********************************************************************************************************************************/
  // OLED Display
  /**********************************************************************************************************************************/  
  if (oled_avail && mySettings.useOLED) {
    if ( (currentTime - lastOLED) >= intervalOLED ) {
      D_printSerial("D:U:OLED..");
      startUpdate = millis();
      lastOLED = currentTime;
      if      ( mySettings.OLEDDisplay == 1 ) { ok = updateECGDisplay(); }   // <<<<<<<<<<<<<<<<<<<<<------------  Update ECG data on display
      else if ( mySettings.OLEDDisplay == 2 ) { ok = updateBatteryDisplay(); } // <<<<<<<<<<<<<<<<<<<<------------  Update Battery data on display
      else { if (mySettings.debuglevel > 0) { R_printSerialln("OLED display configuration not yet supported"); } } 
      if ( (mySettings.debuglevel > 1) && mySettings.useOLED ) { R_printSerialln("OLED updated"); }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateOLED    < deltaUpdate) { maxUpdateOLED = deltaUpdate; }
      if (AllmaxUpdateOLED < deltaUpdate) { AllmaxUpdateOLED = deltaUpdate; }
    }
  }

  /**********************************************************************************************************************************/
  // Status & Performance
  /**********************************************************************************************************************************/    
  // This deals with debugging information if we want to know how long routines took to complete
  if ((currentTime - lastSYS) >= intervalSYS) {
    lastSYS = currentTime;      

    if (mySettings.debuglevel == 99) {   // update continously
      D_printSerial("D:U:SYS..");
      startUpdate = millis();
      printProfile();
      printSensors();
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
    } // dbg level 99
    
    // reset max values and measure them again until next display (usually every second)
    // AllmaxUpdate... are the runtime maxima that are not reset every second
    myLoopMin = intervalSYS; // anything but zero
    myLoopMax = 0;      
    yieldTimeMin = yieldTimeMax = 0;
    maxUpdateESPNow = maxUpdateBattery = maxUpdateOLED = 0;
    maxUpdateINPUT = maxUpdateOUTPUT = maxUpdateEEPROM = 0;
  }
  
  /**********************************************************************************************************************************/
  // Serial User Input
  /**********************************************************************************************************************************/
  // We will read serial input and decode it so that user can change settings without needing to recompile the program
  D_printSerial("D:U:SER..");
  startUpdate = millis();
  // Serial input capture
  if (Serial.available()) {
    char receivedChar = Serial.read();
    if ((receivedChar == '\n') || (receivedChar == '\r'))  {
      // End of input, process the buffer
      serialInputBuff[serialInputBuffIndex] = '\0'; // Null-terminate the string
      inputHandle(); // <<<<<<<<<<<<<<<<<<<<------------  Process the user input
      serialInputBuffIndex = 0; // Reset buffer index for next input
      yieldTime += yieldOS();
    } else if (serialInputBuffIndex < sizeof(serialInputBuff) - 1) {
        // Add character to buffer if there's space
        serialInputBuff[serialInputBuffIndex++] = receivedChar;
    }
  }
  deltaUpdate = millis() - startUpdate;
  if (maxUpdateINPUT < deltaUpdate) { maxUpdateINPUT = deltaUpdate; }
  if (AllmaxUpdateINPUT < deltaUpdate) { AllmaxUpdateINPUT = deltaUpdate; }

  /**********************************************************************************************************************************/
  // Serial Data Output, this code is for the base station. 
  // You dont need separate progrma for base stations, just turn off ADC, battery and other devices in the settings
  /**********************************************************************************************************************************/
  D_printSerial("D:U:SER..");
  startUpdate = millis();
  // We received EPSNow data and want to send it to serial port
  if (ESPNowData_avail) {
    uint8_t sensorID     = myESPNowDataIn.sensorID;         // ECG,Temp,Ox,Sound,etc..
    uint8_t numData      = myESPNowDataIn.numData;          // data points per channel
    uint8_t numChannels  = myESPNowDataIn.numChannels;      // number of channels
    uint8_t checksum     = myESPNowDataIn.checksum;         // checksum
    //check if checksums match
    myESPNowDataIn.checksum = 0; 
    const byte* bytePointer = reinterpret_cast<const byte*>(&myESPNowDataIn);
    uint8_t checksum_received = calculateChecksum(bytePointer, sizeof(myESPNowDataIn));
    if (checksum != checksum_received){
      snprintf(tmpStr, sizeof(tmpStr), "Checksums don't match %u %u", checksum, checksum_received );
      if (mySettings.debuglevel > 0) { R_printSerialln(tmpStr); }
    } else {
      if (!mySettings.useBinary) { //<<<<<<<<<<<<<<<<<<<<------------ report text data
        // Send the data as text over serial interface
        // Format is: SensorID, channel0, channel1, channel2, ... NewLine
        for (int i = 0; i < numData; i++ )  {                 // number of data points per channel
          int pos = snprintf(tmpStr, sizeof(tmpStr), "%u", sensorID);
          for (int j = 0; j < numChannels; j++ )  {           // number of channels
            int dataIndex = i * numChannels + j;              // data point in the array
            pos += snprintf(tmpStr + pos, sizeof(tmpStr) - pos, ",%d", myESPNowDataIn.dataArray[dataIndex]); 
            if (pos >= sizeof(tmpStr)) {                      // break if tmpStr is full
              break;
            }
          }
          R_printSerialln(tmpStr); yieldTime += yieldOS();
        }
      } else { // <<<<<<<<<<<<<<<<<<<<------------ Report binary data
        // Transmit the data in binary format over the serial port
        // First a start signature is sent
        // Then the data structure is sent byte by byte
        // At the end we send end signature
        Serial.write(startSignature, sizeof(startSignature));  // Start signature
        const byte* bytePointer = reinterpret_cast<const byte*>(&myESPNowDataIn);
        Serial.write(bytePointer, sizeof(myESPNowDataIn));     // Message data
        Serial.write(stopSignature, sizeof(stopSignature));    // Stop signature
        yieldTime += yieldOS();
      } // binary tranmission
    } // checksums matched
    ESPNowData_avail = false;
  }
  deltaUpdate = millis() - startUpdate;
  if (maxUpdateOUTPUT < deltaUpdate) { maxUpdateOUTPUT = deltaUpdate; }
  if (AllmaxUpdateOUTPUT < deltaUpdate) { AllmaxUpdateOUTPUT = deltaUpdate; }

  /**********************************************************************************************************************************/
  // Other Time Managed Events such as keeping track how long the program has been running and saving settings infrequently
  /**********************************************************************************************************************************/
  
  // Update runtime every minute -------------------------------------
  if ((currentTime - lastTime) >= intervalRuntime) { // 60,000 milli seconds
    lastTime = currentTime;
    D_printSerial("D:U:RUNTIME..");
    startUpdate = millis();
    mySettings.runTime = mySettings.runTime + ((currentTime - lastTime) / 1000);
    if (mySettings.debuglevel > 1) { R_printSerialln("Runtime updated"); }
    deltaUpdate = millis() - startUpdate;
    if (maxUpdateRT    < deltaUpdate) { maxUpdateRT = deltaUpdate; }
    if (AllmaxUpdateRT < deltaUpdate) { AllmaxUpdateRT = deltaUpdate; }
  }

  // Save Configuration infrequently ---------------------------------------
  if ((currentTime - lastSaveSettings) >= intervalSettings) { // 12 hours
    lastSaveSettings = currentTime;
    D_printSerial("D:U:EEPROM..");
    startUpdate = millis();
    EEPROM.put(eepromAddress, mySettings);
    if (EEPROM.commit()) {
      lastSaveSettings = currentTime;
      if (mySettings.debuglevel > 1) { R_printSerial("EEPROM updated"); }
    }
    deltaUpdate = millis() - startUpdate;
    if (maxUpdateEEPROM    < deltaUpdate) { maxUpdateEEPROM = deltaUpdate; }
    if (AllmaxUpdateEEPROM < deltaUpdate) { AllmaxUpdateEEPROM = deltaUpdate; }
    yieldTime += yieldOS(); 
  }

  /**********************************************************************************************************************************/
  // Keep track of free processor time 
  /**********************************************************************************************************************************/  
  // This comnputes at the end of the main loop how long the loop in average takes and what the longest time was it ever took.
  // If you are expecting to update data every 10ms but this loops frequently takes 50 or 100ms, you will be loosing data.
  // If you sometimes update the display and it takes 200ms to update it but you want to read a set of data every 10ms then you will
  // be loosing data. If everything works as expected, you no longer need to keep track of execution times, but I just keep this
  // going anyway as it does not take many resources. 
  myLoop  = millis() - currentTime;
  myLoopAvg = 0.9 * myLoopAvg + 0.1 * float(myLoop);
  if ( myLoop > 0 ) { 
    if ( myLoop < myLoopMin ) { myLoopMin = myLoop; }
    if ( myLoop > myLoopMax ) { 
      myLoopMax = myLoop; 
      if ( myLoop > myLoopMaxAllTime)  { myLoopMaxAllTime = myLoop; }
    }
  }

  if ( yieldTime > 0 ) { 
    if ( yieldTime < yieldTimeMin ) { yieldTimeMin = yieldTime; }
    if ( yieldTime > yieldTimeMax ) {
      yieldTimeMax = yieldTime; 
      if ( yieldTime > yieldTimeMaxAllTime)  { yieldTimeMaxAllTime = yieldTime; }
    }
  }

  // one could add code here to wait or put the processor to sleep until we need to run the loop again, 
  // but this often creates complications. Since we need to run the loop 100 times per second I just 
  // run the loop at full speed and let yieldOS occur the beginning of the loop.

} // end main loop

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Yield when necessary, there should be a yield/delay every 100ms to make sure the OS processes EPSNow
unsigned long yieldOS() {
  unsigned long beginYieldOS = millis(); // measure how long it takes to complete yield
  if ( (beginYieldOS - lastYield) > MAXUNTILYIELD ) { // only yield if sufficient time expired since previous yield
    yieldFunction(); // replaced with define statement at beginning of the this document
    lastYield = millis();
    return (lastYield - beginYieldOS);
  }
  return (millis()-beginYieldOS);
}

// Boot helper
// Prints message and waits until timeout or user send character on serial terminal
void serialTrigger(const char* mess, int timeout) {
  unsigned long startTime = millis();
  bool clearSerial = true;
  Serial.println(); Serial.println(mess);
  while ( !Serial.available() && (millis() - startTime < timeout) ) {
    delay(50);
  }
  while (Serial.available()) { Serial.read(); }
}

// Scan the I2C bus whether device is attached at address
bool checkI2C(uint8_t address, TwoWire *myWire) {
  myWire->beginTransmission(address);
  if (myWire->endTransmission() == 0) { return true; } else { return false; }
}

// Help Menu
// Expand if needed
void helpMenu() {  
  R_printSerialln(doubleSeparator); yieldTime += yieldOS(); 
  R_printSerialln(doubleSeparator);
  snprintf(tmpStr, sizeof(tmpStr), "ECG, 2023-2024, Team 12345678                                              %s",  VER); 
  R_printSerialln(tmpStr);
  R_printSerialln(doubleSeparator); 
  R_printSerialln("Supports........................................................................"); 
  R_printSerialln("....................................................................OLED SSD1306"); 
  R_printSerialln("........................................................................MAX1704X"); 
  R_printSerialln(".........................................................................ESP ADC"); 
  R_printSerialln("==General======================================================================="); 
  R_printSerialln("| All commands preceeded by ZZ                                                 |"); 
  R_printSerialln(singleSeparator);
  R_printSerialln("| ?:  help screen                       | .:  execution times                  |"); 
  R_printSerialln("| z:  print sensor data                 | b: set baudrate                      |"); 
  R_printSerialln("| s:  print current settings            | dd: default settings                 |");
  R_printSerialln("| Fs: save settings to EEPROM           | Fr: read settings from EEPROM        |"); yieldTime += yieldOS(); 
  R_printSerialln("==ADC===================================|=======================================");
  R_printSerialln("| As: sampling rate                     | Aa: attenuation                      |"); 
  R_printSerialln("| An: number of channels                | Av: average length                   |"); 
  R_printSerialln("| Ab: bitwidth                          |                                      |"); 
  R_printSerialln("| Ac0: channel 0  Ac012                 | Ac3: channel 3                       |"); 
  R_printSerialln("| Ac1: channel 1                        | Ac4: channel 4                       |"); 
  R_printSerialln("| Ac2: channel 2                        | Ac5: channel 5                       |"); yieldTime += yieldOS(); 
  R_printSerialln("==ESPNow================================|=======================================");
  R_printSerialln("| W:  print hostname                    | Wc: channel Wc0                      |"); 
  R_printSerialln("| Wb: set broadcast address             | We: encrypt We0 0/off 1/on           |"); 
  R_printSerialln("|     WbE1.A2.F3.C4.D5.B6               |                                      |"); 
  R_printSerialln("==OLED==================================|======================================="); 
  R_printSerialln("| L: display layout 1..2 L1               1:ECG 2:BATT                         |"); yieldTime += yieldOS(); 
  R_printSerialln("==Disable===============================|==Disable=============================="); 
  R_printSerialln("| x: 1 OLED on/off                      | x: 5 Serial on/off                   |"); 
  R_printSerialln("| x: 2 ESPNow on/off                    | x: 6 Binary on serial on/off         |"); 
  R_printSerialln("| x: 3 ECG on/off                       |                                      |"); 
  R_printSerialln("| x: 4 Battery on/off                   | x: 99 Reboot                         |"); yieldTime += yieldOS();  
  R_printSerialln("==Debug Level===========================|==Debug Level=========================="); 
  R_printSerialln("| l: 0 ALL off                          | l: 3                                 |"); 
  R_printSerialln("| l: 1 Errors only                      | l: 4                                 |"); 
  R_printSerialln("| l: 2 Debug                            | l: 5                                 |"); 
  R_printSerialln("| l: 99 continous                       |                                      |"); 
  R_printSerialln(doubleSeparator);
  R_printSerialln("| You might need to reboot with x99 to initialize the sensors                  |");
  R_printSerialln(doubleSeparator);
  yieldTime += yieldOS(); 
}

// create default settings
void defaultSettings() {
  strlcpy(mySettings.identifier, "ECG", sizeof(mySettings.identifier)); 
  mySettings.runTime                      = 0;      // cumulative runtime of software
  mySettings.debuglevel                   = 1;                      // set reporting level
  mySettings.useOLED                      = true;                   // enable/disable display
  mySettings.useESPNow                    = true;                   // enable/disable ESPNow
  mySettings.useSerial                    = true;                   // porivide Serial interface on USB
  mySettings.useBattery                   = true;                   // ... not used
  mySettings.useECG                       = true;                   // ... not used
  mySettings.useBinary                    = false;                  // ... not used
  mySettings.useDevice4                   = false;                  // ... not used
  mySettings.useDevice5                   = false;                  // ... not used
  mySettings.broadcastAddress[0]          = 0xdc;
  mySettings.broadcastAddress[1]          = 0x54;
  mySettings.broadcastAddress[2]          = 0x75;
  mySettings.broadcastAddress[3]          = 0xcf;
  mySettings.broadcastAddress[4]          = 0x9f;
  mySettings.broadcastAddress[5]          = 0x0c;
  mySettings.ESPNowChannel                = 0;
  mySettings.ESPNowEncrypt                = 0;
  mySettings.baudrate                     = 500000;                 // serial communication rate
  mySettings.samplerate                   = 44100;                  // Ananlog to digital converter sampling rage
  mySettings.channels                     = 1;                      // Number of channels to sample 1..6
  mySettings.bitwidth                     = 12;                     // Number of bits for ADC
  mySettings.atten                        = ADC_ATTEN_DB_11;        // Range of input signal
  mySettings.avglen                       = 8;                      // Downsampling by averaging avglen measurements 
  mySettings.channel0                     = ADC_CHANNEL_3 ;         // Sparkfun Thing Plus CH3 A3, Adafruit S3 CH4 D5
  mySettings.channel1                     = ADC_CHANNEL_0;          // 
  mySettings.channel2                     = ADC_CHANNEL_1;          // 
  mySettings.channel3                     = ADC_CHANNEL_2;          // 
  mySettings.channel4                     = ADC_CHANNEL_4;          // 
  mySettings.channel5                     = ADC_CHANNEL_5;          //
  mySettings.OLEDDisplay                  = 1;                      // Display type 1
} 

// Print settings
void printSettings() {
  printSerialln();
  R_printSerialln(doubleSeparator);
  snprintf(tmpStr, sizeof(tmpStr), "Identifier: %s Version: %s",   mySettings.identifier, VER ); 
  R_printSerialln(tmpStr);
  R_printSerialln("-System-----------------------------------------"); yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "Debug level: .................. %u",   mySettings.debuglevel ); 
  R_printSerialln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Runtime [min]: ................ %lu",  mySettings.runTime / 60); 
  R_printSerialln(tmpStr); 
  R_printSerialln("-Network----------------------------------------"); yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "ESPNow: ....................... %s",  (mySettings.useESPNow) ? mON : mOFF); 
  R_printSerialln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Broadcast Address: ............ %X:%X:%X:%X:%X:%X", mySettings.broadcastAddress[0],mySettings.broadcastAddress[1],
                                                                                        mySettings.broadcastAddress[2],mySettings.broadcastAddress[3],
                                                                                        mySettings.broadcastAddress[4],mySettings.broadcastAddress[5]);  
  R_printSerialln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Encryption: ................... %u",  mySettings.ESPNowEncrypt); 
  R_printSerialln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Channel: ...................... %u",  mySettings.ESPNowChannel); 
  R_printSerialln(tmpStr); yieldTime += yieldOS(); 
  
  R_printSerialln("-Serial-----------------------------------------");
  snprintf(tmpStr, sizeof(tmpStr), "Serial: ....................... %s",  mySettings.useSerial   ? mON : mOFF); 
  R_printSerialln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Serial Baudtrate: ............. %u",  mySettings.baudrate); 
  R_printSerialln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Binary: ....................... %s",  mySettings.useBinary   ? mON : mOFF); 
  R_printSerialln(tmpStr);
  R_printSerialln("-Battery----------------------------------------");
  snprintf(tmpStr, sizeof(tmpStr), "Battery: ...................... %s",  mySettings.useBattery  ? mON : mOFF); 
  R_printSerialln(tmpStr);
  R_printSerialln("-ADC--------------------------------------------");  yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "ECG: .......................... %s",  mySettings.useECG      ? mON : mOFF); 
  snprintf(tmpStr, sizeof(tmpStr), "Samplerate: ................... %u",  mySettings.samplerate); 
  R_printSerialln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Channels: ..................... %u",  mySettings.channels); 
  R_printSerialln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Bitwidth: ..................... %u",  mySettings.bitwidth); 
  R_printSerialln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Attenuation: .................. %u",  mySettings.atten); 
  R_printSerialln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Averages: ..................... %u",  mySettings.avglen); 
  R_printSerialln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 0: .................... %u",  mySettings.channel0); 
  R_printSerialln(tmpStr);  yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 1: .................... %u",  mySettings.channel1); 
  R_printSerialln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 2: .................... %u",  mySettings.channel2); 
  R_printSerialln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 3: .................... %u",  mySettings.channel3); 
  R_printSerialln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 4: .................... %u",  mySettings.channel4); 
  R_printSerialln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 5: .................... %u",  mySettings.channel5); 
  R_printSerialln(tmpStr); 
  R_printSerialln("-OLED-------------------------------------------"); yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "OLED: ......................... %s",  mySettings.useOLED     ? mON : mOFF); 
  R_printSerialln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Display type: ................. %u",  mySettings.OLEDDisplay);  
  R_printSerialln(tmpStr); 
  R_printSerialln(doubleSeparator); 
  yieldTime += yieldOS(); 
}

// Print sesor data
void printSensors() { 
  R_printSerialln();
  R_printSerialln(doubleSeparator); yieldTime += yieldOS(); 
  if (battery_avail && mySettings.useBattery) {
    snprintf(tmpStr, sizeof(tmpStr), "Battery charge %u [%%], rate: %4.1f[%%]", batteryPercent, batteryChargerate ); 
  } else {
    strlcpy(tmpStr, "Battery: not available", sizeof(tmpStr)); 
  }
  R_printSerialln(tmpStr);
  R_printSerialln(doubleSeparator); yieldTime += yieldOS(); 
}

// Print function profiles
void printProfile() {
  startUpdate = millis();
  printSerialln();
  R_printSerialln(doubleSeparator);
  
  snprintf(tmpStr, sizeof(tmpStr), "Average loop %d[us] Avg max loop %d[us] \r\nCurrent loop Min/Max/Alltime: %d/%d/%d[ms] Current: %d[ms]", int(myLoopAvg*1000), int(myLoopMaxAvg*1000), myLoopMin, myLoopMax, myLoopMaxAllTime, myLoop); 
  R_printSerialln(tmpStr);

  snprintf(tmpStr, sizeof(tmpStr), "Free Heap Size: %d[kB] Largest Free Block Size: %d[kB]", ESP.getFreeHeap()/1000, heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)/1000); 
  printSerialln(tmpStr);
  R_printSerialln(doubleSeparator); yieldTime += yieldOS(); 

  R_printSerialln("All values in [ms]");
  snprintf(tmpStr, sizeof(tmpStr), "ADC: Max/Alltime         %4u %4u ESPNow: Max/Alltime      %4u %4u Battery: Max/Alltime     %4u %4u OLED:    Max/Alltime     %4u %4u\r\nINPUT:   Max Alltime     %4u %4u OUTPUT:  Max Alltime     %4u %4u Runtime: Max/Alltime     %4u %4u EEPROM:  Max/Alltime     %4u %4u", 
                                    maxUpdateADC, AllmaxUpdateADC,        
                                    maxUpdateESPNow, AllmaxUpdateESPNow, 
                                    maxUpdateBattery, AllmaxUpdateBattery, 
                                    maxUpdateOLED, AllmaxUpdateOLED, 
                                    maxUpdateINPUT, AllmaxUpdateINPUT, 
                                    maxUpdateOUTPUT, AllmaxUpdateOUTPUT,
                                    maxUpdateRT, AllmaxUpdateRT,
                                    maxUpdateEEPROM, AllmaxUpdateEEPROM);
  R_printSerialln(tmpStr); yieldTime += yieldOS(); 

  deltaUpdate = millis() - startUpdate;
  if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
  if (AllmaxUpdateSYS < deltaUpdate) { AllmaxUpdateSYS = deltaUpdate; }
  snprintf(tmpStr, sizeof(tmpStr), "This display: Max/Alltime  %4u %4u", maxUpdateSYS, AllmaxUpdateSYS);
  R_printSerialln(tmpStr);
  R_printSerialln(doubleSeparator); yieldTime += yieldOS(); 

}

// Print interval settings
void printIntervals() {
  R_printSerialln(singleSeparator); yieldTime += yieldOS(); 
  R_printSerialln("Intervals:");
  if (mySettings.useOLED    && oled_avail)    { snprintf(tmpStr, sizeof(tmpStr), "OLED     %8lus",   intervalOLED/1000);   } else { strlcpy(tmpStr, ("OLED       n.a. "), sizeof(tmpStr)); } 
  R_printSerial(tmpStr);
  if (mySettings.useBattery && battery_avail) { snprintf(tmpStr, sizeof(tmpStr), "Battery  %5lus",   intervalBattery/1000);} else { strlcpy(tmpStr, ("Battery    n.a. "), sizeof(tmpStr)); }    
  R_printSerial(tmpStr);
  if (mySettings.useESPNow  && espnow_avail)  { snprintf(tmpStr, sizeof(tmpStr), "ESPNow   %5lus",   intervalESPNow/1000); } else { strlcpy(tmpStr, ("ESPNow     n.a. "), sizeof(tmpStr)); } 
  R_printSerial(tmpStr); 
  if (                         adc_avail)     { snprintf(tmpStr, sizeof(tmpStr), "ADC      %8lus",   intervalADC/1000);    } else { strlcpy(tmpStr, ("ADC        n.a. "), sizeof(tmpStr)); } 
  R_printSerialln(tmpStr); 
                                              { snprintf(tmpStr, sizeof(tmpStr), "System   %5lus",   intervalSYS/1000);                                                   }
  R_printSerial(tmpStr); 
                                              { snprintf(tmpStr, sizeof(tmpStr), "Settings %3luhrs", intervalSettings/3600000);                                           }    
  R_printSerial(tmpStr); 
                                              { snprintf(tmpStr, sizeof(tmpStr), "Runtime   %3lus",  intervalRuntime/1000);                                               }    
  R_printSerialln(tmpStr); 
                                            
  R_printSerialln(doubleSeparator); yieldTime += yieldOS();  
} // end print intervals

// This is XOR checksum calculation
byte calculateChecksum(const byte* data, size_t len) {
  byte checksum = 0;
  for (size_t i = 0; i < len; i++) {
    checksum ^= data[i];  // Example: XOR checksum
  }
  return checksum;
}