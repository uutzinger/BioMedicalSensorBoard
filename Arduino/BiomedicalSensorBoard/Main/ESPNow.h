/******************************************************************************************************/
// ESPNow
/******************************************************************************************************/
#ifndef ESPNow_H_
#define ESPNow_H_
#include <Arduino.h>
#include <esp_now.h>                                       // ESPNow Module

/******************************************************************************************************/
// ESPNow functions
/******************************************************************************************************/
bool initializeESPNow(void);
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

/******************************************************************************************************/
// ESPNow globals
/******************************************************************************************************/
extern bool          espnow_avail;                    // ESPNow hardware is working
extern bool          ESPNowData_avail;                  
extern char          hostName[32];
extern ESPNowMessage myESPNowDataIn;
extern ESPNowMessage myESPNowDataOut;
extern esp_now_peer_info_t  peerInfo;

#endif
