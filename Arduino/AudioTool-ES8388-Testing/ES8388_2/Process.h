#ifndef SOUND_H_
#define SOUND_H_

#include <Arduino.h>
#include "Commons.h"

byte    calculateChecksum(const byte* data, size_t len);

Buffer  initBuffer(uint16_t numBytes);
void    freeBuffer(Buffer& buf);
void    resetBuffer(Buffer& buffer);
void    clipData(Buffer& buffer, int32_t max_value, int32_t min_value);

void    processBuffer(Buffer& buffer_in, Buffer& buffer_out);
void    computeChannelDifferences(Buffer& buffer_in, Buffer& buffer_delta);

#endif