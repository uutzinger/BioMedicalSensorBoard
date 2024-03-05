/******************************************************************************************************/
// Sensi Main Program
/******************************************************************************************************/
#ifndef ECG_H_
#define ECG_H_

#include <string.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Wire.h>
#include "esp_heap_caps.h"
// #include <ArduinoJson.h>

/******************************************************************************************************/
// Support Functions
/******************************************************************************************************/
unsigned long yieldOS(void);                                    // returns how long yield took
bool checkI2C(uint8_t address, TwoWire *myWire);                // is device attached at this address?
void serialTrigger(const char* mess, int timeout);              // delays until user input
void helpMenu(void);                                            // lists user functions
void printSettings(void);                                       // lists program settings
void defaultSettings(void);                                     // converts settings back to default. need to save to EEPROM to retain
void printSensors(void);                                        // lists current sensor values
void printProfile();
void printIntervals();
byte calculateChecksum(const byte* data, size_t len);          // computes XOR checksum

#endif
