/******************************************************************************************************/
// Printing functions
/******************************************************************************************************/
#ifndef PRINT_H_
#define PRINT_H_

/******************************************************************************************************/
// DEBUG Print Configurations
/******************************************************************************************************/
#if defined DEBUG
  #define D_printSerial(X)            printSerial(X)                                      // debug output
  #define D_printSerialln(X)          printSerialln(X)                                    // debug output
  #define R_printSerial(X)            printSerialln(); printSerial(X)                     // regular output, with /r/n prepended
  #define R_printSerialln(X)          printSerialln(); printSerialln(X)                   // regular output, with /r/n appended and prepended
#else
  #define D_printSerial(X)                                                                // no debug output
  #define D_printSerialln(X)                                                              // no debug output
  #define R_printSerial(X)            printSerial(X)                                      // regular output
  #define R_printSerialln(X)          printSerialln(X)                                    // regular output with /r/n appended
#endif

/******************************************************************************************************/
// Print functions
/******************************************************************************************************/
void printSerial(const char* str);
void printSerialln(const char* str);
void printSerialln();

#endif
