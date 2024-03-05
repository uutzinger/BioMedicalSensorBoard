/******************************************************************************************************/
// Common Definitions
/******************************************************************************************************/
#ifndef COMMON_H_
#define COMMON_H_

/**************************************************************************************/
// Include common libraries
/**************************************************************************************/
#include "Arduino.h"

// Character Array Modifications
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
// I2C
#include <Wire.h>
// Math
#include <math.h>
// ESP
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp32-hal-log.h>
#include <esp32-hal-periman.h>
#include <esp32/rom/gpio.h>
// #include <soc/dac_channel.h>
#include <soc/adc_channel.h>
#include <soc/rtc.h>
#include <driver/i2s.h>
#include <driver/gpio.h>
#include <driver/adc.h>
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_cali_scheme.h>

/************************************************************************************************************************************/
// Build Configuration
/************************************************************************************************************************************/
#define VER "0.0.2"

// Debug
// -----
//#undef DEBUG                                              // disable "crash" DEBUG output for production
#define DEBUG                                           // enabling "crash" debug will pinpoint where in the code the program crashed 

// Settings
#define INITSETTINGS                                      // create new eeprom settings, comment for production
//#undef INITSETTINGS                                     // uncomment for production

// Yield
// -----
// At many locations in the program, yieldOS was inserted as functions can take more than 100ms to complete and the
// ESP operating system will need to take care of wireless and other system routines. Calling delay() or yield() gives
// OS opportunity to catch up.
// yieldOS keeps track when it was called the last time and calls the yield function if the interval is exceeded. 
#define MAXUNTILYIELD 50                                   // time in ms until yield should be called
// Options for yieldOS:
//#define yieldFunction() delay(1)                         // delay(1), wait at least 1 ms
#define yieldFunction() yield()                            // just yield, runs operating system functions if needed
//#define yieldFunction()                                  // no yield, only if you are sure that system runs stable without yield

// Serial
// ------
#define BAUDRATE        500000                             // can go up to 2,000,000 but 500k is stable on ESP32

// ECG Hardware Pin Configuration
// --------------------------
// You need to wire the ECG board to these pins.
#define SDN                 16                             // ENABLE ECG is on pin 16
#define LO_MINUS            17                             // Electrode Disconnected is detected on pin 17
#define LO_PLUS              4                             // Electrode Disconnected is detected on pin 4

// I2C Configuration
// --------------------------
// We will use the following pins and settings for I2C.
// Adafruit Feather S3
// SDA 7 41 GPIO3
// SCL 6 40 GPIO4
// Sparkfun Thing Plus C
// SDA 23
// SCL 22
#define SDAPIN               3                             // I2C Data, Adafruit ESP32 S3 3, Sparkfun Thing Plus C 23
#define SCLPIN               4                             // I2C Clock, Adafruit ESP32 S3 4, Sparkfun Thing Plus C 22
//#define SDAPIN            23                             // I2C Data, Adafruit ESP32 S3 3, Sparkfun Thing Plus C 23
//#define SCLPIN            22                             // I2C Clock, Adafruit ESP32 S3 4, Sparkfun Thing Plus C 22
#define I2CSPEED        400000                             // Clock Rate
#define I2CCLOCKSTRETCH 200000                             // Clock Stretch
#define OLEDADDR          0x3C                             // SSD1306
#define BATTERYADDR       0x36                             // MAX17048

/********************************************************** **************************************************************************/
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!You should not need to change variables and constants below this line!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Runtime changes to this software are enabled through the terminal.
// Permanent settings for the sensor and display devices can be changed in the *.h files.
/************************************************************************************************************************************/

#define SENSORID_ECG   1                 // ESPNow message sensor ID
#define SENSORID_TEMP  2                 // ESPNow message sensor ID
#define SENSORID_SOUND 3                 // ESPNow message sensor ID
#define SENSORID_OX    4                 // ESPNow message sensor ID
#define SENSORID_IMP   5                 // ESPNow message sensor ID

// ADC
/******************************************************************************************************/
// The maximum buffer size we might need is the number of samples that fit into the ESPNow payload times the maximum averaging length
// The number of samples we will read form ADC will be calculated in ADC at run time
#define BYTES_PER_SAMPLE sizeof(int16_t)  // We will work with int16 data when signals are measured
#define MAX_AVG_LEN                   32  // We want to average over up to MAX_AVG_LEN values 
#define MAX_ADC_BUFFER_SIZE ESPNOW_NUMDATABYTES/BYTES_PER_SAMPLE*MAX_AVG_LEN

// OLED
// Reset pin not used but needed for library
#define OLED_RESET -1

// Data Array Size
// ---------------
// ESPNow max payload size is 250bytes
// Data will be int16_t which is from -32,768 to 32,767
// We should allow for 10 bytes header to include sensor id and other values as well as checksum
#define ESPNOW_NUMDATABYTES 240

// ESPNow message size can not exceed 250 bytes (251 for ESP32)
// dataArray is organized like [ch1,ch2,ch3,...,ch1,ch2,ch3,... etc.]
typedef struct {
  uint8_t sensorID;                                       // Sensor number
  uint8_t numData;                                        // Number of valid integers per channel
  uint8_t numChannels;                                    // Number of channels
  int16_t dataArray[ESPNOW_NUMDATABYTES/sizeof(int16_t)]; // Array of 16-bit integers
  byte    checksum;                                       // checksum over structure excluding the checksum
} ESPNowMessage;

/******************************************************************************************************/
// Support Functions
/******************************************************************************************************/
extern unsigned long yieldOS(void);                       // returns how long yield took
extern byte calculateChecksum(const byte* data, size_t len); // computes XOR checksum
extern void printProfile();
extern void printSensors();
extern void printIntervals(); 

/******************************************************************************************************/
// Global Variables
/******************************************************************************************************/

// Character handling
extern char           tmpStr[256];                        // all character array manipulation
// Serial
extern char           serialInputBuff[64];                // serial input goes into this buffer
// Yielding
extern unsigned long  yieldTime;                          // amount of time spent yielding
extern unsigned long  lastYield;                          // keep record of time of last Yield event
// Reusable character arrays
extern const char     singleSeparator[];                  // ----
extern const char     doubleSeparator[];                  // ====
extern const char     mON[];                              // on
extern const char     mOFF[];                             // off
extern const char     clearHome[];                        // clear and go to top left
extern const char     waitmsg[];                          // message to indicate wiating
// I2C
extern TwoWire        myWire;                             // universal I2C interface
extern const int      SDA_PIN;
extern const int      SCL_PIN;
// SYS update timing
extern unsigned long  deltaUpdate;
extern unsigned long  maxUpdateSYS;
//EEPROM
extern const unsigned int eepromAddress;                  // EEPROM start address for saving the settings

#endif
