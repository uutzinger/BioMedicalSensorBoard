/****************************************************************
 * ICM 20948 Read sensor data and enage DMP
 * 
 * Periodically store sensor bias data to EEPROM
 * Initialize the DMP with TDK InvenSense ICM20948_eMD_nucleo_1.0
 * 
 * ** Important note: by default the DMP functionality is disabled in src\util\ICM_20948_C.h.
 * ** To use the DMP, you will need to: uncomment line 29: #define ICM_20948_USE_DMP
 *
 * Paul Clark, April 25th, 2021
 * Based on original code by:
 * Owen Lyke @ SparkFun Electronics, April 17, 2019
 *
 * Please see License.md for the license information.
 * Distributed as-is; no warranty is given.
 *
 ***************************************************************/

#include "ICM_20948.h"
#include <Wire.h>
#include <SPI.h>
#include "logger.h"

// SPI Settings
#define SCLK_PIN       SCK
#define MISO_PIN      MISO
#define MOSI_PIN      MOSI
#define SPI_SCLK_FREQUENCY 1000000 // 100 to 1000 kHz
// IMU
#define ICM_CS_PIN      11
#define ICM_INT_PIN     13
// Pressure
#define BMP_CS_PIN       6
#define BMP_INT_PIN     12

ICM_20948_SPI myICM;        // the ICM object

#include <EEPROM.h>

// Define a storage struct for the biases. Include a non-zero header and a simple checksum
struct biasStore
{
  int32_t header = 0x42;
  int32_t biasGyroX = 0;
  int32_t biasGyroY = 0;
  int32_t biasGyroZ = 0;
  int32_t biasAccelX = 0;
  int32_t biasAccelY = 0;
  int32_t biasAccelZ = 0;
  int32_t biasCPassX = 0;
  int32_t biasCPassY = 0;
  int32_t biasCPassZ = 0;
  int32_t sum = 0;
};

void updateBiasStoreSum(biasStore *store) // Update the bias store checksum
{
  int32_t sum = store->header;
  sum += store->biasGyroX;
  sum += store->biasGyroY;
  sum += store->biasGyroZ;
  sum += store->biasAccelX;
  sum += store->biasAccelY;
  sum += store->biasAccelZ;
  sum += store->biasCPassX;
  sum += store->biasCPassY;
  sum += store->biasCPassZ;
  store->sum = sum;
}

bool isBiasStoreValid(biasStore *store) // Returns true if the header and checksum are valid
{
  int32_t sum = store->header;

  if (sum != 0x42)
    return false;

  sum += store->biasGyroX;
  sum += store->biasGyroY;
  sum += store->biasGyroZ;
  sum += store->biasAccelX;
  sum += store->biasAccelY;
  sum += store->biasAccelZ;
  sum += store->biasCPassX;
  sum += store->biasCPassY;
  sum += store->biasCPassZ;

  return (store->sum == sum);
}

void printBiases(biasStore *store)
{

  Serial.print(F("Gyro X: "));
  Serial.print(store->biasGyroX);
  Serial.print(F(" Gyro Y: "));
  Serial.print(store->biasGyroY);
  Serial.print(F(" Gyro Z: "));
  Serial.println(store->biasGyroZ);
  Serial.print(F("Accel X: "));
  Serial.print(store->biasAccelX);
  Serial.print(F(" Accel Y: "));
  Serial.print(store->biasAccelY);
  Serial.print(F(" Accel Z: "));
  Serial.println(store->biasAccelZ);
  Serial.print(F("CPass X: "));
  Serial.print(store->biasCPassX);
  Serial.print(F(" CPass Y: "));
  Serial.print(store->biasCPassY);
  Serial.print(F(" CPass Z: "));
  Serial.println(store->biasCPassZ);

}

// -----------------------------------------------------------------------------

void setup()
{

  currentLogLevel = LOG_LEVEL_WARN; // NONE, ERROR, WARN, INFO, DEBUG

  Serial.begin(500000);
  while (!Serial);

  LOGI("ICM20948: Setting Pins");
  pinMode(BMP_CS_PIN, OUTPUT);
  pinMode(ICM_CS_PIN, OUTPUT);
  pinMode(BMP_INT_PIN, INPUT_PULLDOWN); // need to configure interrupt register accordingly
  pinMode(ICM_INT_PIN, INPUT_PULLDOWN); // need to fongigure interrupt register accordingly
  digitalWrite(BMP_CS_PIN, HIGH); // deselect BMP before startup
  digitalWrite(ICM_CS_PIN, HIGH); // deselect ICM before startup

 // Initialize the SPI port
  LOGI("ICM20948: setting SPI");
  SPI.begin(SCLK_PIN, MISO_PIN, MOSI_PIN, ICM_CS_PIN);

  // LOGI('ICM20948: enable debugging');
  // myICM.enableDebugging(Serial);

  bool initialized = false;
  while (!initialized)
  {
    myICM.begin(ICM_CS_PIN, SPI, SPI_SCLK_FREQUENCY);

    LOGI("Initialization of the sensor returned: %s", myICM.statusString());
    if (myICM.status != ICM_20948_Stat_Ok) {
      LOGE("Trying again...");
      delay(500);
    } else {
      initialized = true;
    }
  }

  LOGI("Device connected!");

  bool success = true; // Use success to show if the DMP configuration was successful

  // Initialize the DMP. initializeDMP is a weak function. You can overwrite it if you want to e.g. to change the sample rate
  LOGI("Initializing DMP");
  success &= (myICM.initializeDMP() == ICM_20948_Stat_Ok);

  // DMP sensor options are defined in ICM_20948_DMP.h
  //    INV_ICM20948_SENSOR_ACCELEROMETER               (16-bit accel)
  //    INV_ICM20948_SENSOR_GYROSCOPE                   (16-bit gyro + 32-bit calibrated gyro)
  //    INV_ICM20948_SENSOR_RAW_ACCELEROMETER           (16-bit accel)
  //    INV_ICM20948_SENSOR_RAW_GYROSCOPE               (16-bit gyro + 32-bit calibrated gyro)
  //    INV_ICM20948_SENSOR_MAGNETIC_FIELD_UNCALIBRATED (16-bit compass)
  //    INV_ICM20948_SENSOR_GYROSCOPE_UNCALIBRATED      (16-bit gyro)
  // ** INV_ICM20948_SENSOR_STEP_DETECTOR               (Pedometer Step Detector)
  // ** INV_ICM20948_SENSOR_STEP_COUNTER                (Pedometer Step Detector)
  //    INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR        (32-bit 6-axis quaternion)
  //    INV_ICM20948_SENSOR_ROTATION_VECTOR             (32-bit 9-axis quaternion + heading accuracy)
  //    INV_ICM20948_SENSOR_GEOMAGNETIC_ROTATION_VECTOR (32-bit Geomag RV + heading accuracy)
  // ** INV_ICM20948_SENSOR_GEOMAGNETIC_FIELD           (32-bit calibrated compass)
  //    INV_ICM20948_SENSOR_GRAVITY                     (32-bit 6-axis quaternion)
  // ** INV_ICM20948_SENSOR_LINEAR_ACCELERATION         (16-bit accel + 32-bit 6-axis quaternion)
  // ** INV_ICM20948_SENSOR_ORIENTATION                 (32-bit 9-axis quaternion + heading accuracy)

  // Enable the DMP Game Rotation Vector sensor
  // success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR) == ICM_20948_Stat_Ok);

  LOGI("Setting DMP to 9 axis based orientation")
  success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_ORIENTATION) == ICM_20948_Stat_Ok);

  // Enable any additional sensors / features
  // ** success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_STEP_DETECTOR) == ICM_20948_Stat_Ok);
  // ** success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_STEP_COUNTER) == ICM_20948_Stat_Ok);
  //success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_RAW_GYROSCOPE) == ICM_20948_Stat_Ok);
  //success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_RAW_ACCELEROMETER) == ICM_20948_Stat_Ok);
  //success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_MAGNETIC_FIELD_UNCALIBRATED) == ICM_20948_Stat_Ok);
  // ** success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_GEOMAGNETIC_FIELD) == ICM_20948_Stat_Ok);
  // ** success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_LINEAR_ACCELERATION) == ICM_20948_Stat_Ok);

  // Configuring DMP to output data at multiple ODRs:
  // DMP is capable of outputting multiple sensor data at different rates to FIFO.
  // Setting value can be calculated as follows:
  // Value = (DMP running rate / ODR ) - 1
  // E.g. For a 5Hz ODR rate when DMP is running at 55Hz, value = (55/5) - 1 = 10.
  // 0 is full speed
  success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Quat9, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  //success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Quat6, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  // ** success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Accel, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  //success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Gyro, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  //success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Gyro_Calibr, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  //success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Cpass, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  // ** success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Cpass_Calibr, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  // ** success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Step, 0) == ICM_20948_Stat_Ok); // Step detection
  // ** success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Step_Counter, 0) == ICM_20948_Stat_Ok); // Step detection

  // Enable the FIFO
  success &= (myICM.enableFIFO() == ICM_20948_Stat_Ok);

  // Enable the DMP
  success &= (myICM.enableDMP() == ICM_20948_Stat_Ok);

  // Reset DMP
  success &= (myICM.resetDMP() == ICM_20948_Stat_Ok);

  // Reset FIFO
  success &= (myICM.resetFIFO() == ICM_20948_Stat_Ok);

  // Check success
  if (success)
  {
    LOGI("DMP enabled!");
  }
  else
  {
    LOGE("Enable DMP failed!");
    LOGE("Please check that you have uncommented line 29 (#define ICM_20948_USE_DMP) in ICM_20948_C.h...");
    while (1) (delay(50)) ; // Do nothing more
  }

  // Read existing biases from EEPROM
  if (!EEPROM.begin(128)) // Allocate 128 Bytes for EEPROM storage. ESP32 needs this.
  {
    LOGI("EEPROM.begin failed! You will not be able to save the biases...");
  }

  // Setup sensor bias data

  biasStore store;

  // Read the biases from EEPROM
  EEPROM.get(0, store); // Read existing EEPROM, starting at address 0
  if (isBiasStoreValid(&store))
  {
    LOGI("Bias data in EEPROM is valid. Restoring it...");
    success &= (myICM.setBiasGyroX(store.biasGyroX) == ICM_20948_Stat_Ok);
    success &= (myICM.setBiasGyroY(store.biasGyroY) == ICM_20948_Stat_Ok);
    success &= (myICM.setBiasGyroZ(store.biasGyroZ) == ICM_20948_Stat_Ok);
    success &= (myICM.setBiasAccelX(store.biasAccelX) == ICM_20948_Stat_Ok);
    success &= (myICM.setBiasAccelY(store.biasAccelY) == ICM_20948_Stat_Ok);
    success &= (myICM.setBiasAccelZ(store.biasAccelZ) == ICM_20948_Stat_Ok);
    success &= (myICM.setBiasCPassX(store.biasCPassX) == ICM_20948_Stat_Ok);
    success &= (myICM.setBiasCPassY(store.biasCPassY) == ICM_20948_Stat_Ok);
    success &= (myICM.setBiasCPassZ(store.biasCPassZ) == ICM_20948_Stat_Ok);

    if (success)
    {
      LOGI("Biases restored");
      printBiases(&store);
    }
    else
      LOGE("Bias restore failed!");
  }

  LOGI("The biases will be saved in two minutes.");
  LOGI("Before then:");
  LOGI("* Rotate the sensor around all three axes");
  LOGI("* Hold the sensor stationary in all six orientations for a few seconds");

}

// -----------------------------------------------------------------------------
char out_text_buffer[64];

void loop() {

  static unsigned long startTime = millis(); // Save the biases when the code has been running for two minutes
  static bool biasesStored = false;

  // Read any DMP data waiting in the FIFO
  // Note:
  //    readDMPdataFromFIFO will return ICM_20948_Stat_FIFONoDataAvail if no data is available.
  //    If data is available, readDMPdataFromFIFO will attempt to read _one_ frame of DMP data.
  //    readDMPdataFromFIFO will return ICM_20948_Stat_FIFOIncompleteData if a frame was present but was incomplete
  //    readDMPdataFromFIFO will return ICM_20948_Stat_Ok if a valid frame was read.
  //    readDMPdataFromFIFO will return ICM_20948_Stat_FIFOMoreDataAvail if a valid frame was read _and_ the FIFO contains more (unread) data.
  icm_20948_DMP_data_t data;
  myICM.readDMPdataFromFIFO(&data);

  if ((myICM.status == ICM_20948_Stat_Ok) || (myICM.status == ICM_20948_Stat_FIFOMoreDataAvail)) // Was valid data available?
  {
    //Serial.print(F("Received data! Header: 0x")); // Print the header in HEX so we can see what data is arriving in the FIFO
    //if ( data.header < 0x1000) Serial.print( "0" ); // Pad the zeros
    //if ( data.header < 0x100) Serial.print( "0" );
    //if ( data.header < 0x10) Serial.print( "0" );
    //Serial.println( data.header, HEX );

    if ((data.header & DMP_header_bitmap_Quat9) > 0) // We have asked for 9 axis data so we should receive Quat9
    {
      // Q0 value is computed from this equation: Q0^2 + Q1^2 + Q2^2 + Q3^2 = 1.
      // In case of drift, the sum will not add to 1, therefore, quaternion data need to be corrected with right bias values.
      // The quaternion data is scaled by 2^30.

      LOGD("Quat6 data is: Q1:%ld Q2:%ld Q3:%ld", data.Quat6.Data.Q1, data.Quat6.Data.Q2, data.Quat6.Data.Q3);

      // Scale to +/- 1
      double q1 = ((double)data.Quat9.Data.Q1) / 1073741824.0; // Convert to double. Divide by 2^30
      double q2 = ((double)data.Quat9.Data.Q2) / 1073741824.0; // Convert to double. Divide by 2^30
      double q3 = ((double)data.Quat9.Data.Q3) / 1073741824.0; // Convert to double. Divide by 2^30

      LOGD("Q1: %f Q2: %f Q3: %f", q1, q2, q3);

      // Convert the quaternions to Euler angles (roll, pitch, yaw)
      // https://en.wikipedia.org/w/index.php?title=Conversion_between_quaternions_and_Euler_angles&section=8#Source_code_2

      double q0 = sqrt(1.0 - ((q1 * q1) + (q2 * q2) + (q3 * q3)));
      double q2sqr = q2 * q2;
      // roll (x-axis rotation)
      double t0 = +2.0 * (q0 * q1 + q2 * q3);
      double t1 = +1.0 - 2.0 * (q1 * q1 + q2sqr);
      double roll = atan2(t0, t1) * 180.0 / PI;
      // pitch (y-axis rotation)
      double t2 = +2.0 * (q0 * q2 - q3 * q1);
      t2 = t2 > 1.0 ? 1.0 : t2;
      t2 = t2 < -1.0 ? -1.0 : t2;
      double pitch = asin(t2) * 180.0 / PI;
      // yaw (z-axis rotation)
      double t3 = +2.0 * (q0 * q3 + q1 * q2);
      double t4 = +1.0 - 2.0 * (q2sqr + q3 * q3);
      double yaw = atan2(t3, t4) * 180.0 / PI;

      snprintf(out_text_buffer, sizeof(out_text_buffer), "%5.1f %5.1f %5.1f", roll, pitch, yaw);
      Serial.println(out_text_buffer);

      // Output the Quaternion data in the format expected by ZaneL's Node.js Quaternion animation tool
      //snprintf(out_text_buffer, sizeof(out_text_buffer, 
      //  "{\"quat_w\": %f, \"quat_x\": %f, \"quat_y\": %f, \"quat_z\": %f",
      //  q0, q1, q2, q3);
      //Serial.println(out_text_buffer);
    }
  }

  if (myICM.status != ICM_20948_Stat_FIFOMoreDataAvail) // If more data is available then we should read it right away - and not delay
  {
    if (!biasesStored) // Should we store the biases?
    {
      if (millis() > (startTime + 120000)) // Is it time to store the biases?
      {
        LOGI("Saving bias data...");

        biasStore store;
          
        bool success = (myICM.getBiasGyroX(&store.biasGyroX) == ICM_20948_Stat_Ok);
        success &= (myICM.getBiasGyroY(&store.biasGyroY) == ICM_20948_Stat_Ok);
        success &= (myICM.getBiasGyroZ(&store.biasGyroZ) == ICM_20948_Stat_Ok);
        success &= (myICM.getBiasAccelX(&store.biasAccelX) == ICM_20948_Stat_Ok);
        success &= (myICM.getBiasAccelY(&store.biasAccelY) == ICM_20948_Stat_Ok);
        success &= (myICM.getBiasAccelZ(&store.biasAccelZ) == ICM_20948_Stat_Ok);
        success &= (myICM.getBiasCPassX(&store.biasCPassX) == ICM_20948_Stat_Ok);
        success &= (myICM.getBiasCPassY(&store.biasCPassY) == ICM_20948_Stat_Ok);
        success &= (myICM.getBiasCPassZ(&store.biasCPassZ) == ICM_20948_Stat_Ok);

        updateBiasStoreSum(&store);
      
        if (success) {
          biasesStored = true; // Only attempt this once
        
          EEPROM.put(0, store); // Write biases to EEPROM, starting at address 0
          EEPROM.commit(); // ESP32/SAMD/STM32 needs this
  
          EEPROM.get(0, store); // Read existing EEPROM, starting at address 0
          if (isBiasStoreValid(&store))
          {
            LOGI("Biases stored.");
            printBiases(&store);
          }
          else
            LOGE("Bias store failed!");
        } else{
          LOGE("Bias update failed!");
        }
      }
    }
    delay(10);
  }
}
