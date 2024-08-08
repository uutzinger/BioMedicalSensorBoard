/******************************************************************************************************/
// Printing
/******************************************************************************************************/
#ifndef PRINT_H_
#define PRINT_H_

#include <Arduino.h>

/******************************************************************************************************/
// DEBUG Print Configurations
/******************************************************************************************************/
#if defined DEBUG
  #define LOG_D(X)    printSerial(X)                   // debug output
  #define LOG_Dln(X)  printSerialln(X)                 // debug output
#else
  #define LOG_D(X)                                     // no debug output, as LOG_D is defined to be empty
  #define LOG_Dln(X)                                   // no debug output, ""
#endif
#define LOG_I(X)      printSerial(X)                   // regular output
#define LOG_Iln(X)    printSerialln(X)                 // regular output with /r/n appended
#define LOG_E(X)      printSerial(X)                   // regular output
#define LOG_Eln(X)    printSerialln(X)                 // regular output with /r/n appended

/******************************************************************************************************/
// Print functions
/******************************************************************************************************/
void printSerial(const char* str);
void printSerialln(const char* str);
void printSerialln();

#endif
