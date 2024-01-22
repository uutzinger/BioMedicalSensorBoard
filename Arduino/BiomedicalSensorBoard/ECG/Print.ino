/******************************************************************************************************/
// Printing functions
/******************************************************************************************************/
// These functions are used instead of Serial.print because it allows to modify the printing.
// For example you can enable a DBUG option to figure out where the program crashes but for regular
// operating this would not be included in the program.

#include "Print.h"
#include "Config.h"

extern Settings mySettings;

/******************************************************************************************************/
// Printing to Serial
// Print with /r/n appended
// Just print /r/n
// Print strings and char*
/******************************************************************************************************/

void printSerial(const char* str) {
  if ( mySettings.useSerial && str && (strlen(str) > 0) ) { Serial.print(str); }
}

void printSerialln(const char* str) {
  if ( mySettings.useSerial && str && (strlen(str) > 0) ) { Serial.print(str); Serial.print("\r\n"); }
}

void printSerialln() {
  if ( mySettings.useSerial ) { Serial.print("\r\n"); }
}
