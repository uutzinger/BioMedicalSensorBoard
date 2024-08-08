#ifndef STREAM_H_
#define STREAM_H_

#include <Arduino.h>
#include "Commons.h"

// For Serial Transmission of binary Data
//   in the data stream we need to mark the start and end of a message.
//   we should mark it with unique bit pattern that is unlikely to occur naturally
const byte startSignature[4] = {0xFF, 0x7F, 0x00, 0x80}; // Start signature, max int16 followed by min int16, which is unlikely to occur naturally
const byte stopSignature[4]  = {0x00, 0x80, 0xFF, 0x7F}; // Stop signaturee, min int16 followed by max int16, which is unlikely to occur naturally

void    streamCustomDataSerial(Buffer& buffer, uint8_t sensorID);
void    streamBinaryDataSerial(Buffer& buffer, Message& message, uint8_t sensorID);

#endif