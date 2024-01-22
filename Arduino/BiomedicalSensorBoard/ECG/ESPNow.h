#ifndef ESPNow_H_
#define ESPNow_H_

#include <WiFi.h>                                          // Wifi Module
#include <esp_now.h>                                       // ESPNow Module

// Data Array Size
// ---------------
// ESPNow max payload size is 250bytes
// Data will be int16_t which is from -32,768 to 32,767
// We should allow for 10 bytes header to include sensor id and other values
#define ESPNOW_NUMDATABYTES 240

// ESPNow message size can not exceed 250 bytes (251 for ESP32)
// dataArray is organized like [ch1,ch2,ch3,...,ch1,ch2,ch3,... etc.]
typedef struct {
  uint8_t sensorID;                                       // Sensor number
  uint8_t numData;                                        // Number of valid integers per channel
  uint8_t numChannels;                                    // Number of channels
  int16_t dataArray[ESPNOW_NUMDATABYTES/sizeof(int16_t)]; // Array of 16-bit integers
  byte    checksum;                                       // checksum over strucuter excluding the checksum
} ESPNowMessage;

//
bool initializeESPNow(void);
bool updateESPNow(void);

void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len);
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

#endif
