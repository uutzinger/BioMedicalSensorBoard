/******************************************************************************************************/
// Sound
/******************************************************************************************************/
#include "CommonDefs.h"
#include "Sound.h"
#include "Print.h"
#include "Config.h"
#include "ESPNow.h"
#include "ADC.h"

bool updateSound() {
  bool ok = false;
  LOG_Dln("D:U:SOUND:..");
 
  if (buffer_full) { // When ADC sets buffer full signal we will send it with ESPNow
    buffer_full = false;                                                           // reset buffer flag
    // populate the ESPNow payload
    myESPNowDataOut.sensorID = (uint8_t) SENSORID_SOUND;                           // ID for sensor, e.g. ECG, Sound, Temp etc.
    myESPNowDataOut.numData = (uint8_t) avg_samples_per_channel;                   // Number of samples per channel, is computed after every ADC read update
    myESPNowDataOut.numChannels = 1;                                               // Number of channels
    memcpy(myESPNowDataOut.dataArray, buffer_out, avg_samples_per_channel * mySettings.channels * BYTES_PER_SAMPLE); // copy the data from the ADC buffer to ESPNow structure
    myESPNowDataOut.checksum = 0;                                                  // We want to exclude the checksum field when caclulating the checksum 
    const byte* bytePointer = reinterpret_cast<const byte*>(&myESPNowDataOut);
    myESPNowDataOut.checksum = calculateChecksum(bytePointer, sizeof(myESPNowDataOut));
    if (esp_now_send(mySettings.broadcastAddress, (uint8_t *)&myESPNowDataOut, sizeof(myESPNowDataOut)) != ESP_OK) { // send the data
        if (mySettings.debuglevel > 0) { LOG_Iln("ESPNow: Error sending data"); }
    } 
    else {
        if (mySettings.debuglevel > 1) { LOG_Iln("ESPNow: data sent"); }
        ok = true;
    }
  }

  return ok;
} 
