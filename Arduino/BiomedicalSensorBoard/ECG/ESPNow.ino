/******************************************************************************************************/
// ESPNow
/******************************************************************************************************/

#include "ESPNow.h"
#include "Print.h"
#include "ECG.h"
#include "Config.h"

char          hostName[32] = {0};                          // esp32-02:02:02:02:02:02
bool          wifi_avail = false;                          // do we have wifi?
bool          espnow_avail = false;                        // do we have espnow?
bool          ESPNowData_avail = false;                    // data received

esp_now_peer_info_t  peerInfo;

extern bool   buffer_full;                                 // did ADC complete and fill up the output buffer?
extern uint16_t avg_samples;
extern Settings mySettings;

// Create a struct_message called myESPNowData
ESPNowMessage myESPNowDataOut;
ESPNowMessage myESPNowDataIn;

/******************************************************************************************************/
// Data send an receive
/******************************************************************************************************/
// These are interrupt service routines handeled by the ESP subsystem

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (mySettings.debuglevel > 0) { 
      R_printSerial("ESPNow: Last Packet Send Status: ");
      if (status == ESP_NOW_SEND_SUCCESS) {
        if (mySettings.debuglevel > 1) { R_printSerialln("ESPNow data delivery successful"); }
      } 
      else {
        if (mySettings.debuglevel > 1) { R_printSerialln("ESPNOW data delivery failed");  }
      }
    }
}

// Callback when data is received
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&myESPNowDataIn, incomingData, sizeof(myESPNowDataIn));
  ESPNowData_avail = true;
  if (mySettings.debuglevel > 1) { R_printSerialln("ESPNow Data received"); }
}

/******************************************************************************************************/
// Initialize ESPNow
/******************************************************************************************************/

bool initializeESPNow() {
  bool ok = false;
  uint8_t mac[6]; 
  espnow_avail = false;
  D_printSerial("D:U:ESPNOW:IN..");

  if (WiFi.status() == WL_NO_SHIELD) {
    strlcpy(hostName, "esp32", sizeof(hostName));
    wifi_avail = false;
    if (mySettings.debuglevel > 0) { R_printSerialln("WiFi: is not available"); }
  } 
  
  else {
    wifi_avail = true;
    WiFi.macAddress(mac);
    if (mySettings.debuglevel > 1) { 
      R_printSerialln("WiFi: is available");
      snprintf(tmpStr, sizeof(tmpStr), "WiFi: MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]); 
      R_printSerialln(tmpStr); 
    }
    snprintf(hostName, sizeof(hostName), "esp32-%02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]); // create unique host name
    WiFi.hostname(hostName);
    WiFi.mode(WIFI_STA);
    if (mySettings.debuglevel > 1) { 
      snprintf(tmpStr, sizeof(tmpStr), "WiFi: hostname is %s", hostName); 
      R_printSerialln(tmpStr); 
      R_printSerialln("WiFi: mode set to client"); 
    }

    if (esp_now_init() != ESP_OK) {
      if (mySettings.debuglevel > 0) { R_printSerialln("ESPNow: error initializing ESPNow"); }
    } 
    
    else {
     // Register for send and receive callbacks
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);
      // Register peer
      memcpy(peerInfo.peer_addr, mySettings.broadcastAddress, 6);
      peerInfo.channel = uint8_t(mySettings.ESPNowChannel);  
      peerInfo.encrypt = (mySettings.ESPNowEncrypt == 0) ? false: true;      
      // Add peer        
      if (esp_now_add_peer(&peerInfo) != ESP_OK){
        if (mySettings.debuglevel > 0) { R_printSerialln("ESPNow: failed to add peer"); }
      } else {
        if (mySettings.debuglevel > 1) { R_printSerialln("ESPNow: initialized"); }
        espnow_avail = true;
        ok = true;
      }
    }

    delay(50); lastYield = millis();
    return ok;
  }
} // init espnow

bool updateESPNow() {
  bool ok = false; 

  // sending data
  // ------------
  if (buffer_full) { // When ADC sets buffer full signal we will send it with ESPNow
    buffer_full = false;
    // populate the ESPNow payload
    myESPNowDataOut.sensorID = (uint8_t) SENSORID;                                 // ID for sensor, e.g. ECG, Sound, Temp etc.
    myESPNowDataOut.numData = (uint8_t) avg_samples_per_channel;                   // Number of samples per channel, is computed after every ADC read update
    myESPNowDataOut.numChannels = (uint8_t) mySettings.channels;                   // Number of channels
    memcpy(myESPNowDataOut.dataArray, buffer_out, avg_samples_per_channel * mySettings.channels * BYTES_PER_SAMPLE); // copy the data from the ADC buffer to ESPNow structure
    myESPNowDataOut.checksum = 0;                                                  // We want to exclude the checksum field when caclulating the checksum 
    const byte* bytePointer = reinterpret_cast<const byte*>(&myESPNowDataOut);
    myESPNowDataOut.checksum = calculateChecksum(bytePointer, sizeof(myESPNowDataOut));
    if (esp_now_send(mySettings.broadcastAddress, (uint8_t *)&myESPNowDataOut, sizeof(myESPNowDataOut)) != ESP_OK) { // send the data
        if (mySettings.debuglevel > 0) { R_printSerialln("ESPNow: Error sending data"); }
    } 
    else {
        if (mySettings.debuglevel > 1) { R_printSerialln("ESPNow: data sent"); }
        ok = true;
    }
  }

  // receiving data
  // ------------
  
  // This is handeled in OnDataRecv and the main program (ECG.ino) that reads ESPNowData_avail and if true decodes the data

  return ok;
}
