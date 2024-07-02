/******************************************************************************************************/
// Printing functions
/******************************************************************************************************/
#include <stdio.h>
#include "Arduino.h"

void printSerial(char* str) {
  #if USE_SERIAL
  Serial.print(str);
  #else
  ;
  #endif
}

void printSerial(String str) {
  #if USE_SERIAL
  Serial.print(str); 
  #else
  ; 
  #endif
}

void printSerialln(char* str) {
  #if USE_SERIAL
  Serial.print(str); 
  Serial.print("\r\n");
  #else
  ;
  #endif
}

void printSerialln() {
  #if USE_SERIAL
  Serial.print("\r\n");
  #else
  ;
  #endif
}

void printSerialln(String str) {
  #if USE_SERIAL
  Serial.print(str); Serial.print("\r\n");
  #else
  ;
  #endif
}
