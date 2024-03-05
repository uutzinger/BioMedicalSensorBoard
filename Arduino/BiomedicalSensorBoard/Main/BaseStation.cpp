/******************************************************************************************************/
// Base Station
/******************************************************************************************************/
#include "CommonDefs.h"
#include "BaseStation.h"
#include "Print.h"
#include "Config.h"
#include "ESPNow.h"

// For Serial Transmission of Data:
const byte startSignature[2] = {0xFF, 0xFD};                  // Start signature
const byte stopSignature[2]  = {0xFD, 0xFF};                  // Stop signature

bool updateBaseStation() {
  bool ok = false;
  LOG_Dln("D:U:BASE:..");

  if (ESPNowData_avail) {                                      // Did we receive an ESPNow message?
    ESPNowData_avail = false;                                  // reset the flag
    // extract from the ESPNow message the data we need
    uint8_t sensorID     = myESPNowDataIn.sensorID;            // ECG,Temp,Ox,Sound,etc..
    uint8_t numData      = myESPNowDataIn.numData;             // data points per channel
    uint8_t numChannels  = myESPNowDataIn.numChannels;         // number of channels
    uint8_t checksum     = myESPNowDataIn.checksum;            // checksum
    //check if checksums match
    myESPNowDataIn.checksum = 0;                               // checksum was calculated with checksum field set to zero
    const byte* bytePointer = reinterpret_cast<const byte*>(&myESPNowDataIn);
    uint8_t checksum_received = calculateChecksum(bytePointer, sizeof(myESPNowDataIn));
    if (checksum != checksum_received){
      snprintf(tmpStr, sizeof(tmpStr), "Checksums don't match %u %u", checksum, checksum_received );
      if (mySettings.debuglevel > 0) { LOG_Iln(tmpStr); }
    } else {
      if (!mySettings.useBinary) {                             //<<<<<<<<<<<<<<<<<<<<------------ report text data
        // Send the data as text over serial interface
        // Format is: SensorID, channel0, channel1, channel2, ... NewLine
        for (int i = 0; i < numData; i++ )  {                  // number of data points per channel
          int pos = snprintf(tmpStr, sizeof(tmpStr), "%u", sensorID);
          for (int j = 0; j < numChannels; j++ )  {            // number of channels
            int dataIndex = i * numChannels + j;               // data point in the array
            pos += snprintf(tmpStr + pos, sizeof(tmpStr) - pos, ",%d", myESPNowDataIn.dataArray[dataIndex]); 
            if (pos >= sizeof(tmpStr)) {                       // break if tmpStr is full and dont print remainder of data
              if (mySettings.debuglevel > 0) { LOG_Iln("Error: tmpStr is too small to send the received data to serial"); }
              break;
            }
          }
          LOG_Iln(tmpStr); 
          yieldTime += yieldOS();
        }
      } else {                                                 // <<<<<<<<<<<<<<<<<<<<------------ Report binary data
        // Transmit the data in binary format over the serial port
        // First a start signature is sent
        // Then the data structure is sent byte by byte
        // At the end we send end signature
        Serial.write(startSignature, sizeof(startSignature));  // Start signature
        const byte* bytePointer = reinterpret_cast<const byte*>(&myESPNowDataIn);
        Serial.write(bytePointer, sizeof(myESPNowDataIn));     // Message data
        Serial.write(stopSignature, sizeof(stopSignature));    // Stop signature
        yieldTime += yieldOS();
      } // end binary transmission
    } // end checksums matched

    ok = true;
  }

  return ok;
} 
