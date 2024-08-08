#include <Arduino.h>
#include <Wire.h>
#include <logger.h>
#include <SensirionI2CSgp41.h>
#include <sensirion_gas_index_algorithm.h>

SensirionI2CSgp41 sgp41;

// Device Info
uint16_t error;
char errorMessage[256];
uint16_t serialNumber[3];
uint16_t testResult;

#define DEFAULT_COMPENSATION_RH 0x8000
#define DEFAULT_COMPENSATION_T 0x6666

// Gas Inde Algorithm
uint16_t compensation_rh = DEFAULT_COMPENSATION_RH;
uint16_t compensation_t = DEFAULT_COMPENSATION_T;
uint16_t srawVoc = 0;
uint16_t srawNox = 0;

// Gas Index Algorithm
GasIndexAlgorithmParams voc_params;
GasIndexAlgorithmParams nox_params;
int32_t vocIndex = 0; // compensated value
int32_t noxIndex = 0; // compensated value

// Time in seconds needed for NOx conditioning
// WARNING: To avoid damage to the sensing material the conditioning must not exceed 10s!
uint16_t conditioning_s = 10;

// Timings
unsigned long currentTime;
unsigned long lastSGP41Time;
const unsigned long sgp41Interval = 1000;
float sampling_interval;
#define SGP41_I2CFrequency 400000

void setup() {

  Serial.begin(115200);
  while (!Serial) {}

  Wire.begin();
  Wire.setClock(SGP41_I2CFrequency);
  sgp41.begin(Wire);


  error = sgp41.getSerialNumber(serialNumber);
  if (error) {
    errorToString(error, errorMessage, 256);
    LOGE("Error obtaining serial numbers: %s", errorMessage);
  } else {
    LOGI("Serial number: %d.%d.%d", serialNumber[0], serialNumber[1], serialNumber[2]);
  }

  error = sgp41.executeSelfTest(testResult);
  if (error) {
    errorToString(error, errorMessage, 256);
    LOGE("Error executing self test: %s", errorMessage);
  } else if (testResult != 0xD400) {
    LOGE("Error self test failed with error: %d", testResult);
  } else {
    LOGI("Passed self test");
  }

  sampling_interval = 1000./(float)(sgp41Interval);

  // initialize gas index parameters
  GasIndexAlgorithm_init_with_sampling_interval(&voc_params, GasIndexAlgorithm_ALGORITHM_TYPE_VOC, sampling_interval);
  GasIndexAlgorithm_init_with_sampling_interval(&nox_params, GasIndexAlgorithm_ALGORITHM_TYPE_NOX, sampling_interval);

}

void loop() {

  currentTime = millis();

  if (false) {
    // Read Humidity from other sensor
    // Read Temperature from other sensor
    float rh = 50.;
    float temperature =25.0;
    compensation_rh = (uint16_t)rh * 65535 / 100;
    compensation_t = (uint16_t)(temperature + 45) * 65535 / 175;
  }

  if ((currentTime - lastSGP41Time) >= sgp41Interval) {
    lastSGP41Time = currentTime;

    if (conditioning_s > 0) {
      // During NOx conditioning (10s) SRAW NOx will remain 0
      Wire.setClock(SGP41_I2CFrequency);
      error = sgp41.executeConditioning(compensation_rh, compensation_t, srawVoc);
      conditioning_s--;
      LOGI("Conditioning: %d", conditioning_s);
    } else {
      // Read Measurement
      // For temperature and humidity compesnation you can provide external measured temp and rel hum
      // If you have relative humidity values: defaultRh = %RH * 65535 / 100
      // If you have Temperature values:       defaultRh = degC + 45) * 65535 / 175

      Wire.setClock(SGP41_I2CFrequency);
      error = sgp41.measureRawSignals(compensation_rh, compensation_t, srawVoc, srawNox);
      if (error) {
        errorToString(error, errorMessage, 256);
        LOGE("Error obtaining raw signals: %s", errorMessage);
      } else {

        GasIndexAlgorithm_process(&voc_params, srawVoc, &vocIndex);
        GasIndexAlgorithm_process(&nox_params, srawNox, &noxIndex);

        Serial.print("SRAW_VOC:");
        Serial.print(srawVoc);
        Serial.print("\t");
        Serial.print("SRAW_NOx:");
        Serial.print(srawNox);
        Serial.print("\t");
        Serial.print("VOC Index (100 avg): ");
        Serial.print(vocIndex);
        Serial.print("\t");
        Serial.print("NOx Index (1 avg) :");
        Serial.println(noxIndex);
      }
    }

  }
  // low power mode
  //sgp41.turnHeaterOff();
}
