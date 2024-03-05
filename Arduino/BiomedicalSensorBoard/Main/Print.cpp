/******************************************************************************************************/
// Printing
/******************************************************************************************************/
// These functions are used instead of Serial.print because it allows to modify the printing.
// For example you can enable a DBUG option to figure out where the program crashes but for regular
// operating this would not be included in the program.
#include "Print.h"

void printSerial(const char* str) {
  if ( str && (strlen(str) > 0) ) { Serial.print(str); }
}

void printSerialln(const char* str) {
  if ( str && (strlen(str) > 0) ) { Serial.println(str); }
}

void printSerialln() {
  Serial.println();
}
