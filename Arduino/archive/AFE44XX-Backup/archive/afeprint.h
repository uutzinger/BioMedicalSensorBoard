/******************************************************************************************************/
// Printing functions
/******************************************************************************************************/
#ifndef AFEPRINT_H_
#define AFEPRINT_H_

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
  #define R_printSerialLog(X)         printSerialLog(X)                                   // regular output
#endif

void printSerial(char* str);
void printSerial(String str);
void printSerialln(char* str);
void printSerialln(String str);
void printSerialln();

#endif