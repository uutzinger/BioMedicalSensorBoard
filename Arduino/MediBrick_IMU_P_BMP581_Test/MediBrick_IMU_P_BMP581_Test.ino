/*******************************************************************************
 * For the MediBrick IMU Board BMP581 Pressure Sensor Test
 * @author Urs Utzinger
 * @copyright GPLv3 
 ******************************************************************************/

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
 * AT FIRST USE OF THE SENSOR
 * IT IS RECOMMENDED THAT YOU WRITE CUSTOM STRING TO NVM TO AVOID FUTURE POWER UP ISSUES
 *
 * IF SENSOR DOES NOT WANT TO POWER UP, UNPLUG and REPLUG SYSTEM FROM USB OR POWER MIGHT 
 * HELP ALSO 
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

#include <SPI.h>
#include "SparkFun_BMP581_Arduino_Library.h"
#include "logger.h"

char dataToWrite[] = "MediBr"; // 6 bytes max

// Create a new sensor object
BMP581 pressureSensor;

// SPI Settings
#define SCLK_PIN      SCK
#define MISO_PIN     MISO
#define MOSI_PIN     MOSI
#define SPI_SCLK_FREQUENCY 7000000 // 100 kHz (default) to 12 MHz
#define UNUSED_CS_PIN   9
// IMU
#define ICM_CS_PIN     11
#define ICM_INT_PIN    13
// Pressure
#define BMP_CS_PIN      6
#define BMP_INT_PIN    12

int8_t err = BMP5_OK;

// -----------------------------------------------------------------------------
void setup()
{

  // Chip Select pins
  // Set them high before setting output so that we have no transitions on the lines.
  // Do this first to make sure the sensors are not interrupted during power up
  digitalWrite(BMP_CS_PIN, HIGH); // deselect BMP chip
  digitalWrite(ICM_CS_PIN, HIGH); // deselect ICM chip
  digitalWrite(UNUSED_CS_PIN, HIGH); // deselect dummy chip
  pinMode(BMP_CS_PIN, OUTPUT);
  pinMode(ICM_CS_PIN, OUTPUT);
  pinMode(UNUSED_CS_PIN, OUTPUT);

  // Interrupt pins
  pinMode(BMP_INT_PIN, INPUT_PULLDOWN);
  pinMode(ICM_INT_PIN, INPUT_PULLDOWN);

  // Initialize the SPI library, use pin other than BMP_CS_PIN
  LOGI("Initialize SPI");
  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, UNUSED_CS_PIN);

  // Initialize sensor and its SPI connection
  // If init fails it will attempt soft reset and init again
  err = pressureSensor.beginSPI(BMP_CS_PIN, SPI_SCLK_FREQUENCY);

  // logger
  currentLogLevel = LOG_LEVEL_INFO; // NONE, ERROR, WARN, INFO, DEBUG

  // Start serial 
  Serial.begin(500000);
  while (!Serial);

  // if Sensor startup was unsuccessfull report issue and try again
  // most common errors are illustrated, 
  while( err != BMP5_OK)
  {
    if (err == BMP5_E_NULL_PTR){
      LOGE("BMP581: driver not setup properly...");
    } else if (err == BMP5_E_COM_FAIL){
      LOGE("BMP581: communicationi failure...");
    } else if (err == BMP5_E_DEV_NOT_FOUND){
      LOGE("BMP581: device not found...");
    } else if (err == BMP5_E_INVALID_CHIP_ID){
      LOGE("BMP581: invalid chip id...");
    } else if (err == BMP5_E_POWER_UP){
      LOGE("BMP581: powerup check failure, NVM RDY is false or NVM ERR is true ...");
    } else if (err == BMP5_E_POR_SOFTRESET){
      LOGE("BMP581: soft reset failure...");
    } else if (err == BMP5_E_INVALID_POWERMODE) {
      LOGE("BMP581: invalid power mote...");    
    } else if (err == BMP5_E_INVALID_THRESHOLD) {
      LOGE("BMP581: invalid threshold...");    
    } else if (err == BMP5_E_FIFO_FRAME_EMPTY) {
      LOGE("BMP581: frame empty...");
    } else if (err == BMP5_E_NVM_INVALID_ADDR) {
      LOGE("BMP581: invalid NVM address...");
    } else if (err == BMP5_E_NVM_NOT_READY) {
      LOGE("BMP581: NVM not ready ...");
    } else if (err == BMP5_E_NVM_ERROR) {
      LOGE("BMP581: NVM error ...");
    } else { 
      LOGE("BMP581: Error: %d. Unexpected other failure, check datasheet...", err);
    }
    delay(500);
    err = pressureSensor.beginSPI(BMP_CS_PIN, SPI_SCLK_FREQUENCY);
  }

  LOGI("BMP581: connected!");

}

// -----------------------------------------------------------------------------
char out_text_buffer[64];

void loop()
{
  // Get measurements from the sensor
  bmp5_sensor_data data = {0,0};
  err = pressureSensor.getSensorData(&data);

  // Check whether data was acquired successfully
  if(err == BMP5_OK)
  {
      snprintf(out_text_buffer, sizeof(out_text_buffer), "%5.1f %5.1f", data.temperature, data.pressure);
      Serial.println(out_text_buffer);
  }
  else
  {
      // Acquisition failed, most likely a communication error (code -2)
      LOGE("Error getting data from sensor! Error code: %d", err);
  }

  delay(100);
}