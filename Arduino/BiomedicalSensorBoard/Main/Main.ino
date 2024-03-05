//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ECG Sensor                                                                                                                       //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Supported Hardware:
//  - ESP32 micro controller.
//  - AD8232                                      Sparkfun ECG sensor, https://www.sparkfun.com/products/12650
//  - Sparkfun_MAX1704X                           Battery level sensor, integrated into Feather and Thing Plus
//                                                https://github.com/sparkfun/SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library/
// Data display through:
//  - Adafruit_SSD1306                            OLED Display, https://github.com/adafruit/Adafruit_SSD1306
// Network related libraries:
//  - ESPNow                                      ESPNow
// Other Libraries:
//  - Audio Tools                                 Continuous Analog Input, https://github.com/pschatzmann/arduino-audio-tools
//  - ArduJSON                                    MessagePack data serialization, https://github.com/bblanchon/ArduinoJson.git
//
// Software implementation:
//  Sensor driver are divided into initialization and update section.
//  The updates are a pre determined intervals in the main loop.
//  The intervals at which the updates are called are hardcoded.
//  All settings of the system are stored in EEPROM and can be modified at runtime without compiling the code.
//  To change the setting the user can obtain a menu by sending ZZ? to the serial console.
//  The code also measure time consumption in each subroutine and you can report that timing.
//  The code keeps a local max time over about 1 second and an all time max time. 
//  This allows to investigate if subroutines take too long to complete. 
//
//  For ADC continuous ADC on ESP32 is used. It is advised to sample between 20k and 80k samples per second
//  and to down sample with averaging. For example recording at 44100 samples per second and averaging
//  8 samples results in 5k samples per second.
//
// Communication:
//   ESPNow is used to send data to a base station.
//
//   In general EPSNow max payload is 250 bytes and the code reserves 240 bytes for data and 10 bytes for header. 
//   Since data is int16_t (2 bytes) we can record 120 samples at once. If we record with 5k samples per second 
//   we will fill the buffer every 20 ms and we need to make sure the main loop is completed in 20ms.
//   This also will result in the EPSNow message being send 50 times per second, which is a reasonable interval.
//
//   The structure being sent is defined in ESPNow.h
//   It consists of a head for sensor ID, number of data points per channel and number of channels
//   Data is in an array of 240 bytes.
//   The structure contains a checksum calculated with XOR and its end. 
//   To compute the checksum, one needs to set the checksum field to 0, then calculate the checksum and update the structure.
//
//   Data received at the base station is either printed to serial port in text or binary mode.
//   
//   In text mode each line contains Sensor ID, data_channel1, data_channel2 ... Newline
//   In binary mode each ESPNow packet is printed directly to serial directly byte by byte 
//   but its enclosed with start and end sequency of 2 bytes so that the serial receiver can identify start and end.
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
// # Message Format is little endian: 
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
//     return sensor_id, numData, numChannels, data_array, received_checksum
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/************************************************************************************************************************************/
// Includes
/************************************************************************************************************************************/
#include "CommonDefs.h"
#include "Config.h"
#include "ESPNow.h"
#include "ADC.h"
#include "ECG.h"
#include "Temperature.h"
#include "Sound.h"
#include "Battery.h"
#include "Print.h"
#include "OLED.h"
#include "BaseStation.h"

// Additional Libraries
#include <EEPROM.h>                            // storeing and reading settings
#include <esp_heap_caps.h>                     // for the system status heap memory

/************************************************************************************************************************************/
// Local Definitions
/************************************************************************************************************************************/

// LOOP Intervals
// --------------
// all times in milliSeconds
#define intervalADC                  10        // up to 100 times per second, assumption is that updateADC will not complete until buffer is full.
#define intervalECG                  10        // needs same interval as ADC as we have full ADC buffer after intervalADC and need to process it for ECG
#define intervalSound                10        // same as above if we use ADC
#define intervalTemperature          10        // same as above as we use ADC
#define intervalOx                   10        // depends on pulse oximeter I2C software
#define intervalImpedance            10        // depends on SPI impedance software
#define intervalOLED               1000        // up to once per second
#define intervalBattery           30000        // up twice per minute
#define intervalSYS                1000        // update system executing maximums every 1 sec
#define intervalRuntime           60000        // update every minute, how long the system has been running 
#define intervalSettings       43200000        // store runtime and settings every 12 hrs

/************************************************************************************************************************************/
// Global Variables
/************************************************************************************************************************************/

// Character array handling
char          tmpStr[256];                     // universal buffer for formatting texT
// Yielding
unsigned long yieldTime             = 0;       // how long we spent on yielding
unsigned long lastYield             = 0;       // keep record of time of last Yield event
// Serial user input
char serialInputBuff[64];                      // we expect new line after 63 characters

// Serial text line constants for reusing the same text
const char    singleSeparator[]     = "--------------------------------------------------------------------------------";
const char    doubleSeparator[]     = "================================================================================";
const char    mON[]                 = "on";
const char    mOFF[]                = "off";
const char    clearHome[]           = "\e[2J\e[H\r\n";
const char    waitmsg[]             = "Waiting 5 seconds, skip by hitting enter";

// I2C
TwoWire       myWire                = TwoWire(0); // universal I2C interface
const int     SDA_PIN               = SDAPIN;
const int     SCL_PIN               = SCLPIN;

/************************************************************************************************************************************/
// Persistent variables used only in the loop function
/************************************************************************************************************************************/

// Input character handling
int serialInputBuffIndex = 0;                              // to scan for characters or append text

// Time keeping
unsigned long currentTime;                                 // Populated at beginning of main loop
unsigned long lastTime;                                    // last time we update run time
unsigned long lastSYS;                                     // last time we updated performance values
unsigned long lastADC;                                     // last time we worked on analog to digital conversion
unsigned long lastBattery;                                 // last time we update the battery status
unsigned long lastSaveSettings;                            // last time we updated EEPROM, should occur every couple days
unsigned long lastESPNow;                                  // last time we sent data over ESPNow
unsigned long lastOLED;                                    // last time we updated the display
unsigned long lastECG;                                     // last time we updated ECG
unsigned long lastOx;                                      // last time we updated Oximeter
unsigned long lastTemperature;                             // last time we updated Temperature
unsigned long lastSound;                                   // last time we updated Sound
unsigned long lastImpedance;                               // last time we updated Impedance
unsigned long lastBaseStation;                             // last time we updated Base Station

// Loop execution
unsigned long myLoop                 =        0;           // main loop execution time, automatically computed
unsigned long myLoopMin              =        0;           // main loop recent shortest execution time, automatically computed
unsigned long myLoopMax              =        0;           // main loop recent maximal execution time, automatically computed
unsigned long myLoopMaxAllTime       =        0;           // main loop all time maximum execution time, automatically computed
float         myLoopAvg              =        0;           // average delay over the last couple seconds
float         myLoopMaxAvg           =        0;           // average max loop delay over the last couple seconds

// Yielding
unsigned long yieldTimeMin           =     1000;           // shortest amount of time spend on yield
unsigned long yieldTimeMax           =        0;           // longest time we spent on yield over the last 1 second
unsigned long yieldTimeMaxAllTime    =        0;           // longest time we spent on yield for ever.

// Execution times
unsigned long startUpdate            =        0;
unsigned long deltaUpdate            =        0;
// Current max update times
unsigned long maxUpdateOLED          =        0;           // max time spent on display
unsigned long maxUpdateBattery       =        0;           // max time spent on battery
unsigned long maxUpdateSerIn         =        0;           // max time spent on serial input
unsigned long maxUpdateSerOut        =        0;           // max time spent on serial output (base station)
unsigned long maxUpdateSYS           =        0;           // max time spent on performance updates
unsigned long maxUpdateRT            =        0;           // max time spent on updating runt ime
unsigned long maxUpdateADC           =        0;           // max time spent on analog to digital conversion
unsigned long maxUpdateEEPROM        =        0;           // max time spent on settings
unsigned long maxUpdateECG           =        0;           // max time spent on ecg
unsigned long maxUpdateTemp          =        0;           // max time spent on 
unsigned long maxUpdateSound         =        0;           // max time spent on 
unsigned long maxUpdateOx            =        0;           // max time spent on
unsigned long maxUpdateImpedance     =        0;           // max time spent on
unsigned long maxUpdateBaseStation   =        0;           // max time spent on

// Alltime max update times
unsigned long AllmaxUpdateESPNow     =        0;
unsigned long AllmaxUpdateOLED       =        0;
unsigned long AllmaxUpdateSerIn      =        0; 
unsigned long AllmaxUpdateSerOut     =        0; 
unsigned long AllmaxUpdateSYS        =        0;
unsigned long AllmaxUpdateRT         =        0;
unsigned long AllmaxUpdateADC        =        0;
unsigned long AllmaxUpdateBattery    =        0;
unsigned long AllmaxUpdateEEPROM     =        0;
unsigned long AllmaxUpdateECG        =        0;           // max time spent on ecg
unsigned long AllmaxUpdateTemp       =        0;           // max time spent on 
unsigned long AllmaxUpdateSound      =        0;           // max time spent on 
unsigned long AllmaxUpdateOx         =        0;           // max time spent on
unsigned long AllmaxUpdateImpedance  =        0;           // max time spent on
unsigned long AllmaxUpdateBaseStation=        0;           // max time spent on

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP SYSTEM
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

  byte error, address;

  /************************************************************************************************************************************/
  // Read configuration from EEPROM
  /************************************************************************************************************************************/
  // EEPROM is a section on the flash memory of the ESP. When you compile a program you can reserve how much is used for program space
  // and whether you have a file system emulating a regular flash drive and you can have a short section used to store settings. 
  // These settings are loaded here each time the program starts. We can change the settings with commands from the serial port
  // to make them available next time the system boots.
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(eepromAddress, mySettings);
  // Inject hard coded values into the settings, when we run the program the first time.
  #ifdef INITSETTINGS 
    defaultSettings(); 
    yieldTime += yieldOS(); 
  #endif

  /************************************************************************************************************************************/
  // When EEPROM is used for the first time, it has random content.
  // At boot time we  need to ensure that any non zero value for boolean settings are forced to true.
  // Once the variables a set with code below and written back to EEPROM, this should not be necessary any longer.
  mySettings.useOLED        = (mySettings.useOLED        > 0) ? true: false;
  mySettings.useESPNow      = (mySettings.useESPNow      > 0) ? true: false;
  mySettings.useSerial      = (mySettings.useSerial      > 0) ? true: false;
  mySettings.useBattery     = (mySettings.useBattery     > 0) ? true: false;
  mySettings.useECG         = (mySettings.useECG         > 0) ? true: false;
  mySettings.useBinary      = (mySettings.useBinary      > 0) ? true: false;
  mySettings.useTemperature = (mySettings.useTemperature > 0) ? true: false;
  mySettings.useSound       = (mySettings.useSound       > 0) ? true: false;
  mySettings.useOx          = (mySettings.useOx          > 0) ? true: false;
  mySettings.useImpedance   = (mySettings.useImpedance   > 0) ? true: false;
  mySettings.useBaseStation = (mySettings.useBaseStation > 0) ? true: false;

  /************************************************************************************************************************************/
  // Serial Interface
  /************************************************************************************************************************************/
  Serial.begin(mySettings.baudrate);
  serialTrigger(waitmsg, 5000); // waits 5 seconds until serial is available and initialization continues, otherwise you can not see I2c scan

  /************************************************************************************************************************************/
  // Initialize I2C
  /************************************************************************************************************************************/
  // Searches for devices connected to I2C, this is my general I2C bus scanning routine to find any I2C devices.
  // You can add other know addresses.
  snprintf(tmpStr, sizeof(tmpStr), "Scanning (SDA:SCL) - %u:%u", SDA_PIN, SCL_PIN); 
  if (mySettings.debuglevel > 1) {  Serial.println(tmpStr); }

  myWire.begin(SDA_PIN, SCL_PIN);                     // init I2C pins
  myWire.setClock(I2CSPEED);                          // init I2C speed
  // myWire.setClockStretchLimit(I2CCLOCKSTRETCH);    // ESP32 does not have clock stretch settings apparently
  oled_avail    = false;
  battery_avail = false;
  for (address = 1; address < 127; address++ )  {     // scan all 127 addresses
    myWire.beginTransmission(address);                // check if something is connected at this address
    error = myWire.endTransmission();                 // finish up and check if a device responded
    if (error == 0) {
      // found a device
      if (address == OLEDADDR) {
        oled_avail  = true;                           // OLED display Adafruit
        snprintf(tmpStr, sizeof(tmpStr), "OLED    - 0x%02X", address); 
      } else if (address == BATTERYADDR) {
        battery_avail  = true;                        // Battery gauge
        snprintf(tmpStr, sizeof(tmpStr), "Battery - 0x%02X", address); 
      } else {                                        // Something else is connected
        snprintf(tmpStr, sizeof(tmpStr), "Other   - 0x%02X", address); 
      }
      if (mySettings.debuglevel > 1) { Serial.println(tmpStr); }
    }
  }
  if (mySettings.debuglevel > 1) {  
    LOG_Iln("I2C scan completed"); 
    // if (oled_avail)    { LOG_Iln("OLED available");}    else {LOG_Iln("OLED not available");}
    // if (battery_avail) { LOG_Iln("Battery available");} else {LOG_Iln("Battery not available");}
  }
  
  /************************************************************************************************************************************/
  // Initialize Devices
  /************************************************************************************************************************************/
  if (mySettings.useECG || mySettings.useTemperature || mySettings.useSound) 
                                              { adc_avail     = initializeADC();       yieldTime += yieldOS(); } // see ADC.ino and ADC.h
  if (mySettings.useECG)                      { ecg_avail     = initializeECG();       yieldTime += yieldOS(); } // see ADC.ino and ADC.h
  if (mySettings.useBattery && battery_avail) { battery_avail = initializeBattery();   yieldTime += yieldOS(); } // see Battery.ino and Battery.h
  if (mySettings.useOLED && oled_avail)       { oled_avail    = initializeOLED();      yieldTime += yieldOS(); } // see OLED.ino and OLED.h 
  if (mySettings.useESPNow)                   { espnow_avail  = initializeESPNow();    yieldTime += yieldOS(); } // see ESPNow.ino and ESPNow.h

  // No initialization for BaseStation
  // No initialization for ECG as ADC is used and has its own initialization
  // No initialization for Temperature as it uses ADC.
  // Initialization for Sound to be developed
  // Initialization for Impedance to be developed
  // Initialization for Pulseoximeter to be developed

  if (mySettings.debuglevel > 1) { LOG_Iln("Initialization completed"); }

} // END setup

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN LOOP
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {

  bool ok;

  // Yield to Operating System (RTOS)
  yieldTime = yieldOS();
  // Initialize the time at start of the loop
  currentTime = lastYield = millis();
  
  /**********************************************************************************************************************************/
  // Update ADC Readings
  /**********************************************************************************************************************************/
  if (adc_avail && (mySettings.useECG || mySettings.useTemperature || mySettings.useSound)) { 
    if ( (currentTime - lastADC) >= intervalADC ) {
      lastADC = currentTime;                // we need to keep track when we ran the ADC update
      startUpdate = millis();               // we want to know how long it took to obtain the sensor readings
      ok = updateADC();                     //<<<<<<<<<<<<<<<<<<<<--------------- ADC Update
      deltaUpdate = millis() - startUpdate; // this is how long it took to update the ADC readings
      if (maxUpdateADC    < deltaUpdate) { maxUpdateADC = deltaUpdate; }    // This keeps track of longest update over last couple seconds
      if (AllmaxUpdateADC < deltaUpdate) { AllmaxUpdateADC = deltaUpdate; } // This keeps track of maximum time it ever took to update
      yieldTime += yieldOS();               // yield to Operating System
    }
  } 

  /**********************************************************************************************************************************/
  // Update ECG
  /**********************************************************************************************************************************/
  if (adc_avail && mySettings.useECG) { 
    if ( (currentTime - lastECG) >= intervalECG ) {
      lastECG = currentTime;                // we need to keep track when we ran the ADC update
      startUpdate = millis();               // we want to know how long it took to obtain the sensor readings
      ok = updateECG();                     //<<<<<<<<<<<<<<<<<<<<--------------- ECG Update
      deltaUpdate = millis() - startUpdate; // this is how long it took to update the ADC readings
      if (maxUpdateECG    < deltaUpdate) { maxUpdateECG = deltaUpdate; }    // This keeps track of longest update over last couple seconds
      if (AllmaxUpdateECG < deltaUpdate) { AllmaxUpdateECG = deltaUpdate; } // This keeps track of maximum time it ever took to update
      yieldTime += yieldOS();               // yield to Operating System
    }
  } 

  /**********************************************************************************************************************************/
  // Update Temperature
  /**********************************************************************************************************************************/

  if (adc_avail && mySettings.useTemperature) { 
    if ( (currentTime - lastECG) >= intervalTemperature ) {
      lastECG = currentTime;                // we need to keep track when we ran the ADC update
      startUpdate = millis();               // we want to know how long it took to obtain the sensor readings
      ok = updateTemp();                    //<<<<<<<<<<<<<<<<<<<<--------------- Temperature Update
      deltaUpdate = millis() - startUpdate; // this is how long it took to update the Temp readings
      if (maxUpdateTemp    < deltaUpdate) { maxUpdateTemp = deltaUpdate; }    // This keeps track of longest update over last couple seconds
      if (AllmaxUpdateTemp < deltaUpdate) { AllmaxUpdateTemp = deltaUpdate; } // This keeps track of maximum time it ever took to update
      yieldTime += yieldOS();               // yield to Operating System
    }
  } 

  /**********************************************************************************************************************************/
  // Update Sound
  /**********************************************************************************************************************************/
  if (adc_avail && mySettings.useSound) { 
    if ( (currentTime - lastSound) >= intervalSound ) {
      lastSound= currentTime;               // we need to keep track when we ran the Sound update
      startUpdate = millis();               // we want to know how long it took to obtain the sensor readings
      ok = updateSound();                   //<<<<<<<<<<<<<<<<<<<<--------------- Sound Update
      deltaUpdate = millis() - startUpdate; // this is how long it took to update the Sound readings
      if (maxUpdateSound    < deltaUpdate) { maxUpdateSound = deltaUpdate; }    // This keeps track of longest update over last couple seconds
      if (AllmaxUpdateSound < deltaUpdate) { AllmaxUpdateSound = deltaUpdate; } // This keeps track of maximum time it ever took to update
      yieldTime += yieldOS();               // yield to Operating System
    }
  } 

  /**********************************************************************************************************************************/
  // Update Pulse Oximeter
  /**********************************************************************************************************************************/
  if (mySettings.useOx) { 
    if ( (currentTime - lastOx) >= intervalOx ) {
      lastOx = currentTime;                 // we need to keep track when we ran the Oximeter update
      startUpdate = millis();               // we want to know how long it took to obtain the sensor readings
      // ok = updateOx();                   //<<<<<<<<<<<<<<<<<<<<--------------- Oximeter Update
      deltaUpdate = millis() - startUpdate; // this is how long it took to update the Oximeter readings
      if (maxUpdateOx    < deltaUpdate) { maxUpdateOx = deltaUpdate; }    // This keeps track of longest update over last couple seconds
      if (AllmaxUpdateOx < deltaUpdate) { AllmaxUpdateOx = deltaUpdate; } // This keeps track of maximum time it ever took to update
      yieldTime += yieldOS();               // yield to Operating System
    }
  }

  /**********************************************************************************************************************************/
  // Update Impedance
  /**********************************************************************************************************************************/
  if (mySettings.useImpedance) { 
    if ( (currentTime - lastImpedance) >= intervalImpedance ) {
      lastImpedance = currentTime;          // we need to keep track when we ran the Impedance update
      startUpdate = millis();               // we want to know how long it took to obtain the sensor readings
      // ok = updateImpedance();            //<<<<<<<<<<<<<<<<<<<<--------------- Impedance Update
      deltaUpdate = millis() - startUpdate; // this is how long it took to update the Impedance readings
      if (maxUpdateImpedance    < deltaUpdate) { maxUpdateImpedance = deltaUpdate; }    // This keeps track of longest update over last couple seconds
      if (AllmaxUpdateImpedance < deltaUpdate) { AllmaxUpdateImpedance = deltaUpdate; } // This keeps track of maximum time it ever took to update
      yieldTime += yieldOS();               // yield to Operating System
    }
  }

  /**********************************************************************************************************************************/
  // Battery
  /**********************************************************************************************************************************/
  if (battery_avail && mySettings.useBattery) {
    if ( (currentTime - lastBattery) >= intervalBattery ) {
      lastBattery = currentTime;            //
      startUpdate = millis();               // measure how long the update takes
      ok = updateBattery();                 //<<<<<<<<<<<<<<<<<<<<------------ update the battery
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBattery    < deltaUpdate) { maxUpdateBattery = deltaUpdate; }    //
      if (AllmaxUpdateBattery < deltaUpdate) { AllmaxUpdateBattery = deltaUpdate; } //
      yieldTime += yieldOS();               // yield to Operating System
    }
  }

  /**********************************************************************************************************************************/
  // OLED Display
  /**********************************************************************************************************************************/  
  if (oled_avail && mySettings.useOLED) {
    if ( (currentTime - lastOLED) >= intervalOLED ) {
      lastOLED = currentTime;               //
      startUpdate = millis();               //
      if      ( mySettings.OLEDDisplay == 1 ) { ok = updateECGDisplay(); }   //   <<<<<<<<<<<<<<<<<<<<------------  Update ECG data on display
      else if ( mySettings.OLEDDisplay == 2 ) { ok = updateBatteryDisplay(); } // <<<<<<<<<<<<<<<<<<<<------------  Update Battery data on display
      else { if (mySettings.debuglevel > 0) { LOG_Iln("OLED display configuration not yet supported"); } } 
      if ( (mySettings.debuglevel > 1) && mySettings.useOLED ) { LOG_Iln("OLED updated"); }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateOLED    < deltaUpdate) { maxUpdateOLED = deltaUpdate; }          //
      if (AllmaxUpdateOLED < deltaUpdate) { AllmaxUpdateOLED = deltaUpdate; }       //
      yieldTime += yieldOS();               // yield to Operating System
    }
  }

  /**********************************************************************************************************************************/
  // Status & Performance
  /**********************************************************************************************************************************/    
  // This deals with debugging information if we want to know how long routines took to complete
  if ((currentTime - lastSYS) >= intervalSYS) {
    lastSYS = currentTime;      
    if (mySettings.debuglevel == 99) {      // update continuously
      LOG_D("D:U:SYS..");
      startUpdate = millis();               //
      printProfile();                       //
      printSensors();                       //
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }               //
    } // dbg level 99
    
    // reset max values and measure them again until next display (usually every second)
    // AllmaxUpdate... are the runtime maxima that are not reset every second
    myLoopMin = intervalSYS;                                                        // anything but zero
    myLoopMax = 0;                                                                  //
    yieldTimeMin = yieldTimeMax = 0;                                                //
    maxUpdateBattery = maxUpdateOLED = 0;                                           //
    maxUpdateSerIn = maxUpdateSerOut = maxUpdateEEPROM = maxUpdateECG = 0;          //
    maxUpdateTemp = maxUpdateSound = maxUpdateImpedance = maxUpdateOx = 0;          //
  }
  
  /**********************************************************************************************************************************/
  // Serial User Input
  /**********************************************************************************************************************************/
  // We will read serial input and decode it so that user can change settings without needing to recompile the program
  startUpdate = millis();
  // Serial input capture
  if (Serial.available()) {
    LOG_Dln("D:U:SER..");
    char receivedChar = Serial.read();
    if ((receivedChar == '\n') || (receivedChar == '\r'))  {
      // End of input, process the buffer
      serialInputBuff[serialInputBuffIndex] = '\0'; // Null-terminate the string
      inputHandle();                                // <<<<<<<<<<<<<<<<<<<<------------  Process the user input
      serialInputBuffIndex = 0;                     // Reset buffer index for next input
    } else if (serialInputBuffIndex < sizeof(serialInputBuff) - 1) {
        // Add character to buffer if there's space
        serialInputBuff[serialInputBuffIndex++] = receivedChar;
        if (serialInputBuffIndex == sizeof(serialInputBuff) - 1) {
          // reached end of buffer
          serialInputBuffIndex = sizeof(serialInputBuff) - 2; // Reset buffer index for next input
        }
    }
  }
  deltaUpdate = millis() - startUpdate;                                             //
  if (maxUpdateSerIn < deltaUpdate) { maxUpdateSerIn = deltaUpdate; }               //
  if (AllmaxUpdateSerIn < deltaUpdate) { AllmaxUpdateSerIn = deltaUpdate; }         //

  /**********************************************************************************************************************************/
  // Serial Data Output, this code is for the base station. 
  // You dont need separate program for base stations, just turn off ADC, battery and other devices in the settings
  /**********************************************************************************************************************************/
  if (mySettings.useBaseStation) { 
    startUpdate = millis();                         //
    ok = updateBaseStation();                       //<<<<<<<<<<<<<<<<<<<------------  Update the Base Station
    deltaUpdate = millis() - startUpdate;
    if (maxUpdateSerOut < deltaUpdate) { maxUpdateSerOut = deltaUpdate; }           //
    if (AllmaxUpdateSerOut < deltaUpdate) { AllmaxUpdateSerOut = deltaUpdate; }     //
  } 

  /**********************************************************************************************************************************/
  // Other Time Managed Events such as keeping track how long the program has been running and saving settings infrequently
  /**********************************************************************************************************************************/
  
  // Update runtime every minute -------------------------------------
  if ((currentTime - lastTime) >= intervalRuntime) { // 60,000 milli seconds
    lastTime = currentTime;
    LOG_Dln("D:U:RUNTIME..");
    startUpdate = millis();
    mySettings.runTime = mySettings.runTime + ((currentTime - lastTime) / 1000);
    if (mySettings.debuglevel > 1) { LOG_Iln("Runtime updated"); }
    deltaUpdate = millis() - startUpdate;
    if (maxUpdateRT    < deltaUpdate) { maxUpdateRT = deltaUpdate; }
    if (AllmaxUpdateRT < deltaUpdate) { AllmaxUpdateRT = deltaUpdate; }
  }

  // Save Configuration infrequently ---------------------------------------
  if ((currentTime - lastSaveSettings) >= intervalSettings) { // 12 hours
    lastSaveSettings = currentTime;
    LOG_Dln("D:U:EEPROM..");
    startUpdate = millis();
    EEPROM.put(eepromAddress, mySettings);
    if (EEPROM.commit()) {
      lastSaveSettings = currentTime;
      if (mySettings.debuglevel > 1) { LOG_I("EEPROM updated"); }
    }
    deltaUpdate = millis() - startUpdate;
    if (maxUpdateEEPROM    < deltaUpdate) { maxUpdateEEPROM = deltaUpdate; }
    if (AllmaxUpdateEEPROM < deltaUpdate) { AllmaxUpdateEEPROM = deltaUpdate; }
    yieldTime += yieldOS(); 
  }

  /**********************************************************************************************************************************/
  // Keep track of free processor time 
  /**********************************************************************************************************************************/  
  // This computes at the end of the main loop how long the loop in average takes and what the longest time was it ever took.
  // If you are expecting it to update data every 10ms but this loops frequently takes 50 or 100ms, you will be loosing data.
  // If you updating the display takes 200ms you will be loosing data and we will need to change the display routines. 
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
  // but this often creates complications. For example you might need to restart WiFi after sleeping. 
  // Since we need to run the loop 100 times per second, there is no opprotunity to go to sleep
  // I just run the loop without throttling and let yieldOS occur at the beginning of the loop.

} // end main loop

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Yield when necessary, there should be a yield/delay every 100ms to make sure the OS processes EPSNow input/output.
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

// Print sensor data
void printSensors() { 
  LOG_Iln();
  LOG_Iln(doubleSeparator); yieldTime += yieldOS(); 
  if (battery_avail && mySettings.useBattery) {
    snprintf(tmpStr, sizeof(tmpStr), "Battery charge %u [%%], rate: %4.1f[%%]", batteryPercent, batteryChargerate ); 
  } else {
    strlcpy(tmpStr, "Battery: not available", sizeof(tmpStr)); 
  }
  LOG_Iln(tmpStr);
  LOG_Iln(doubleSeparator); yieldTime += yieldOS(); 
}

// Print function profiles
void printProfile() {
  startUpdate = millis();
  LOG_Iln(doubleSeparator);
  
  snprintf(tmpStr, sizeof(tmpStr), "Average loop %d[us] Avg max loop %d[us] \r\nCurrent loop Min/Max/Alltime: %d/%d/%d[ms] Current: %d[ms]", int(myLoopAvg*1000), int(myLoopMaxAvg*1000), myLoopMin, myLoopMax, myLoopMaxAllTime, myLoop); 
  LOG_Iln(tmpStr);

  snprintf(tmpStr, sizeof(tmpStr), "Free Heap Size: %d[kB] Largest Free Block Size: %d[kB]", ESP.getFreeHeap()/1000, heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)/1000); 
  LOG_Iln(tmpStr);
  LOG_Iln(doubleSeparator); yieldTime += yieldOS(); 

  LOG_Iln("All values in [ms]");
  snprintf(tmpStr, sizeof(tmpStr), "ADC:    Max/Alltime %4u %4u Battery: Max/Alltime %4u %4u OLED:      Max/Alltime %4u %4u\r\n"
                                   "SerIn:  Max/Alltime %4u %4u Output:  Max/Alltime %4u %4u Runtime:   Max/Alltime %4u %4u\r\n"
                                   "EEPROM: Max/Alltime %4u %4u ECG:     Max/Alltime %4u %4u Sound:     Max/Alltime %4u %4u\r\n"
                                   "Temp:   Max/Alltime %4u %4u Ox:      Max/Alltime %4u %4u Impedance: Max/Alltime %4u %4u",
                                    maxUpdateADC,    AllmaxUpdateADC,    maxUpdateBattery, AllmaxUpdateBattery, maxUpdateOLED,      AllmaxUpdateOLED, 
                                    maxUpdateSerIn,  AllmaxUpdateSerIn,  maxUpdateSerOut,  AllmaxUpdateSerOut,  maxUpdateRT,        AllmaxUpdateRT,
                                    maxUpdateEEPROM, AllmaxUpdateEEPROM, maxUpdateECG,     AllmaxUpdateECG,     maxUpdateSound,     AllmaxUpdateSound,
                                    maxUpdateTemp,   AllmaxUpdateTemp,   maxUpdateOx,      AllmaxUpdateOx,      maxUpdateImpedance, AllmaxUpdateImpedance
                                    );
  LOG_Iln(tmpStr); yieldTime += yieldOS(); 

  deltaUpdate = millis() - startUpdate;
  if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
  if (AllmaxUpdateSYS < deltaUpdate) { AllmaxUpdateSYS = deltaUpdate; }
  snprintf(tmpStr, sizeof(tmpStr), "This display: Max/Alltime  %4u %4u", maxUpdateSYS, AllmaxUpdateSYS);
  LOG_Iln(tmpStr);
  LOG_Iln(doubleSeparator); yieldTime += yieldOS(); 

}

// Print interval settings
void printIntervals() {
  LOG_Iln(singleSeparator); yieldTime += yieldOS(); 
  LOG_Iln("Intervals:");
  if (mySettings.useOLED    && oled_avail)    { snprintf(tmpStr, sizeof(tmpStr), "OLED      %8lus",   intervalOLED/1000);        } else { strlcpy(tmpStr, ("OLED       n.a. "), sizeof(tmpStr)); } 
  LOG_I(tmpStr);
  if (mySettings.useBattery && battery_avail) { snprintf(tmpStr, sizeof(tmpStr), "Battery   %5lus",   intervalBattery/1000);     } else { strlcpy(tmpStr, ("Battery    n.a. "), sizeof(tmpStr)); }    
  LOG_I(tmpStr);
  if (                         adc_avail)     { snprintf(tmpStr, sizeof(tmpStr), "ADC       %8lus",   intervalADC/1000);         } else { strlcpy(tmpStr, ("ADC        n.a. "), sizeof(tmpStr)); } 
  LOG_Iln(tmpStr); 
  if (mySettings.useECG)                      { snprintf(tmpStr, sizeof(tmpStr), "ECG       %5lus",   intervalECG/1000);         } else { strlcpy(tmpStr, ("ECG        n.a. "), sizeof(tmpStr)); }    
  LOG_Iln(tmpStr); 
  if (mySettings.useECG)                      { snprintf(tmpStr, sizeof(tmpStr), "Sound     %5lus",   intervalSound/1000);       } else { strlcpy(tmpStr, ("Sound      n.a. "), sizeof(tmpStr)); }    
  LOG_Iln(tmpStr); 
  if (mySettings.useTemperature)              { snprintf(tmpStr, sizeof(tmpStr), "Temp      %5lus",   intervalTemperature/1000); } else { strlcpy(tmpStr, ("Temparture n.a. "), sizeof(tmpStr)); }    
  LOG_Iln(tmpStr); 
  if (mySettings.useOx)                       { snprintf(tmpStr, sizeof(tmpStr), "Oximeter  %5lus",   intervalOx/1000);          } else { strlcpy(tmpStr, ("Oximeter   n.a. "), sizeof(tmpStr)); }    
  LOG_Iln(tmpStr); 
  if (mySettings.useImpedance)                { snprintf(tmpStr, sizeof(tmpStr), "Impedance %5lus",   intervalImpedance/1000);   } else { strlcpy(tmpStr, ("Impedance  n.a. "), sizeof(tmpStr)); }    
  LOG_Iln(tmpStr); 
                                              { snprintf(tmpStr, sizeof(tmpStr), "System    %5lus",   intervalSYS/1000);         }
  LOG_I(tmpStr); 
                                              { snprintf(tmpStr, sizeof(tmpStr), "Settings  %3luhrs", intervalSettings/3600000); }    
  LOG_I(tmpStr); 
                                              { snprintf(tmpStr, sizeof(tmpStr), "Runtime   %3lus",   intervalRuntime/1000);     }    
  LOG_Iln(tmpStr); 
                                            
  LOG_Iln(doubleSeparator); yieldTime += yieldOS();  
} // end print intervals

// This is XOR checksum calculation
byte calculateChecksum(const byte* data, size_t len) {
  byte checksum = 0;
  for (size_t i = 0; i < len; i++) {
    checksum ^= data[i];  // XOR checksum
  }
  return checksum;
}