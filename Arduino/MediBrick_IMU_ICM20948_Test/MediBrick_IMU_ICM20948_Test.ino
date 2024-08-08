/****************************************************************
 * ICM 20948 Read sensor data and print scaled values
 * Use the default configuration to stream 9-axis IMU data
 * Owen Lyke @ SparkFun Electronics
 * Original Creation Date: April 17 2019
 *
 * Please see License.md for the license information.
 *
 * Distributed as-is; no warranty is given.
 ***************************************************************/
#include "ICM_20948.h"
#include <Wire.h>

#define SERIAL_PORT Serial    // Your desired seriak port
#define SPI_PORT SPI          // Your desired SPI port

// SPI Settings
#define SCLK_PIN  SCK
#define MISO_PIN MISO
#define MOSI_PIN MOSI
#define SPI_SCLK_FREQUENCY 1000000 // 100 to 1000kHz
// IMU
#define ICM_CS     11
#define ICM_INT    13
// Pressure
#define BMP_CS     6
#define BMP_INT    12

ICM_20948_SPI myICM;          // the ICM object

// -----------------------------------------------------------------------------

void setup()
{

  pinMode(BMP_CS, OUTPUT);
  pinMode(ICM_CS, OUTPUT);
  pinMode(BMP_INT, INPUT_PULLDOWN); // need to configure interrupt register accordingly
  pinMode(ICM_INT, INPUT_PULLDOWN); // need to fongigure interrupt register accordingly
  digitalWrite(BMP_CS, HIGH); // deselect BMP before startup
  digitalWrite(ICM_CS, HIGH); // deselect ICM before startup

  SERIAL_PORT.begin(500000);
  while (!SERIAL_PORT);

  Serial.println(F("ICM20948 Test"));

 // Initialize the SPI port
  SPI_PORT.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, ICM_CS);

  myICM.enableDebugging(SERIAL_PORT);

  bool initialized = false;
  while (!initialized)
  {
    myICM.begin(ICM_CS, SPI_PORT, SPI_SCLK_FREQUENCY);

    SERIAL_PORT.print(F("Initialization of the sensor returned: "));
    SERIAL_PORT.println(myICM.statusString());
    if (myICM.status != ICM_20948_Stat_Ok) {
      SERIAL_PORT.println("Trying again...");
      delay(500);
    } else {
      initialized = true;
    }
  }
}

// -----------------------------------------------------------------------------

void loop() {

  if (myICM.dataReady()) {
    myICM.getAGMT();             // The values are only updated when you call 'getAGMT'
    // printRawAGMT( myICM.agmt );  // Uncomment this to see the raw values, taken directly from the agmt structure
    printScaledAGMT(&myICM);     // This function takes into account the scale settings from when the measurement was made to calculate the values with units
    delay(30);
  } else {
    SERIAL_PORT.println("Waiting for data");
    delay(500);
  }
}

// -----------------------------------------------------------------------------

void printPaddedInt16b(int16_t val)
{
  if (val > 0) {
    SERIAL_PORT.print(" ");
    if (val < 10000) {
      SERIAL_PORT.print("0");
    }
    if (val < 1000) {
      SERIAL_PORT.print("0");
    }
    if (val < 100) {
      SERIAL_PORT.print("0");
    }
    if (val < 10) {
      SERIAL_PORT.print("0");
    }
  } else {
    SERIAL_PORT.print("-");
    if (abs(val) < 10000) {
      SERIAL_PORT.print("0");
    }
    if (abs(val) < 1000) {
      SERIAL_PORT.print("0");
    }
    if (abs(val) < 100) {
      SERIAL_PORT.print("0");
    }
    if (abs(val) < 10) {
      SERIAL_PORT.print("0");
    }
  }
  SERIAL_PORT.print(abs(val));
}

void printRawAGMT(ICM_20948_AGMT_t agmt)
{
  SERIAL_PORT.print("RAW. Acc [ ");
  printPaddedInt16b(agmt.acc.axes.x);
  SERIAL_PORT.print(", ");
  printPaddedInt16b(agmt.acc.axes.y);
  SERIAL_PORT.print(", ");
  printPaddedInt16b(agmt.acc.axes.z);
  SERIAL_PORT.print(" ], Gyr [ ");
  printPaddedInt16b(agmt.gyr.axes.x);
  SERIAL_PORT.print(", ");
  printPaddedInt16b(agmt.gyr.axes.y);
  SERIAL_PORT.print(", ");
  printPaddedInt16b(agmt.gyr.axes.z);
  SERIAL_PORT.print(" ], Mag [ ");
  printPaddedInt16b(agmt.mag.axes.x);
  SERIAL_PORT.print(", ");
  printPaddedInt16b(agmt.mag.axes.y);
  SERIAL_PORT.print(", ");
  printPaddedInt16b(agmt.mag.axes.z);
  SERIAL_PORT.print(" ], Tmp [ ");
  printPaddedInt16b(agmt.tmp.val);
  SERIAL_PORT.print(" ]");
  SERIAL_PORT.println();
}

void printFormattedFloat(float val, uint8_t leading, uint8_t decimals)
{
  float aval = abs(val);
  if (val < 0) {
    SERIAL_PORT.print("-");
  } else {
    SERIAL_PORT.print(" ");
  }
  for (uint8_t indi = 0; indi < leading; indi++) {
    uint32_t tenpow = 0;
    if (indi < (leading - 1)) {
      tenpow = 1;
    }
    for (uint8_t c = 0; c < (leading - 1 - indi); c++) {
      tenpow *= 10;
    }
    if (aval < tenpow) {
      SERIAL_PORT.print("0");
    } else {
      break;
    }
  }
  if (val < 0) {
    SERIAL_PORT.print(-val, decimals);
  } else {
    SERIAL_PORT.print(val, decimals);
  }
}

void printScaledAGMT(ICM_20948_SPI *sensor) {
  SERIAL_PORT.print("Scaled. Acc (mg) [ ");
  printFormattedFloat(sensor->accX(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->accY(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->accZ(), 5, 2);
  SERIAL_PORT.print(" ], Gyr (DPS) [ ");
  printFormattedFloat(sensor->gyrX(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->gyrY(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->gyrZ(), 5, 2);
  SERIAL_PORT.print(" ], Mag (uT) [ ");
  printFormattedFloat(sensor->magX(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->magY(), 5, 2);
  SERIAL_PORT.print(", ");
  printFormattedFloat(sensor->magZ(), 5, 2);
  SERIAL_PORT.print(" ], Tmp (C) [ ");
  printFormattedFloat(sensor->temp(), 5, 2);
  SERIAL_PORT.print(" ]");
  SERIAL_PORT.println();
}
