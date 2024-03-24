/******************************************************************************************************/
// Stream Functions
/******************************************************************************************************/
#include "Process.h"
#include "Commons.h"
#include "myPrint.h"

void streamCustomDataSerial(Buffer& buffer, uint8_t sensorID) {
// =======================================================================================
// Custom text streaming
// That streamed to serial is custom formatted to match custom data display software.
// If serial buffer is full, and we would want to add data to serial output, the serial
// writer would block execution until space is available in the serial driver.
// Here we check if there is space available in the serial buffer.
// =======================================================================================

  // Format is: SensorID, channel0, channel1, channel2, ... NewLine
  const size_t tmpStrSize = sizeof(tmpStr);
  uint16_t total_iterations = buffer.bytes_recorded / buffer.bytes_per_sample / buffer.num_chan;
  
  if ( (buffer.bytes_per_sample<=4) && (buffer.bytes_per_sample > 0) ) {

    for(uint16_t m = 0; m < total_iterations; m++) {
      int pos = snprintf(tmpStr, tmpStrSize, "%u", sensorID);
      for(uint16_t n = 0; n < buffer.num_chan; n++) {
        int32_t value = 0;
        size_t index = (m * buffer.num_chan + n) * buffer.bytes_per_sample;
        if (buffer.bytes_per_sample == 1){
          value = buffer.data[index];
        } else if (buffer.bytes_per_sample == 2) {
          // Assuming little-endian byte order
          value = buffer.data[index] | (buffer.data[index + 1] << 8);
        } else if (buffer.bytes_per_sample == 3) {
          // Manually combine three bytes, assuming little-endian byte order
          value = buffer.data[index] | (buffer.data[index + 1] << 8) | (buffer.data[index + 2] << 16);        
        } else if (buffer.bytes_per_sample == 4) {
          // Manually combine three bytes, assuming little-endian byte order
          value = buffer.data[index] | (buffer.data[index + 1] << 8) | (buffer.data[index + 2] << 16) | (buffer.data[index + 3] << 24);              
        }       
        size_t remaining = tmpStrSize - pos;
        pos += snprintf(tmpStr + pos, remaining, ",%d", value); 
        if (pos < 0) {
            // Handle encoding error or other problems indicated by negative return value
            break;
        } else if (pos >= tmpStrSize) {
            // Output was truncated, ensure null termination at the end of the buffer
            tmpStr[tmpStrSize - 1] = '\0'; // Manually ensure null-termination
            break;
        }
      }
      if (Serial.availableForWrite() < (strlen(tmpStr) + 2)) { // +2 for newline characters
        LOG_Eln("Serial buffer to small, program was blocked.");
      }
      Serial.println(tmpStr); // stream the data
    }
  
  } else { LOG_Eln("Bytes per sample needs to be 1,2,3 or 4"); } 

}

void streamBinaryDataSerial(Buffer& buffer, Message& message, uint8_t sensorID) {
// =======================================================================================
// Transmit the data in binary format over the serial port
// First a start signature is sent
// Then the data structure is sent byte by byte
// At the end we send the end signature
// Buffer data will be reformatted to match the datamessage structure
// Buffer overflow is not yet handeled.
// =======================================================================================

  const byte* bytePointer;
  // For Serial Transmission of binary Data
  //   in the data stream we need to mark the start and end of a message.
  //   we should mark it with unique bit pattern that is unlikely to occur naturally
  const byte startSignature[4] = {0xFF, 0xFF, 0xFF, 0x7F}; // Start signature, max int32_t
  const byte stopSignature[4]  = {0x00, 0x00, 0x00, 0x80}; // Stop signaturee, min int32_t

  // Setup message
  message.sensorID = sensorID;                                            // ID for sensor, e.g. ECG, Sound, Temp etc.
  message.numData = (uint8_t) buffer.bytes_recorded/buffer.bytes_per_sample/buffer.num_chan;        // Number of samples per channel, is computed after every ADC read update
  message.numChannels = (uint8_t) buffer.num_chan;                        // Number of channels

  // Exclue max int32_t and min int32_t from data
  // This is to make sure we do not have start or stop signature in data:
  //   startSignature {0xFF, 0xFF, 0xFF, 0x7F};
  //   stopSignature {0x00, 0x00, 0x00, 0x80}
  clipMaxMin(buffer);
  
  // Copy data
  if (buffer.bytes_recorded <= sizeof(message.data)) {
    // it will fit
    memcpy(message.data, buffer.data, buffer.bytes_recorded); // copy the data from the buffer to message structure
  } else {
    //it will not fit
    memcpy(message.data, buffer.data, sizeof(message.data));              // copy partial data from the buffer to message structure
    LOG_Eln("Message buffer overrun, data truncated");
  }
  // Checksum
  message.checksum = 0;                                                    // We want to exclude the checksum field when calculating the checksum 
  bytePointer = reinterpret_cast<const byte*>(&message);                   //
  message.checksum = calculateChecksum(bytePointer, sizeof(message));      // fill in the checksum

  // Transmit
  Serial.write(startSignature, sizeof(startSignature));                    // Start signature
  if (Serial.availableForWrite() < sizeof(message)) { 
    LOG_Eln("Serial print: buffer too small, program blocked."); 
  }
  bytePointer = reinterpret_cast<const byte*>(&message);                   //
  Serial.write(bytePointer, sizeof(message));                              // Message data
  Serial.write(stopSignature, sizeof(stopSignature));                      // Stop signature
}
