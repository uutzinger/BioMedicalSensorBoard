/*******************************************************************************
 * For the MediBrick IMU Board BMP581 Pressure Sensor Test
 * @author Urs Utzinger
 * @copyright GPLv3 
 ******************************************************************************/

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
#define SPI_SCLK_FREQUENCY 100000 // 100 kHz (default) to 12 MHz
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
  Serial.println("Initialize SPI");
  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, UNUSED_CS_PIN);
  delay(200); // from examples in bmp5_api

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
  // on Arduino IDE with SPI I have have issues with non volatile memory check (BMP_E_POWER_UP) which then might lead to reset error (POR_SOFTRESET)
  while( err != BMP5_OK)
  {
    if (err == BMP5_E_NULL_PTR){
      Serial.println("BMP581: BMP581 driver not setup properly...");
    } else if (err == BMP5_E_COM_FAIL){
      Serial.println("BMP581: BMP581 communicationi failure...");
    } else if (err == BMP5_E_DEV_NOT_FOUND){
      Serial.println("BMP581: BMP581 device not found...");
    } else if (err == BMP5_E_INVALID_CHIP_ID){
      Serial.println("BMP581: BMP581 invalid chip id...");
    } else if (err == BMP5_E_POWER_UP){
      Serial.println("BMP581: Powerup check failure, NVM RDY is false or NVM ERR is true ...");
    } else if (err == BMP5_E_POR_SOFTRESET){
      Serial.println("BMP581: Soft reset failure...");
    } else { 
      Serial.print("BMP581: Error: ");
      Serial.print(err);
      Serial.println("other failure, check source code...");
    }
    delay(500);
    err = pressureSensor.beginSPI(BMP_CS_PIN, SPI_SCLK_FREQUENCY);
  }

// #define BMP5_OK                                   INT8_C(0)
// #define BMP5_E_NULL_PTR                           INT8_C(-1)
// #define BMP5_E_COM_FAIL                           INT8_C(-2)
// #define BMP5_E_DEV_NOT_FOUND                      INT8_C(-3)
// #define BMP5_E_INVALID_CHIP_ID                    INT8_C(-4)
// #define BMP5_E_POWER_UP                           INT8_C(-5)
// #define BMP5_E_POR_SOFTRESET                      INT8_C(-6)
// #define BMP5_E_INVALID_POWERMODE                  INT8_C(-7)
// #define BMP5_E_INVALID_THRESHOLD                  INT8_C(-8)
// #define BMP5_E_FIFO_FRAME_EMPTY                   INT8_C(-9)
// #define BMP5_E_NVM_INVALID_ADDR                   INT8_C(-10)
// #define BMP5_E_NVM_NOT_READY                      INT8_C(-11)

  Serial.println("BMP581: connected!");

  uint16_t dataIndex = 0;
  for(uint8_t addr = BMP5_NVM_START_ADDR; addr <= BMP5_NVM_END_ADDR; addr++)
  {
      uint16_t data = dataToWrite[dataIndex] | (dataToWrite[dataIndex+1] << 8);
      dataIndex += 2;

      pressureSensor.writeNVM(addr, data);
  }
    for(uint8_t addr = BMP5_NVM_START_ADDR; addr <= BMP5_NVM_END_ADDR; addr++)
  {
      uint16_t data = 0;
      pressureSensor.readNVM(addr, &data);
      char first = data & 0xFF;
      char second = (data >> 8) & 0xFF;
      Serial.print(first);
      Serial.print(second);
  }

}


void loop()
{
  delay(100);
}