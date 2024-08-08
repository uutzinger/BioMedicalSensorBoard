/******************************************************************************************************/
// Printing
/******************************************************************************************************/
// These functions are used instead of Serial.print because it allows to modify the printing behaviour.
// For example you can enable a DBUG option for more detailed output but for regular
// operating we would omit printing debug information.
// See print.h

#include "myPrint.h"

void printSerial(const char* str) {
  if ( str && (strlen(str) > 0) ) { Serial.print(str); }
}

void printSerialln(const char* str) {
  if ( str && (strlen(str) > 0) ) { Serial.println(str); }
}

void printSerialln() {
  Serial.println();
}
