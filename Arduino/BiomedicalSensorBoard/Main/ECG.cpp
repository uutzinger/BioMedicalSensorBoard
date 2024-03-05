/******************************************************************************************************/
// ECG
/******************************************************************************************************/
// This is not fully implemented
// Make sure OLED is update infrequently
#include "CommonDefs.h"
#include "ECG.h"
#include "Print.h"
#include "Config.h"
#include "ESPNow.h"
#include "ADC.h"
#include "Battery.h"

bool ecg_avail;                           // ECG is working
int stateLOMinus;                         // are the electrodes attached
int stateLOPlus;                          // are the electrodes attached

bool initializeECG() {
  LOG_D("D:I:ECG..");

  // Input Output Pins
  pinMode(SDN, OUTPUT);                                                   // ECG enable
  pinMode(LO_MINUS, INPUT);                                               // Electrode disconnected
  pinMode(LO_PLUS, INPUT);                                                // Electrode disconnected
  delay(50);
  // Enable ECG
  digitalWrite(SDN, HIGH);

  if (mySettings.debuglevel > 1) { LOG_Iln("ECG: initialized"); }

  return true;
}

bool updateECG() {
  bool ok = false;
  LOG_Dln("D:U:ECG:..");

  // Check if electrodes are connected and system is charging
  stateLOMinus = gpio_get_level((gpio_num_t)LO_MINUS);
  stateLOPlus  = gpio_get_level((gpio_num_t)LO_PLUS);
  if (((stateLOMinus > 0) || (stateLOPlus > 0)) && (batteryChargerate > 0.0)) {
    snprintf(tmpStr, sizeof(tmpStr), "DANGER: Electrodes minus %s, plus %s and charging at %f", (stateLOMinus > 0) ? "connected" : "disconnected",  (stateLOPlus > 0) ? "connected" : "disconnected", batteryChargerate); 
    if (mySettings.debuglevel > 0) { LOG_Iln(tmpStr); }
  }

  if (buffer_full) { // When ADC sets buffer full signal we will send it with ESPNow
    buffer_full = false;                                                    // clear the buffer flag  
    // populate the ESPNow payload
    myESPNowDataOut.sensorID = (uint8_t) SENSORID_ECG;                      // ID for sensor, e.g. ECG, Sound, Temp etc.
    myESPNowDataOut.numData = (uint8_t) avg_samples_per_channel;            // Number of samples per channel, is computed after every ADC read update
    myESPNowDataOut.numChannels = 1;                                        // Number of channels
    memcpy(myESPNowDataOut.dataArray, buffer_out, avg_samples_per_channel * mySettings.channels * BYTES_PER_SAMPLE); // copy the data from the ADC buffer to ESPNow structure
    myESPNowDataOut.checksum = 0;                                           // We want to exclude the checksum field when calculating the checksum 
    const byte* bytePointer = reinterpret_cast<const byte*>(&myESPNowDataOut);
    myESPNowDataOut.checksum = calculateChecksum(bytePointer, sizeof(myESPNowDataOut)); // fill in the checksum
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
