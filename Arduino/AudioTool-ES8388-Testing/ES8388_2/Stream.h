#ifndef STREAM_H_
#define STREAM_H_

#include <Arduino.h>
#include "Commons.h"

void streamCustomDataSerial(Buffer& buffer, uint8_t sensorID);
void streamBinaryDataSerial(Buffer& buffer, Message& message, uint8_t sensorID);
void clipMaxMin(Buffer& buffer);

#endif