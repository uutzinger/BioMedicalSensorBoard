#include <Arduino.h>
#include <SensirionI2CScd4x.h>
#include <Wire.h>
#include <logger.h>

SensirionI2CScd4x scd4x;

uint16_t error;
char errorMessage[256];
uint16_t serial0;
uint16_t serial1;
uint16_t serial2;

// Measurement
uint16_t co2 = 0;
float temperature = 0.0f;
float humidity = 0.0f;
bool isDataReady = false;

// Timings
unsigned long currentTime;
unsigned long lastSCD4XTime;
const unsigned long scd4XInterval = 1000;
#define I2CFrequency 100000

void setup() {

    Serial.begin(115200);
    while (!Serial) {}

    Wire.begin();
    Wire.setClock(I2CFrequency);
    scd4x.begin(Wire);

    // stop potentially previously started measurement
    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        errorToString(error, errorMessage, 256);
        LOGE("Error stopping periodic measurements: %s", errorMessage);
    }

    error = scd4x.getSerialNumber(serial0, serial1, serial2);
    if (error) {
        errorToString(error, errorMessage, 256);
        LOGE("Error obtaining serial numbers: %s", errorMessage);
    } else {
      LOGI("Serial number: %d.%d.%d", serial0, serial1, serial2);
    }

    // Start Measurement
    error = scd4x.startPeriodicMeasurement();
    if (error) {
        errorToString(error, errorMessage, 256);
        LOGE("Error trying starting periodic measurement: %s", errorMessage);
    }

    lastSCD4XTime = 0;


}

void loop() {

    currentTime = millis();

    if ((currentTime - lastSCD4XTime) >= scd4XInterval) {
      lastSCD4XTime = currentTime;

      error = scd4x.getDataReadyFlag(isDataReady);
      if (error) {
          errorToString(error, errorMessage, 256);
          LOGE("Error checking data ready: %s", errorMessage);
          return;
      }
      if (!isDataReady) {
          return;
      }
      error = scd4x.readMeasurement(co2, temperature, humidity);
      if (error) {
          errorToString(error, errorMessage, 256);
          LOGE("Error reading data: %s", errorMessage);
      } else if (co2 == 0) {
          LOGE("Invalid sample detected, skipping.")
      } else {
          Serial.print("Co2:");         Serial.print(co2);         Serial.print("\t");
          Serial.print("Temperature:"); Serial.print(temperature); Serial.print("\t");
          Serial.print("Humidity:");    Serial.print(humidity);
          Serial.println();
      }
    }
}
