#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2CSen5x.h>
#include <logger.h>

SensirionI2CSen5x sen5x;

uint16_t error;
char errorMessage[256];

// Device Signature
uint8_t firmwareMajor;
uint8_t firmwareMinor;
bool    firmwareDebug;
uint8_t hardwareMajor;
uint8_t hardwareMinor;
uint8_t protocolMajor;
uint8_t protocolMinor;
uint8_t productNameSize = 32;
uint8_t serialNumberSize = 32;
float   tempOffset = 0.0;
uint32_t autoCleaningInterval;
uint32_t deviceStatus;
unsigned char productName[32];
unsigned char serialNumber[32];

// Measurements
float massConcentrationPm1p0;
float massConcentrationPm2p5;
float massConcentrationPm4p0;
float massConcentrationPm10p0;
float ambientHumidity;
float ambientTemperature;
float vocIndex;
float noxIndex;

// Timings
unsigned long currentTime;
unsigned long lastSEN5XTime;
const unsigned long sen5XInterval = 1000;

bool isBitSet(uint32_t value, int bit) {
    return (value & (1 << bit)) != 0;
}

void setup() {

  
  currentLogLevel = LOG_LEVEL_INFO; // NONE, ERROR, WARN, INFO, DEBUG
  Serial.begin(115200);
  while(!Serial){}
  // delay(4000);


  Wire.begin();
  sen5x.begin(Wire);

  error = sen5x.deviceReset();
  if (error) {
      errorToString(error, errorMessage, 256);
      LOGE("Error executing device reset: %s", errorMessage);
  }

  error = sen5x.getProductName(productName, productNameSize);
  if (error) {
      errorToString(error, errorMessage, 256);
      LOGE("Error gettng product name: %s", errorMessage);
  } else {
      LOGI("Produce name: %s", productName);
  }

  error = sen5x.getVersion(firmwareMajor, firmwareMinor, firmwareDebug,
                            hardwareMajor, hardwareMinor, protocolMajor,
                            protocolMinor);
  if (error) {
      errorToString(error, errorMessage, 256);
      LOGE("Error to obtain version: %s", errorMessage);
  } else {
      LOGI("Firmware: %d.%d, Hardware: %d.%d", firmwareMajor, firmwareMinor, hardwareMajor, hardwareMinor);
  }

  error = sen5x.getSerialNumber(serialNumber, serialNumberSize);
  if (error) {
      errorToString(error, errorMessage, 256);
      LOGE("Error to obtain serial number: %s", errorMessage);
  } else {
      LOGI("SerialNumber: %s", serialNumber);
  }

  error = sen5x.setTemperatureOffsetSimple(tempOffset);
  if (error) {
      errorToString(error, errorMessage, 256);
      LOGE("Error setting temperature offset: %s", errorMessage);
  } else {
    sen5x.getTemperatureOffsetSimple(tempOffset);
    LOGI("Temperatuer offset is: %d C", tempOffset);
  }

  error = sen5x.getFanAutoCleaningInterval(autoCleaningInterval);
  if (error) {
      errorToString(error, errorMessage, 256);
      LOGE("Error obtaining autocleaning interval: %s", errorMessage);
  } else {
    LOGI("Autocleaning interval is: %d hrs", autoCleaningInterval);
  }

  error = sen5x.readDeviceStatus(deviceStatus);
  if (error) {
      errorToString(error, errorMessage, 256);
      LOGE("Error obtaining device status: %s", errorMessage);
  } else {
    // Bit 21: Speed
    if (isBitSet( deviceStatus, 21)) { LOGI("Speed: Too high"); }           else { LOGI("Speed: OK"); }
    // Bit 19: Fan
    if (isBitSet( deviceStatus, 19)) { LOGI("Fan: Cleaning"); }             else { LOGI("Fan: Normal"); }
    // Bit 7: Gas sensor
    if (isBitSet( deviceStatus,  7)) { LOGI("Gas sensor: Error"); }         else { LOGI("Gas sensor: OK"); }
    // Bit 6: Humidity sensor
    if (isBitSet( deviceStatus,  6)) { LOGI("Humidity sensor: Error"); }    else { LOGI("Humidity sensor: OK"); }
    // Bit 5: Laser
    if (isBitSet( deviceStatus,  5)) { LOGI("Laser: Current too high"); }   else { LOGI("Laser: OK"); }
    // Bit 4: Fan
    if (isBitSet( deviceStatus,  4)) { LOGI("Fan: Mechanically blocked"); } else { LOGI("Fan: Normal"); }
  }

  error = sen5x.startMeasurement();
  if (error) {
    errorToString(error, errorMessage, 256);
    LOGE("Error starting measurement: %s", errorMessage);
  }

  lastSEN5XTime = 0;
}

void loop() {

    currentTime = millis();

    if ((currentTime - lastSEN5XTime) >= sen5XInterval) {
      lastSEN5XTime = currentTime;

      error = sen5x.readMeasuredValues(
          massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
          massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex,
          noxIndex);

      if (error) {
          errorToString(error, errorMessage, 256);
          LOGE("Error reading measurement: %s", errorMessage);
      } else {
                                                                Serial.print("PM1p0: ");  Serial.print(massConcentrationPm1p0); //[µg/m³]
                                            Serial.print("\t"); Serial.print("PM2p5: ");  Serial.print(massConcentrationPm2p5); //[µg/m³]
                                            Serial.print("\t"); Serial.print("PM4p0: ");  Serial.print(massConcentrationPm4p0); //[µg/m³]
                                            Serial.print("\t"); Serial.print("PM10p0: "); Serial.print(massConcentrationPm10p0); //[µg/m³]
          if (!isnan(ambientHumidity)) {    Serial.print("\t"); Serial.print("aH: ");     Serial.print(ambientHumidity); } 
          if (!isnan(ambientTemperature)) { Serial.print("\t"); Serial.print("aT:");      Serial.print(ambientTemperature); }
          if (!isnan(vocIndex)) {           Serial.print("\t"); Serial.print("VOC:");     Serial.print(vocIndex); }         
          if (!isnan(noxIndex)) {           Serial.print("\t"); Serial.print("NOx: ");    Serial.print(noxIndex); }
          Serial.println();
      }
    }

    uint16_t readDataReady(bool& dataReady);

  // Before going to deep sleep mode set sensor to idle
  // sen5x.stopMeasurement(void);  // 0.7mA

}
