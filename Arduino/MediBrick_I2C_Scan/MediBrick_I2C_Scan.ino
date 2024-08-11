/**
 * MediBrick2000 Airquality i2c scan 
 * Urs Utzinger, 2024
 **/

// I2C device found at address 0x36 // battery monitor MAX17048, LC is at 0x0B
// I2C device found at address 0x44 // ASHT45
// I2C device found at address 0x47 // BMP581
// I2C device found at address 0x59 // SGP41
// I2C device found at address 0x62 // SCD41
// I2C device found at address 0x69 // SEN5X
// I2C device found at address 0x7E // is reserved address, not sure what this is

#include "Wire.h"

// I2C speed
#define I2C_FAST           400000                          // Fast   400,000
#define I2C_REGULAR        100000                          // Normal 100,000
#define I2C_SLOW            50000                          // Slow    50,000

// I2C clock stretch limit
#define I2C_DEFAULTSTRETCH     50                          // 50ms , typical is 230 micro seconds
#define I2C_LONGSTRETCH       200                          // 200ms

// Define I2C pins for ESP32-S3 Feather if not predefined
#ifndef SDA
#define SDA 3
#endif

#ifndef SCL
  #define SCL 4
#endif

TwoWire myWire = TwoWire(0); // Create a TwoWire instance

void setup() {
  Serial.begin(115200); // Corrected baud rate for Serial communication
  delay(2000);
  myWire.begin(SDA, SCL);                          // SDA, SCL
  myWire.setClock(I2C_REGULAR);                           // 100kHz or 400kHz speed, we need to use slowest of all sensors
  myWire.setTimeOut(I2C_DEFAULTSTRETCH);        // We need to use largest of all sensors
  Serial.println("Setup complete for I2C scan");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  delay(5000);

  Serial.println("Scanning for I2C devices ...");
  for (address = 0x01; address < 0x7F; address++) {
    myWire.beginTransmission(address);
    error = myWire.endTransmission();
    if (error == 0) {
      if (address ==  0x44){
        Serial.println("MAX17048 Battery Monitor found");
      } else if (address ==  0x36) {
        Serial.println("MAX17043 Battery Monitor found");
      } else if (address ==  0x47) {
        Serial.println("BMP581 pressure sensor found");
      } else if (address ==  0x59) {
        Serial.println("SGP41 eVOC eNOx sensor found");
      } else if (address ==  0x62) {
        Serial.println("SCD41 CO2 sensor found");
      } else if (address ==  0x69) {
        Serial.println("SEN5X particulate matter sensor found");
      } else if (address ==  0x44) {
        Serial.println("SHT45 humidity and temperature sensor found");
      } else if (address ==  0x10) {
        Serial.println("ES8388 sound codec found");
      } else {
        Serial.printf("I2C device found at address 0x%02X\n", address);
      }
      nDevices++;
    } else if (error != 2) {
      Serial.printf("Error %d at address 0x%02X\n", error, address);
    }
    delay(5);
  }

  if (nDevices == 0) {
    Serial.println("No I2C devices found");
  }

}
