/******************************************************************************************************/
// Stored System settings
/******************************************************************************************************/
#ifndef CONFIG_H_
#define CONFIG_H_

#include "esp_adc/adc_continuous.h"
#include "driver/adc.h"
#include "soc/adc_channel.h"

#include <EEPROM.h>
#define EEPROM_SIZE               1024                     // make sure this value is larger than the space required by the settings below and lowwer than the max Settings of the microcontroller

// ========================================================// 
// The EEPROM section
//
// Add settings here if you want them to be loaded at boot.
// Appending new settings to the end will keeps the already stored settings.
// Boolean settings are stored as a byte.
//
// If you add large strings or many settings you might need to change EEPROM size above.

struct Settings {
  char          identifier[16];                            // 15 characters max with null terminator
  uint32_t      runTime;                                   // keep track of total sensor run time
  uint8_t       debuglevel;                                // amount of debug output on serial port
  bool          useOLED;                                   // use/not use OLED even if it is connected
  bool          useESPNow;                                 // use/not use ESPNow
  bool          useSerial;                                 // provide Serial interface on USB
  bool          useBattery;                                // provide Battery monitoring
  bool          useECG;                                    // analog in
  bool          useBinary;                                 // send binary data with messagePack
  bool          useDevice4;                                // ... not yet used
  bool          useDevice5;                                // ... not yet used
  uint8_t       broadcastAddress[6];                       // broadcast MAC address, this is the receiver for ESPNow
  uint8_t       ESPNowChannel;                             // ESPNow communication channel
  uint8_t       ESPNowEncrypt;                             // ESPNow encrypt communication yes/no
  uint32_t      baudrate;                                  // 150 to 500,000, 2,000,000 might also work
  uint32_t      samplerate;                                // 611 to 44100, 2,000,000 might also work
  uint8_t       channels;                                  // number of channels, usually 1 to 2
  uint8_t       bitwidth;                                  // 9,10,11,12 ADC bit depth, usually 12 bit
  uint8_t       atten;                                     // attenuation, adjusting analog input range
  uint8_t       avglen;                                    // downsampling by averaging the input values
  adc_channel_t channel0;                                  
  adc_channel_t channel1;
  adc_channel_t channel2;
  adc_channel_t channel3;
  adc_channel_t channel4;
  adc_channel_t channel5;
  uint8_t       OLEDDisplay;                               // Display configuration, if you have more than one layout of your OLED
};

bool inputHandle(void);
void saveConfiguration(const Settings &config);
void loadConfiguration(Settings &config);
void parseMACAddress(const char* str, uint8_t* mac);

#endif
