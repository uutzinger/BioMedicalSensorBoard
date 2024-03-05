/******************************************************************************************************/
// ESPNow
/******************************************************************************************************/
#include <WiFi.h>                                          // Wifi Module
#include <esp_now.h>                                       // ESPNow Module

#include "CommonDefs.h"
#include "ESPNow.h"
#include "Print.h"
#include "CommonDefs.h"
#include "Config.h"

char hostName[32] = {0};                          // esp32-02:02:02:02:02:02
bool wifi_avail = true;                           // do we have wifi? device has wifi
bool espnow_avail = true;                         // do we have espnow? device has espnow
bool ESPNowData_avail = false;                    // no data received yet

esp_now_peer_info_t  peerInfo;

// Create a struct_message called myESPNowData
ESPNowMessage myESPNowDataOut;
ESPNowMessage myESPNowDataIn;

/******************************************************************************************************/
// Data send an receive
/******************************************************************************************************/
// These are interrupt service routines handled by the ESP subsystem

// Callback when data is sent
// Not much shall happen here as sending either succeeds or fails
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (mySettings.debuglevel > 0) { 
      LOG_I("ESPNow: Last Packet Send Status: ");
      if (status == ESP_NOW_SEND_SUCCESS) {
        if (mySettings.debuglevel > 1) { LOG_Iln("ESPNow data delivery successful"); }
      } 
      else {
        if (mySettings.debuglevel > 1) { LOG_Iln("ESPNOW data delivery failed");  }
      }
    }
}

// Callback when data is received
// This is used by base station to receive data from the sensors
// This can also be used if we want the main program to change the program settings (not implemented)
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&myESPNowDataIn, incomingData, sizeof(myESPNowDataIn));
  ESPNowData_avail = true;
  if (mySettings.debuglevel > 1) { LOG_Iln("ESPNow Data received"); }
}

/******************************************************************************************************/
// Initialize ESPNow
/******************************************************************************************************/
// Similar to WiFi initialization

bool initializeESPNow() {
  uint8_t mac[6]; 
  bool ok = false;
  LOG_Dln("D:I:ESPNOW:..");

  if (WiFi.status() == WL_NO_SHIELD) {
    strlcpy(hostName, "esp32", sizeof(hostName));
    wifi_avail = false;
    espnow_avail = false;
    if (mySettings.debuglevel > 0) { LOG_Iln("WiFi: is not available"); }
  } 
  else {
    wifi_avail = true;
    WiFi.macAddress(mac);
    if (mySettings.debuglevel > 1) { 
      LOG_Iln("WiFi: is available");
      snprintf(tmpStr, sizeof(tmpStr), "WiFi: MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]); 
      LOG_Iln(tmpStr); 
    }
    snprintf(hostName, sizeof(hostName), "esp32-%02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]); // create unique host name
    WiFi.hostname(hostName);
    WiFi.mode(WIFI_STA);
    if (mySettings.debuglevel > 1) { 
      snprintf(tmpStr, sizeof(tmpStr), "WiFi: hostname is %s", hostName); 
      LOG_Iln(tmpStr); 
      LOG_Iln("WiFi: mode set to client"); 
    }

    if (esp_now_init() != ESP_OK) {
      espnow_avail = false;
      if (mySettings.debuglevel > 0) { LOG_Iln("ESPNow: error initializing ESPNow"); }
    } 
    else {
      espnow_avail = true;
      // Register for send and receive callbacks
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);
      // Register peer
      memcpy(peerInfo.peer_addr, mySettings.broadcastAddress, 6);
      peerInfo.channel = uint8_t(mySettings.ESPNowChannel);  
      peerInfo.encrypt = (mySettings.ESPNowEncrypt == 0) ? false: true;      
      // Add peer        
      if (esp_now_add_peer(&peerInfo) != ESP_OK){
        if (mySettings.debuglevel > 0) { LOG_Iln("ESPNow: failed to add peer"); }
        // might need to turn off espnow_avail, or we try to add reconnection code
      } else {
        if (mySettings.debuglevel > 1) { LOG_Iln("ESPNow: initialized"); }
        espnow_avail = true;
        ok = true;
      }
    }

    delay(50); lastYield = millis();

    return ok;
  }
} // init espnow
