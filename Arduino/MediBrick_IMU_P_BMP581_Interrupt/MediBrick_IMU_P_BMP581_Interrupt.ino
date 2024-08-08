/*******************************************************************************
 * For the MediBrick IMU Board BMP581 Pressure Sensor Test
 * @author Urs Utzinger
 * @copyright GPLv3 
 ******************************************************************************/

#include <SPI.h>
#include "SparkFun_BMP581_Arduino_Library.h"
#include "logger.h"

// Create a new sensor object
BMP581 pressureSensor;

// SPI Settings
#define SCLK_PIN      SCK
#define MISO_PIN     MISO
#define MOSI_PIN     MOSI
// SPI Clock Frequency:
// EPS32: integer divisions of 80Mhz, e.g 80, 40, 20, 10, 5, 1
// BMP 851 3.3V max: 12MHz 1.8V max: 7MHz
// ICM 20948 1.8V max: 7MHz
// BSS138 level shifter, on delay 2.5, on rise 9, off delay 20ns, off fall 7ns = 40ns max transition for impulse: 25 MHz
#define SPI_SCLK_FREQUENCY 7000000 // 100 to 7000 kHz
#define UNUSED_CS_PIN   9
// IMU
#define ICM_CS_PIN     11
#define ICM_INT_PIN    13
// Pressure
#define BMP_CS_PIN      6
#define BMP_INT_PIN    12

int8_t err = BMP5_OK;

// OOR range specification. The center pressure is 1 atm (101325 Pa), and the
// window is the max value supported by the sensor (255)
// uint32_t oorCenter = 101325;
// uint8_t  oorWindow = 255;


// Flag to know when interrupts occur
volatile bool BMP_interruptOccurred = false;

void bmp581InterruptHandler() {
    BMP_interruptOccurred = true;
}

// -----------------------------------------------------------------------------

void setup() {

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

  // The BMP581 can sample up to 240Hz in normal mode.
  // BMP5_ODR_240_HZ  
  // BMP5_ODR_199_1_HZ
  // BMP5_ODR_160_HZ  
  // BMP5_ODR_140_HZ  
  // BMP5_ODR_120_HZ  
  // BMP5_ODR_100_2_HZ
  // BMP5_ODR_80_HZ 
  // BMP5_ODR_70_HZ 
  // BMP5_ODR_60_HZ 
  // BMP5_ODR_50_HZ 
  // BMP5_ODR_40_HZ 
  // BMP5_ODR_20_HZ 
  // BMP5_ODR_10_HZ 
  // BMP5_ODR_01_HZ 

  err = pressureSensor.setODRFrequency(BMP5_ODR_240_HZ);
  if( err != BMP5_OK) { LOGE("ODR setting failed! Error code: %d", err); }

  // The BMP581 can filter both the temperature and pressure measurements individually.
  // By default, both filter coefficients are set to 0 (no filtering). We can smooth
  // out the measurements by increasing the coefficients with this function
  // It appears the IIR length is proprtional to a reduction of sample rate
  // 3 Coefficients after a steap it takes 20 measurements to reach step
  // 127 coefficients it takes about 700 measurements to reach step 
  // BMP5_IIR_FILTER_BYPASS
  // BMP5_IIR_FILTER_COEFF_1 
  // BMP5_IIR_FILTER_COEFF_3   Fast, takes 2 previously measured data points and 2 previous filtered data points to compute current filtered data point
  // BMP5_IIR_FILTER_COEFF_7
  // BMP5_IIR_FILTER_COEFF_15 
  // BMP5_IIR_FILTER_COEFF_31 
  // BMP5_IIR_FILTER_COEFF_63 
  // BMP5_IIR_FILTER_COEFF_127 Most precise but slowest

  bmp5_iir_config config =
  {
      .set_iir_t = BMP5_IIR_FILTER_COEFF_3, // Set filter coefficient for temperature
      .set_iir_p = BMP5_IIR_FILTER_COEFF_3, // Set filter coefficient for pressure
      .shdw_set_iir_t = BMP5_ENABLE, // Store filtered data in data registers
      .shdw_set_iir_p = BMP5_ENABLE, // Store filtered data in data registers
      .iir_flush_forced_en = BMP5_DISABLE // Flush filter in forced mode, FORCED is for low power infrequent single reads
  };
  err = pressureSensor.setFilterConfig(&config);
  if(err != BMP5_OK) { LOGE("BMP581: Error %d setting filter coefficient!", err); }

  // The BMP581 has multiple possible interrupt conditions, one of which is
  // out-of-range (OOR). This will trigger once the measured pressure goes above
  // or below some customizable range. This range is defined by a center value
  // +/- a window value. The center value can be any 17-bit number in Pa
  // (up to 131071 Pa), and the window can be any 8-bit number in Pa (up to 255)

  // bmp5_oor_press_configuration oorConfig
  // {
  //     .oor_thr_p     = oorCenter, // Center value, up to 131071 Pa
  //     .oor_range_p   = oorWindow, // Window value, up to 255 Pa
  //     .cnt_lim       = BMP5_OOR_COUNT_LIMIT_3, // Number of measurements that need to be out of range before interrupt occurs
  //     .oor_sel_iir_p = BMP5_DISABLE // Whether to check filtered or unfiltered measurements
  // };
  // pressureSensor.setOORConfig(&oorConfig);
  // if(err != BMP5_OK) { LOGE("Interrupt settings failed! Error code: %d", err); }

  // Configure the BMP581 to trigger interrupts whenever a measurement is performed
  BMP581_InterruptConfig interruptConfig =
  {
      .enable   = BMP5_INTR_ENABLE,    // Enable interrupts
      .drive    = BMP5_INTR_PUSH_PULL, // Push-pull or open-drain
      .polarity = BMP5_ACTIVE_HIGH,    // Active low or high
      .mode     = BMP5_PULSED,         // Latch or pulse signal
      .sources  =
      {
          .drdy_en = BMP5_ENABLE,        // Trigger interrupts when data is ready
          .fifo_full_en = BMP5_DISABLE,  // Trigger interrupts when FIFO is full
          .fifo_thres_en = BMP5_DISABLE, // Trigger interrupts when FIFO threshold is reached
          .oor_press_en = BMP5_DISABLE   // Trigger interrupts when pressure goes out of range
      }
  };

  err = pressureSensor.setInterruptConfig(&interruptConfig);
  if(err != BMP5_OK) { LOGE("Interrupt settings failed! Error code: ", err); }

  // Setup interrupt handler
  attachInterrupt(digitalPinToInterrupt(BMP_INT_PIN), bmp581InterruptHandler, RISING);

}

// -----------------------------------------------------------------------------

#define P0 101325.0     // Standard atmospheric pressure at sea level (Pa)
#define T0 288.15       // Standard temperature at sea level (K)
#define L 0.0065        // Temperature lapse rate (K/m)
#define R 8.31432       // Universal gas constant (N·m/(mol·K))
#define g 9.80665       // Acceleration due to gravity (m/s^2)
#define M 0.0289644     // Molar mass of Earth's air (kg/mol)

float pressureToAltitude(float pressure, float seaLevelPressure = P0) {
    if (pressure <= 0) {
        return 0;
    }
    return ((T0 / L) * (1 - pow(pressure / seaLevelPressure, (R * L) / (g * M))));
}

// -----------------------------------------------------------------------------

char out_text_buffer[64];
bmp5_sensor_data data = {0,0};
float temperature = 0.;
float pressure = 0.;
float altitude = 0.;
long currentTime = millis();
long lastTime = currentTime;
uint8_t cnt=0;
uint8_t sps=80;

void loop()
{

  currentTime = millis();
  // Samples per second processed
  if ((currentTime - lastTime) > 1000) {
    lastTime = currentTime;
    sps = cnt;
    cnt = 0;
  }  

  if(BMP_interruptOccurred){
    // Reset flag for next interrupt
    BMP_interruptOccurred = false;
    
    cnt++; // increment sample counter

    LOGD("Interrupt occurred.");

    // Get the interrupt status to know which condition triggered
    uint8_t interruptStatus = 0;
    err = pressureSensor.getInterruptStatus(&interruptStatus);
    if(err != BMP5_OK) { 
      LOGW("Get interrupt status failed! Error code: %d", err);
      return;
    }

    // Check if this is the "data ready" interrupt condition
    if(interruptStatus & BMP5_INT_ASSERTED_DRDY) {
      // Get measurements from the sensor
      int8_t err = pressureSensor.getSensorData(&data);
      // Check whether data was acquired successfully
        if (err == BMP5_OK) {
          temperature = data.temperature;
          pressure = data.pressure;
          altitude = pressureToAltitude(pressure);
          snprintf(out_text_buffer, sizeof(out_text_buffer), "%6.2f, %8.2f, %8.2f, %d", temperature, pressure, altitude, sps);
          Serial.println(out_text_buffer);
        } else {
          LOGW("Error getting data from sensor! Error code: %d", err);
        }
    }

    // // Check if this is the "out-of-range" interrupt condition
    // if(interruptStatus & BMP5_INT_ASSERTED_PRESSURE_OOR) {
    //   LOGI("Out of range condition triggered!");
    // }

    // // Check if neither interrupt occurred
    // if(!(interruptStatus & (BMP5_INT_ASSERTED_PRESSURE_OOR | BMP5_INT_ASSERTED_DRDY))) {
    //   LOGE("Wrong interrupt condition!");
    // }

  }

  delay(1); // give some time for RTOS

}
