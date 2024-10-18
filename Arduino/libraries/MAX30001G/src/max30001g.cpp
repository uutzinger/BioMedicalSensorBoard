/******************************************************************************************************/
// Driver for the MAX30001G Bio Potential and Impedance Front End 
// from Maxim Integrated/Analog Devices
// 
// This driver attempts to be "complete" and offer access to all features of the MAX30001G
// Urs Utzinger, July 2024
// ChatGPT, July 2024
/******************************************************************************************************/
#include "max3001g.h"
#include "logger.h"


/******************************************************************************************************/
// Ring Buffer
/******************************************************************************************************/

class RingBuffer {
private:
    float* buffer;
    size_t capacity;
    size_t start;
    size_t end;
    size_t count;
    bool overflow;

public:
    RingBuffer(size_t size) : capacity(size), start(0), end(0), count(0), overflow(false) { 
      buffer = new float[size]; 
    }

    ~RingBuffer() { delete[] buffer; }

    void push(float value) {
        buffer[end] = value;
        end = (end + 1) % capacity;
        if (count == capacity) {
            start = (start + 1) % capacity;  // Move start forward when overwriting
            overflow = true;
        } else {
            count++;
        }        
    }

    float pop() {
        if (count > 0) {
            float value = buffer[start];
            start = (start + 1) % capacity;
            count--;
            return value;
        }
        return 0; // Or handle underflow appropriately
    }

    bool isFull() const {
        return count == capacity;
    }

    bool isEmpty() const {
        return count == 0;
    }

    bool isOverflow() const {
        return overflow;
    }

    void resetOverflow() {
        overflow = false;  // Reset the overflow flag
    }

    size_t size() const {
        return count;
    }
};

/******************************************************************************************************/
// MAX30001G
/******************************************************************************************************/

// SPI Settings
SPISettings SPI_SETTINGS(MAX30001_SPI_SPEED, MSBFIRST, SPI_MODE0); 

// The AFE device
MAX30001G::MAX30001G(int csPin)
  : ECG_data(128), BIOZ_data(128), _csPin(csPin) { 
    // assing pins
    LOGln("AFE: Pins CS, %u", _csPin);
    pinMode(_csPin,  OUTPUT);       // chip select
    LOGln("AFE: CS pin High");
    digitalWrite(_csPin, HIGH);      // CS float

    // SPI
    LOGln("AFE: SPI start");
    SPI.begin();
    LOG("AFE: Checking communication");
    if (spiCheck()) {
      LOGln("passed");
    } else {
      LOGln("failed");
      return();
    }

    LOGln("AFE Reset");
    swReset();

    LOGln("AFE: Reading all registers");
    readAllRegisters();

    // SetFMSTR
    // 
    // 0 0b00 = 32768 Hz = FLCK
    // 1 0b01 = 32000 Hz = FLCK*625/640
    // 2 0b10 = 32000 Hz = FLCK*625/640
    // 3 0b11 = 31968.78 Hz = FLCK*640/656
    //
    // also updates global variables that depend on the FMSTR
    LOGln("AFE: Setting the clock");
    setFMSTR(0);

    dumpRegisters();

}

/******************************************************************************************************/
// SPI service functions
/******************************************************************************************************/

void MAX30001G::writeRegister(uint8_t address, uint32_t data) {
/*
 * Service routine to write to register with SPI
 */
  LOGD("SPI write 0x%02X, 0x%06X", address, data);  // Address: hex, Data: 24-bit hex
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(_csPin, LOW);
  // delayMicroseconds(50); // Should only take 15ns for SCLK setuptime
  SPI.transfer((address << 1) | 0x00); // write command 
  SPI.transfer((data >> 16) & 0xFF);   // top 8 bits
  SPI.transfer((data >> 8)  & 0xFF);   // middle 8 bits
  SPI.transfer(data         & 0xFF);   // low 8 bits
  // delayMicroseconds(50); // Should not need a delay
  digitalWrite(_csPin, HIGH);
  SPI.endTransaction();
}

uint32_t MAX30001G::readRegister24(uint8_t address) {
/*
 * Service routine to read 3 bytes of data through SPI
 */
  uint32_t data;
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(_csPin, LOW);
  SPI.transfer((address << 1) | 0x01);
  data  = ((uint32_t)SPI.transfer(0xff) << 16); // top 8 bits
  data |= ((uint32_t)SPI.transfer(0xff) << 8);  // middle 8 bits
  data |=  (uint32_t)SPI.transfer(0xff);        // low 8 bits
  digitalWrite(_csPin, HIGH);
  SPI.endTransaction();
  LOGD("SPI read 0x%02X, 0x%06X", address, data);
  return data;
}

uint8_t MAX30001G::readRegisterByte(uint8_t address) {
/*
 * Service routine to read one byte of data through SPI
 */
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(_csPin, LOW);
  SPI.transfer(address << 1 | 0x01);
  uint8_t data =  SPI.transfer(0xff);  // 8 bits
  digitalWrite(_csPin, HIGH);
  SPI.endTransaction();
  LOGD("SPI read 0x%02X, 0x%02X", address, data);
  return data;
}

/******************************************************************************************************/
// Accessing readings
/******************************************************************************************************/

void MAX30001G::readECG_FIFO_BURST(bool reportRaw) {
/*
 * Burst read of the ECG FIFO
 *   Call this routine when ECG FIFO interrupt occured
 */

  // Obtain number of sampples in the FIFO buffer
  this->mngr_int.all = this->readRegister24(MAX30001_MNGR_INT);
  uint8_t  num_samples = (this->mngr_int.e_fit + 1);
  
  max3001_ecg_burst_reg_t ecg_data;
  float fdata;

  // Burst read FIFO register
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(this->_csPin, LOW);
  SPI.transfer((ECG_FIFO_BURST << 1) | 0x01);
  this->ecg_counter = 0;
  bool overflow = false;

  for (int i = 0; i < num_samples; i++) {
    ecg_data.all = ((uint32_t)SPI.transfer(0x00) << 16) | // to 8 bits
                   ((uint32_t)SPI.transfer(0x00) << 8)  | // middle 8 bits
                    (uint32_t)SPI.transfer(0x00);         // low 8 bits
    LOGD("ECG Tag: %3u", ecg_data.bit.etag);
    // check for valid sample, 0 and 2 etag is valid
    if (ecg_data.bit.etag == 0 || ecg_data.bit.etag == 2) {
      int32_t sdata = (int32_t)(ecg_data.bit.data << 14) >> 14; // Sign extend the data 18 bit
      if (reportRaw) {
        fdata = float(sdata);
      } else {
        // Fill the ring buffer with calibrated values
        //   V_ECG (mV) = ADC x VREF / (2^17 x ECG_GAIN): ECG_GAIN is 20V/V, 40V/V, 80V/V or 160V/V. 
        fdata = float(sdata * this->V_ref) / float(131072 * this->ECG_gain);
      }
      this->ECG_data.push(fdata);
      this->ecg_counter++;
      // Check for ring buffer overflow
      if this->ECG_data.isOverflow() { 
        LOGE("ECG burst write  overflow");
        this->ECG_data.resetOverflow(); 
      }
      LOGD("ECG Sample: %.2f [mV] %d", fdata, sdata);
    } else if (ecg_data.bit.etag == 0x07) { 
      // Check for device FIFO Overflow
      overflow = true;
      break;
    }
  }
  digitalWrite(this->_csPin, HIGH);
  SPI.endTransaction();
  if (overflow) { this->FIFOReset(); }
  LOGD("Read %3u samples from ECG FIFO ", this->ecg_counter);
}

void MAX30001G::readBIOZ_FIFO_BURST(bool reportRaw) {
/*
 * Burst read of the BIOZ FIFO
 *  Call this routine when BIOZ FIFO interrupt occured
 */

  // Obtain number of sampples in the FIFO buffer
  this->mngr_int.all = this->readRegister24(MAX30001_MNGR_INT);
  uint8_t  num_samples = (this->mngr_int.b_fit +1);
  
  max3001_bioz_burst_t bioz_data;
  float fdata;

  // Burst read FIFO register
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(this->_csPin, LOW);
  SPI.transfer((BIOZ_FIFO_BURST << 1) | 0x01);
  this->bioz_counter = 0;
  bool overflow = false;

  for (int i = 0; i < num_samples; i++) {
    bioz_data.all = ((uint32_t)SPI.transfer(0x00) << 16) | // to 8 bits
                    ((uint32_t)SPI.transfer(0x00) << 8)  | // middle 8 bits
                     (uint32_t)SPI.transfer(0x00);         // low 8 bits
    LOGD("BIOZ Tag: %3u", bioz_data.bit.btag);
    if (bioz_data.bit.btag == 0 || bioz_data.bit.btag == 2) {     // 0,2 are valid samples
      int32_t sdata = (int32_t)(bioz_data.bit.data << 12) >> 12; // Sign extend the data 20 bit
      if (reportRaw) {
        fdata = float(sdata);
      } else {
        // Fill the buffer with calibrated values
        // BioZ (Ω) = ADC x VREF / (2^19 x BIOZ_CGMAG x BIOZ_GAIN)
        //   BIOZ_CGMAG is 55 to 96000 nano A, set by CNFG_BIOZ and CNFG_BIOZ_LC 
        //   BIOZ_GAIN = 10V/V, 20V/V, 40V/V, or 80V/V. BIOZ_GAIN are set in CNFG_BIOZ (0x18).
        fdata = float(sdata * this->V_ref * 1e9) / float(524288* this->BIOZ_cgmag * this->BIOZ_gain); // in Ω
      }
      this->BIOZ_data.push(fdata);
      this->bioz_counter++;
      LOGD("BIOZ Sample: %.2f [Ohm] %d", fdata, sdata)
      // Check for ring buffer overflow
      if this->BIOZ_data.isOverflow() { 
        LOGE("BIOZ burst write  overflow");
        this->BIOZ_data.resetOverflow(); 
      }
    } else if (bioz_data.bit.btag == 0x07) { 
      // Check for device FIFO Overflow
      overflow = true;
      break;
    }
  }
  digitalWrite(this->_csPin, HIGH);
  SPI.endTransaction();
  if (overflow) { this->FIFOReset(); }
  LOGD("Read %3u samples from BIOZ FIFO ", this->bioz_counter);
}

float MAX30001G::readBIOZ_FIFO(bool reportRaw) {
/*
 * Read of the BIOZ data register
 *
 */

  max3001_bioz_burst_t bioz_data; // FIFO burst and regular read have some register structure
  float fdata;

  bioz_data = this->readRegister24(MAX30001_BIOZ_FIFO);
  this->readStatusRegisters();

  LOGD("BIOZ Tag: %3u", bioz_data.bit.btag);

  if (bioz_data.bit.btag == 0 || bioz_data.bit.btag == 2) {     // 0,2 are valid samples
    int32_t sdata = (int32_t)(bioz_data.bit.data << 12) >> 12; // Sign extend the data 20 bit
    if (reportRaw) {
      fdata = float(sdata);
    } else {
      // Fill the buffer with calibrated values
      // BioZ (Ω) = ADC x VREF / (2^19 x BIOZ_CGMAG x BIOZ_GAIN)
      //   BIOZ_CGMAG is 55 to 96000 nano A, set by CNFG_BIOZ and CNFG_BIOZ_LC 
      //   BIOZ_GAIN = 10V/V, 20V/V, 40V/V, or 80V/V. BIOZ_GAIN are set in CNFG_BIOZ (0x18).
      fdata = float(sdata * this->V_ref) / (float(524288* this->BIOZ_cgmag * this->BIOZ_gain) * 1e-9); // in Ω
    }
  } else {
    fdata = INFINITY; //
    sdata = INT32_MAX;   // 
  }
  LOGD("BIOZ Sample: %.2f [Ohm] %d", fdata, sdata)

  // over_voltage_detected;
  // under_voltage_detected;
  // valid_data_detected;
  // EOF_detected;

  switch (bioz_data.bit.btag) {
  
    case 0b000:  
      this->valid_data_detected = true;
      break;
    case 0b001:
      this->valid_data_detected = false;
      break;
    case 0b010:
      this->valid_data_detected = true;
      this->EOF_data_detected = true;
      break;
    case 0b011:
      this->valid_data_detected = false;
      this->EOF_data_detected = true;
      break;
    case 0b110:
      this->valid_data_detected = false;
      this->EOF_data_detected = true;
      break;
    case 0b111:
      this->valid_data_detected = false;
      this->FIFOReset();
    default:
      this->valid_data_detected = false;
      this->EOF_data_detected = false;
      LOGE("Invalid btag value: %d", bioz_data.bit.btag);
      break
  }
    
  return fdata;
}

void MAX30001G::readHRandRR(void) {
/*
 * Read the RTOR register to calculate the heart rate and RR interval
 *   Call this routine when RTOR interrupt occured
 */
  this->rtor.all = this->readRegister24(MAX30001_RTOR);
  this->rr_interval = float(this->rtor.data) * this->RtoR_resolution; // in [ms]
  this->heart_rate = 60000./this->rr_interval; // in beats per minute 
}

/******************************************************************************************************/
// General
/******************************************************************************************************/

void MAX30001G::synch(void){
/* Synchronize the device */
  this->writeRegister(MAX30001_SYNCH, 0x000000);  
}

void MAX30001G::swReset(void){
/* Rest Device*/
  this->writeRegister(MAX30001_SW_RST, 0x000000);
  delay(100);
}

void MAX30001G::FIFOReset(void) {
/*
 * Resets the ECG and BIOZ FIFOs by writing to the FIFO_RST register.
 */

  // FIFO_RST register (0x0A) is used to reset both ECG and BIOZ FIFOs.
  LOGD("MAX30001G: Resetting ECG and BIOZ FIFOs.");

  // Write to the FIFO_RST register (0x0A) to reset the FIFO pointers
  this->writeRegister(MAX30001_FIFO_RST, 0x000000);

  this->status.all = this->readRegister(MAX30001_STATUS);
  if (this-status.bit.eovf == 1 || this->status.bit.bovf == 1) {
    LOGE("MAX30001G: FIFO reset failed, overflow flags are still set.");
  } else {
    LOGD("MAX30001G: FIFO reset successful, overflow flags are cleared.");
  }
}

void MAX30001G::setFMSTR(uint8 fmstr){ 
/*
  Set Base Clock Frequency. 
  This will affect the timing of the ECG and BIOZ measurements.

  FMSTR[1:0] Mapping:
  0 0b00 = 32768 Hz = FLCK
  1 0b01 = 32000 Hz = FLCK*625/640
  2 0b10 = 32000 Hz = FLCK*625/640
  3 0b11 = 31968.78 Hz = FLCK*640/656

  The following global variables are updated
    fmstr
    tres
    ECG_progression
    Rtor resolution
    RtoR delay
    CAL_resolution

    ECG_samplingRate
    BIOZ_samplingRate
    ECG_lpf
    ECG_latency
    BioZ_lpf
    BIOZ Frequency
*/

  this->cnfg_gen.all  = this->readRegister24(MAX30001_CNFG_GEN);

  // Apply FMSTR
  this->cnfg_gen.bit.fmstr = fmstr;
  this->writeRegister(MAX30001_CNFG_GEN, cnfg_gen.all);

  // Update Global Variables
  switch (this->cnfg_gen.bit.fmstr) {

      case 0b00: // FMSTR = 32768Hz, TRES = 15.26µs, 512Hz
          this->fmstr = 32768.0; // Hz
          this->tres = 15.26; // µs
          this->ECG_progression = 512.0; // per second
          this->RtoR_resolution = 7.8125; // ms
          this->RtoR_delay = 102.844; // ms
          this->CAL_resolution  = 30.52; // µs
          break;

      case 0b01: // FMSTR = 32000Hz, TRES = 15.63µs, 500Hz
          this->fmstr = 32000.0;
          this->tres = 15.63;
          this->ECG_progression = 500.0;
          this->RtoR_resolution = 8.0;
          this->RtoR_delay = 105.313; // ms
          this->CAL_resolution  = 31.25; // µs
          break;

      case 0b10: // FMSTR = 32000Hz, TRES = 15.63µs, 200Hz
          this->fmstr = 32000.0;
          this->tres = 15.63;
          this->ECG_progression = 200.0;
          this->RtoR_resolution = 8.0;
          this->RtoR_delay = 105.313; // ms
          this->CAL_resolution  = 31.25; // µs
          break;

      case 0b11: // FMSTR = 31968.78Hz, TRES = 15.64µs, 199.8049Hz
          this->fmstr = 31968.78;
          this->tres = 15.64; // µs
          this->ECG_progression = 199.8049; // Hz
          this->RtoR_resolution = 8.008; // ms
          this->RtoR_delay = 105.415; // ms
          this->CAL_resolution  = 31.28; // µs
          break;
      
      default:
          LOGE("Error fmstr, can only be 0,1,2,3: given %u", fmstr);
          LOGE("All timing set to 0.0. System will not work.");
          this->fmstr             = 0.0;
          this->tres              = 0.0;
          this->ECG_progression   = 0.0;
          this->RtoR_resolution   = 0.0;
          this->RtoR_delay        = 0.0; // ms
          this->CAL_resolution    = 0.0;
          break;
  }

  updateGlobalECG_samplingRate();
  updateGlobalBIOZ_samplingRate();
  updateGlobalECG_lpf();
  updateGlobalBIOZ_lpf();
  updateGlobalCAL_fcal();
  updateGlobalECG_latency();
  updateGlobalRCAL_frequ();

}

void MAX30001::updateGlobalECG_samplingRate(void){ 
/* Update ECG Sampling Rate Global */
  // this->cnfg_gen.all  = this->readRegister24(MAX30001_CNFG_GEN);
  // this->cnfg_ecg.all  = this->readRegister24(MAX30001_CNFG_ECG);

  switch (this->cnfg_gen.bit.fmstr) {

    case 0b00: // FMSTR = 32768Hz, TRES = 15.26µs, 512Hz
      switch (this->cnfg_ecg.bit.rate) {
        case 0b00:
          this->ECG_samplingRate = 512.;
          break;
        case 0b01:
          this->ECG_samplingRate = 256.;
          break;
        case 0b10:
          this->ECG_samplingRate = 128.;
          break;
        default:
          this->ECG_samplingRate = 0.;
          break;
      }      
      break;

    case 0b01: // FMSTR = 32000Hz, TRES = 15.63µs, 500Hz
      switch (this->cnfg_ecg.bit.rate) {
        case 0b00:
          this->ECG_samplingRate = 500.;
          break;
        case 0b01:
          this->ECG_samplingRate = 250.;
          break;
        case 0b10:
          this->ECG_samplingRate = 125.;
          break;
        default:
          this->ECG_samplingRate = 0.;
          break;
      }
      break;

    case 0b10: // FMSTR = 32000Hz, TRES = 15.63µs, 200Hz
      switch (this->cnfg_ecg.bit.rate) {
        case 0b00:
          this->ECG_samplingRate = 200.;
          break;
        default:
          this->ECG_samplingRate = 0.;
          break;
      }
      break;

    case 0b11: // FMSTR = 31968.78Hz, TRES = 15.64µs, 199.8049Hz
      switch (this->cnfg_ecg.bit.rate) {
        case 0b00:
          this->ECG_samplingRate = 199.8049;
          break;
        default:
          this->ECG_samplingRate = 0.;
          break;
      }
      break;
    
    default:
      this->ECG_samplingRate  = 0.;
      break;
  }
}

void MAX30001::updateGlobalBIOZ_samplingRate(void){ 
/* Update Sampling Rate Globla for BIOZ */
  switch (this->cnfg_gen.bit.fmstr) {

    case 0b00: // FMSTR = 32768Hz, TRES = 15.26µs, 64Hz
      switch (this->cnfg_bioz.bit.rate) {
        case 0:
          this->BIOZ_samplingRate = 64.0;
          break;
        case 1:
          this->BIOZ_samplingRate = 32.0;
          break;
        default:
          this->BIOZ_samplingRate = 0.0;
          break;
      }
      break;

    case 0b01: // FMSTR = 32000Hz, TRES = 15.63µs, 62.5Hz
      switch (this->cnfg_bioz.bit.rate) {
        case 0:
          this->BIOZ_samplingRate = 62.5;
          break;
        case 1:
          this->BIOZ_samplingRate = 31.25;
          break;
        default:
          this->BIOZ_samplingRate = 0.0;
          break;
      }
      break;

    case 0b10: // FMSTR = 32000Hz, TRES = 15.63µs, 50Hz
      switch (this->cnfg_bioz.bit.rate) {
        case 0:
          this->BIOZ_samplingRate = 50.0;
          break;
        case 1:
          this->BIOZ_samplingRate = 25.0;
          break;
        default:
          this->BIOZ_samplingRate = 0.0;
          break;
      }
      break;

    case 0b11: // FMSTR = 31968.78Hz, TRES = 15.64µs, 49.95Hz
      switch (this->cnfg_bioz.bit.rate) {
        case 0:
          this->BIOZ_samplingRate = 49.95;
          break;
        case 1:
          this->BIOZ_samplingRate = 24.98;
          break;
        default:
          this->BIOZ_samplingRate = 0.0;
          break;
      }
      break;
    
    default:
      this->BIOZ_samplingRate = 0.0;
      break;
  }
}

void MAX30001::updateGlobalCAL_fcal(void) {
/*
 * Frequency calibration for ECG pulses
 * can also be used for BIOZ
 */

  switch (this->cnfg_cal.bit.fcal) {
    case 0b000:
      this->CAL_fcal = this->fmstr / 128.0;
      break;
    case 0b001:
      this->CAL_fcal = this->fmstr / 512.0;
      break;
    case 0b010:
      this->CAL_fcal = this->fmstr / 2048.0;
      break;
    case 0b011:
      this->CAL_fcal = this->fmstr / 8192.0;
      break;
    case 0b100:
      this->CAL_fcal = this->fmstr / 32768.0;
      break;
    case 0b101:
      this->CAL_fcal = this->fmstr / 131072.0;
      break;
    case 0b110:
      this->CAL_fcal = this->fmstr / 524288.0;
      break;
    case 0b111:
      this->CAL_fcal = this->fmstr / 2097152.0;
      break;
    default:
      this->CAL_fcal = this->fmstr / 32768.0;
      break;
  }
}

void MAX30001::updateGlobalECG_latency(void) {
/*
 * ECG delay based on ECG progression and digital HPF
 */

  if (this->ECG_progression >= 512) {
      if (this->ECG_hpf > 0) {
          this->ECG_latency = 31.55; // ms
      } else {
          this->ECG_latency = 19.836; // ms
      }
  } else if (this->ECG_progression >= 500) {
      if (this->ECG_hpf > 0) {
          this->ECG_latency = 32.313; // ms
      } else {
          this->ECG_latency = 20.313; // ms
      }
  } else if (this->ECG_progression >= 256) {
      if (this->ECG_hpf > 0) {
          this->ECG_latency = 112.610; // ms
      } else {
          this->ECG_latency = 89.127; // ms
      }
  } else if (this->ECG_progression >= 250) {
      if (this->ECG_hpf > 0) {
          this->ECG_latency = 115.313; // ms
      } else {
          this->ECG_latency = 91.313; // ms
      }
  } else if (this->ECG_progression >= 200) {
      if (this->ECG_hpf > 0) {
          this->ECG_latency = 68.813; // ms
      } else {
          this->ECG_latency = 38.813; // ms
      }
  } else if (this->ECG_progression >= 199.8049) {
      if (this->ECG_hpf > 0) {
          this->ECG_latency = 68.881; // ms
      } else {
          this->ECG_latency = 38.851; // ms
      }
  } else if (this->ECG_progression >= 128) {
      if (this->ECG_hpf > 0) {
          this->ECG_latency = 149.719; // ms
      } else {
          this->ECG_latency = 102.844; // ms
      }
  } else if (this->ECG_progression >= 125) {
      if (this->ECG_hpf > 0) {
          this->ECG_latency = 153.313; // ms
      } else {
          this->ECG_latency = 105.313; // ms
      }
  } else {
      this->ECG_latency = 0.0;
  }
}

void MAX30001::updateGlobalBIOZ_frequency(void) {
/*
 * BIOZ modulation frequency
 */

  switch (this->cnfg_bioz.bit.fcgen) {

    case 0b0000:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 131072;
          break;
        case 0b01:
          this->BIOZ_frequency = 128000;
          break;
        case 0b10:
          this->BIOZ_frequency = 128000;
          break;
        case 0b11:
          this->BIOZ_frequency = 127872;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b0001:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 81920;
          break;
        case 0b01:
          this->BIOZ_frequency = 80000;
          break;
        case 0b10:
          this->BIOZ_frequency = 80000;
          break;
        case 0b11:
          this->BIOZ_frequency = 79920;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b0010:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 40960;
          break;
        case 0b01:
          this->BIOZ_frequency = 40000;
          break;
        case 0b10:
          this->BIOZ_frequency = 40000;
          break;
        case 0b11:
          this->BIOZ_frequency = 39920;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b0011:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 18204;
          break;
        case 0b01:
          this->BIOZ_frequency = 17780;
          break;
        case 0b10:
          this->BIOZ_frequency = 17780;
          break;
        case 0b11:
          this->BIOZ_frequency = 17720;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b0100:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 8192;
          break;
        case 0b01:
          this->BIOZ_frequency = 8000;
          break;
        case 0b10:
          this->BIOZ_frequency = 8000;
          break;
        case 0b11:
          this->BIOZ_frequency = 7992;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b0101:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 4096;
          break;
        case 0b01:
          this->BIOZ_frequency = 4000;
          break;
        case 0b10:
          this->BIOZ_frequency = 4000;
          break;
        case 0b11:
          this->BIOZ_frequency = 3996;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b0110:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 2048;
          break;
        case 0b01:
          this->BIOZ_frequency = 2000;
          break;
        case 0b10:
          this->BIOZ_frequency = 2000;
          break;
        case 0b11:
          this->BIOZ_frequency = 1998;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b0111:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 1024;
          break;
        case 0b01:
          this->BIOZ_frequency = 1000;
          break;
        case 0b10:
          this->BIOZ_frequency = 1000;
          break;
        case 0b11:
          this->BIOZ_frequency = 999;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b1000:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 512;
          break;
        case 0b01:
          this->BIOZ_frequency = 500;
          break;
        case 0b10:
          this->BIOZ_frequency = 500;
          break;
        case 0b11:
          this->BIOZ_frequency = 499.5;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b1001:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 256;
          break;
        case 0b01:
          this->BIOZ_frequency = 250;
          break;
        case 0b10:
          this->BIOZ_frequency = 250;
          break;
        case 0b11:
          this->BIOZ_frequency = 249.5;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b1010:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 128;
          break;
        case 0b01:
          this->BIOZ_frequency = 125;
          break;
        case 0b10:
          this->BIOZ_frequency = 125;
          break;
        case 0b11:
          this->BIOZ_frequency = 124.75;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b1011:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 128;
          break;
        case 0b01:
          this->BIOZ_frequency = 125;
          break;
        case 0b10:
          this->BIOZ_frequency = 125;
          break;
        case 0b11:
          this->BIOZ_frequency = 124.75;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b1110:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 128;
          break;
        case 0b01:
          this->BIOZ_frequency = 125;
          break;
        case 0b10:
          this->BIOZ_frequency = 125;
          break;
        case 0b11:
          this->BIOZ_frequency = 124.75;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    case 0b1111:
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00:
          this->BIOZ_frequency = 128;
          break;
        case 0b01:
          this->BIOZ_frequency = 125;
          break;
        case 0b10:
          this->BIOZ_frequency = 125;
          break;
        case 0b11:
          this->BIOZ_frequency = 124.75;
          break;
        default:
          this->BIOZ_frequency = 0;
          break;
      }
      break;

    default:
      this->BIOZ_frequency = 0;
      break;
  }
}

void MAX30001::updateGlobalECG_lpf(void){ 
/*
 * ECG digitsl low pass filter
 */

  switch (this->cnfg_gen.bit.fmstr) {

    case 0b00: // FMSTR = 32768Hz, TRES = 15.26µs, 512Hz
      switch (this->cnfg_ecg.bit.rate) {
        case 0b00:
          switch (this->cnfg_ecg.bit.dlpf){
            case 0b00:
              this->ECG_lpf = 0.;  
              break;
            case 0b01:
              this->ECG_lpf = 40.96;  
              break;
            case 0b10:
              this->ECG_lpf = 102.4;  
              break;
            case 0b11:
              this->ECG_lpf = 153.6;  
              break;
          }
          break;
        case 0b01:
          switch (this->cnfg_ecg.bit.dlpf){
            case 0b00:
              this->ECG_lpf = 0.;  
              break;
            case 0b01:
              this->ECG_lpf = 40.96;  
              break;
            case 0b10:
              this->ECG_lpf = 102.4;  
              break;
            case 0b11:
              this->ECG_lpf = 40.96;  
              break;
          }
          break;
        case 0b10:
          switch (this->cnfg_ecg.bit.dlpf){
            case 0b00:
              this->ECG_lpf = 0.;  
              break;
            case 0b01:
              this->ECG_lpf = 28.35;  
              break;
            case 0b10:
              this->ECG_lpf = 28.35;  
              break;
            case 0b11:
              this->ECG_lpf = 28.35;  
              break;
          }
          break;
        default:
          this->ECG_lpf = 0.;  
          break;
      }
      break;

    case 0b01: // FMSTR = 32000Hz, TRES = 15.63µs, 500Hz
      switch (this->cnfg_ecg.bit.rate) {
        case 0b00:
          switch (this->cnfg_ecg.bit.dlpf){
            case 0b00:
              this->ECG_lpf = 0.;  
              break;
            case 0b01:
              this->ECG_lpf = 40.;  
              break;
            case 0b10:
              this->ECG_lpf = 100.;  
              break;
            case 0b11:
              this->ECG_lpf = 150.;  
              break;
          }
          break;
        case 0b01:
          switch (this->cnfg_ecg.bit.dlpf){
            case 0b00:
              this->ECG_lpf = 0.;  
              break;
            case 0b01:
              this->ECG_lpf = 40.;  
              break;
            case 0b10:
              this->ECG_lpf = 100.;  
              break;
            case 0b11:
              this->ECG_lpf = 150.0;  
              break;
          }
          break;
        case 0b10:
          switch (this->cnfg_ecg.bit.dlpf){
            case 0b00:
              this->ECG_lpf = 0.;  
              break;
            case 0b01:
              this->ECG_lpf = 27.68;  
              break;
            case 0b10:
              this->ECG_lpf = 27.68;  
              break;
            case 0b11:
              this->ECG_lpf = 27.68;  
              break;
          }
          break;
        default:
          this->ECG_lpf = 0.;
          break;
      }
      break;

    case 0b10: // FMSTR = 32000Hz, TRES = 15.63µs, 200Hz
      switch (this->cnfg_ecg.bit.rate) {
        case 0b10:
          switch (this->cnfg_ecg.bit.dlpf){
            case 0b00:
              this->ECG_lpf = 0.;  
              break;
            case 0b01:
              this->ECG_lpf = 40.;  
              break;
            case 0b10:
              this->ECG_lpf = 40.;  
              break;
            case 0b11:
              this->ECG_lpf = 40.;  
              break;
          }
          break;
        default:
          this->ECG_lpf = 0.;  
          break;
      }
      break;

    case 0b11: // FMSTR = 31968.78Hz, TRES = 15.64µs, 199.8049Hz
      switch (this->cnfg_ecg.bit.rate) {
        case 0b10:
          switch (this->cnfg_ecg.bit.dlpf){
            case 0b00:
              this->ECG_lpf = 0.;  
              break;
            case 0b01:
              this->ECG_lpf = 39.96;  
              break;
            case 0b10:
              this->ECG_lpf = 39.96;  
              break;
            case 0b11:
              this->ECG_lpf = 39.96;  
              break;
          }
          break;
        default:
          this->ECG_lpf = 0.;  
          break;
      }
      break;

    default:
      this->ECG_lpf = 0.;
      break;
  }
}

void MAX30001::updateGlobalBIOZ_lpf(void){ 
/*
 * BIOZ digitsl low pass filter
 */

  switch (this->cnfg_gen.bit.fmstr) {

    case 0b00: // FMSTR = 32768Hz, TRES = 15.26µs, 512Hz
      switch (this->cnfg_bioz.bit.rate) {
        case 0:
          switch (this->cnfg_bioz.bit.dlpf){
            case 0b00:
              this->BIOZ_lpf = 0.;  
              break;
            case 0b01:
              this->BIOZ_lpf = 4.096;  
              break;
            case 0b10:
              this->BIOZ_lpf = 8.192;  
              break;
            case 0b11:
              this->BIOZ_lpf = 16.384;  
              break;
          }
          break;

        case 1:
          switch (this->cnfg_bioz.bit.dlpf){
            case 0b00:
              this->BIOZ_lpf = 0.;  
              break;
            case 0b01:
              this->BIOZ_lpf = 4.096;  
              break;
            case 0b10:
              this->BIOZ_lpf = 8.192;  
              break;
            case 0b11:
              this->BIOZ_lpf = 4.096;  
              break;
          }
          break;
      }
      break;

    case 0b01: // FMSTR = 32000Hz, TRES = 15.63µs, 500Hz
      switch (this->cnfg_bioz.bit.rate) {
        case 0:
          switch (this->cnfg_bioz.bit.dlpf){
            case 0b00:
              this->BIOZ_lpf = 0.;  
              break;
            case 0b01:
              this->BIOZ_lpf = 4.;  
              break;
            case 0b10:
              this->BIOZ_lpf = 8.;  
              break;
            case 0b11:
              this->BIOZ_lpf = 16.;  
              break;
          }
          break;
        case 1:
          switch (this->cnfg_bioz.bit.dlpf){
            case 0b00:
              this->BIOZ_lpf = 0.;  
              break;
            case 0b01:
              this->BIOZ_lpf = 4.;  
              break;
            case 0b10:
              this->BIOZ_lpf = 8.;  
              break;
            case 0b11:
              this->BIOZ_lpf = 4.0;  
              break;
          }
          break;
      }
      break;

    case 0b10: // FMSTR = 32000Hz, TRES = 15.63µs, 200Hz
      switch (this->cnfg_bioz.bit.rate) {
        case 0:
          switch (this->cnfg_bioz.bit.dlpf){
            case 0b00:
              this->BIOZ_lpf = 0.;  
              break;
            case 0b01:
              this->BIOZ_lpf = 4.;  
              break;
            case 0b10:
              this->BIOZ_lpf = 8.;  
              break;
            case 0b11:
              this->BIOZ_lpf = 16.;  
              break;
          }
          break;
        case 1:
          switch (this->cnfg_bioz.bit.dlpf){
            case 0b00:
              this->BIOZ_lpf = 0.;  
              break;
            case 0b01:
              this->BIOZ_lpf = 4.;  
              break;
            case 0b10:
              this->BIOZ_lpf = 8.;  
              break;
            case 0b11:
              this->BIOZ_lpf = 4.;  
              break;
          }
          break;
      }
      break;

    case 0b11: // FMSTR = 31968.78Hz, TRES = 15.64µs, 199.8049Hz
      switch (this->cnfg_bioz.bit.rate) {
        case 0:
          switch (this->cnfg_bioz.bit.dlpf){
            case 0b00:
              this->BIOZ_lpf = 0.;  
              break;
            case 0b01:
              this->BIOZ_lpf = 3.996;  
              break;
            case 0b10:
              this->BIOZ_lpf = 7.992;  
              break;
            case 0b11:
              this->BIOZ_lpf = 15.984;  
              break;
          }
          break;
        case 1:
          switch (this->cnfg_bioz.bit.dlpf){
            case 0b00:
              this->BIOZ_lpf = 0.;  
              break;
            case 0b01:
              this->BIOZ_lpf = 3.996;  
              break;
            case 0b10:
              this->BIOZ_lpf = 7.992;  
              break;
            case 0b11:
              this->BIOZ_lpf = 3.996;  
              break;
          }
          break;
      }
      break;

    default:
      this->BIOZ_lpf = 0.;
      break;
  }
}

void MAX30001::updateGlobalRCAL_freq() {
/*
 * Test resistor modulation frequency, changes thre resistance at that frequency
 */

  switch (this->cnfg_bmu.bit.fbist) {
    case 0:
      this->RCAL_freq =this->fmstr/8192.;
      break;
    case 1:
      this->RCAL_freq =this->fmstr/32768.;
      break;
    case 2:
      this->RCAL_freq =this->fmstr/131072;
      break;
    case 3:
      this->RCAL_freq =this->fmstr/524288;
      break;
  }
};

/******************************************************************************************************/
// Interrupt service routine
/******************************************************************************************************/

void MAX30001G::serviceAllInterrupts() {
/* General Interrupt Service Routine
 * This will updated gloabl variable
 * Mainprogram will need to check the variables
 */

  this->status.all = this->readRegister24(MAX30001_STATUS);

  LOGD("Interrupt Status: %s", intToBinaryString(this->status));

  if (this->status.bit.eint) {
    LOGD("ECG FIFO interrupt");
    this->ecg_available = true;
  }

  if (this->status.bit.eovf) {
    LOGD("ECG buffer overflow detection interrupt");
    this->ecg_overflow_occured = true;
  }

  if (this->status.bit.fstint) {
    LOGD("Fast recovery interrupt");
    this->ecg_fast_recovery_occurred = true;
  }

  if (this->status.bit.dcloffint) {
    LOGD("ECG lead off interrupt");
    this->ecg_lead_off = true;
  }

  if (this->status.bit.bint) {
    LOGD("BIOZ FIFO interrupt");
    this->bioz_available = true;
  }

  if (this->status.bit.bovf) {
    LOGD("BIOZ FIFO overflow interrupt");
    this->bioz_overflow_occured = true;
  }

  if (this->status.bit.bover) {
    LOGD("BIOZ over voltage interrupt");
    this->bioz_overvoltage_occured = true;
  }

  if (this->status.bit.bundr) {
    LOGD("BIOZ under voltage interrupt");
    this->bioz_undervoltage_occured =  true;
  }

  if (this->status.bit.bcgmon) {
    LOGD("BIOZ current generator monitor interrupt");
    this->bioz_cgm_occured = true;
  }

  if (this->status.bit.lonint) {
    LOGD("Leads on detection");
    this->leads_on_detected = true;
  }

  if (this->status.bit.print) {
    LOGD("R to R interrupt");
    this->rtor_available = true;
  }

  if (this->status.bit.pllint) {
    LOGD("PLL interrupt");
    this->pll_unlocked_occured = true;
  }
}

void MAX30001G::setInterrupt1(bool ecg, bool bioz, bool leadon, bool leadoff, bool fourwire) {
/*
  Enable/Disable Interrupts.

  There are two interrupt lines and each can be triggered by setting its registers
  Here we set the first interrupt line.

  ecg:      ECG   related interrupts enabled
  bioz:     BIOZ  related interrupts enabled
  leadon:   Leads on detection enabled
  leadoff:  Leads off detection enabled
  fourwire: BIOZ  current monitor if 4 wire BIOZ is used

Registers involved

General

  en_int1.bit.int_type
    Hrdware logic used for the interrupt pin
    0 disabled
    1 CMOS
    2 OpenDrain
    3 OpenDrain with internal pull up * default

BIOZ

  en_int1.bit.en_bint       
    BIOZ FIFO
    set if threshold for available samples in FIFO is met
    remains active until the FIFO is read to the extent required by BFIT

  en_int1.bit.en_bovf       
    BIOZ FIFO overflow
    Overlow of FIFO occured. Remains active until FIFO reset.

  en_int1.bit.en_bcgmon     
    BIOZ current monitor
    DRVN/P is in lead off condition
    Remains on as long as condition persist, cleared by reading status
    4 electrode  BIOZ

  en_int1.bit.en_bunder
    BIOZ under range
    Output has exceeded low threshold (BLOFF_LO_IT).
    Remains on as long as condition persists, cleared by reading status
    4  electrode BIOZ with lead off detection enabled

  en_int1.bit.en_bover      
    BIOZ over range
    Output has exceeded high threshold (BLOFF_HI_IT).
    Remains on as long as condition persists, cleared by reading status
    2 or 4 electrode BIOZ with lead off detection enabled

  en_int1.bit.en_pllint     
    PLL unlocked
    PLL has lost its phase or is not yet active
    Can occur when ECG or BIOZ channels are enabled
    Remains on until PLL is locked and status is read

ECG

  en_int1.bit.en_eint       
    ECG FIFO
    set if threshold for evailable samples in FIFO is met
    remains active until the FIFO is read to the extent required by EFIT

  en_int1.bit.en_fstint     
    ECG fast recovery
    Manual or automatic recovery started
    Behavious set by CLR_FAST in MNGR_INT 
  
  en_int1.bit.en_eovf       
    ECG FIFO overflow
    Overflow in the FIFO occured. Remains active until FIFO reset

  en_int1.bit.en_samp       
    Sample synchronization pulse
    To synchronize other external hardware based on ECG sampling instant

  en_int1.bit.en_rrint      
    ECG R to R detection
    Cleared based on CLR_RRINT in MNGR_INT register

ECG & BIOZ

  en_int1.bit.en_dcloffint  
    DC lead off detection
    Remains on until lead on and cleared by status read

  en_int1.bit.en_lonint     
    Ultra low power leads on detection
    EN_ULP_LON in CNFG_GEN determine the condition
    Remains on while leads are on and cleared by status read.

*/

  this->en_int1.all  = this->readRegister24(MAX30001_EN_INT1);

  this->en_int1.bit.int_type      = 3; // 3 OpenDrain with internal pull up
  
  this->en_int1.bit.en_samp       = 0; // no external hardware needs sampling pulse

  if (ecg) {
    this->en_int1.bit.en_eint       = 1; // ECG FIFO interrupt enable
    this->en_int1.bit.en_fstint     = 1; // ECG fast recovery interrupt enable
    this->en_int1.bit.en_eovf       = 1; // ECG FIFO overflow interrupt enable
    this->en_int1.bit.en_rrint      = 1; // ECG R to R detection interrupt enable
  } else {
    this->en_int1.bit.en_eint       = 0; // ECG FIFO interrupt enable
    this->en_int1.bit.en_fstint     = 0; // ECG fast recovery interrupt enable
    this->en_int1.bit.en_eovf       = 0; // ECG FIFO overflow interrupt enable
    this->en_int1.bit.en_rrint      = 0; // ECG R to R detection interrupt enable
  }

  if (bioz) {
    this->en_int1.bit.en_bint       = 1; // BIOZ FIFO interrupt enable
    this->en_int1.bit.en_bover      = 1; // BIOZ over range interrupt enable
    this->en_int1.bit.en_bovf       = 1; // BIOZ FIFO overflow interrupt enable
    this->en_int1.bit.en_bunder     = fourwire ? 1:0; // BIOZ under range interrupt enable
    this->en_int1.bit.en_pllint     = 0; // PLL unlocked, PLL has lost its phase or is not yet active
  } else {
    this->en_int1.bit.en_bint       = 0; // BIOZ FIFO interrupt enable
    this->en_int1.bit.en_bover      = 0; // BIOZ over range interrupt enable
    this->en_int1.bit.en_bovf       = 0; // BIOZ FIFO overflow interrupt enable
    this->en_int1.bit.en_bunder     = 0; // BIOZ under range interrupt enable
    this->en_int1.bit.en_pllint     = 0; // PLL unlocked, PLL has lost its phase or is not yet active
  }

  this->en_int1.bit.en_lonint     = leadon ? 1:0; // Ultra low power leads on detection interrupt enable

  if (leadoff) {
    this->en_int1.bit.en_dcloffint  = 1; // DC lead off detection interrupt enable
    this->en_int1.bit.en_bcgmon     = fourwire ? 1:0; // BIOZ current monitor
  } else {
    this->en_int1.bit.en_dcloffint  = 0; // DC lead off detection interrupt enable
    this->en_int1.bit.en_bcgmon     = 0; // BIOZ current monitor
  }

  this->writeRegister(MAX30001_EN_INT1, this->en_int1.all);
}

void MAX30001G::setInterrupt2(bool ecg, bool bioz, bool leadon, bool leadoff, bool fourwire) {
/*
  Enable/Disable interrupts.
  There are two interrupt lines and each can be triggered by setting its registers
  Here we set the second interrupt line

  en_int2.bit.int_type
  Hardware logic used for the interrupt pin
    0 disabled
    1 CMOS
    2 OpenDrain
    3 OpenDrain with internal pull up 

BIOZ

  en_int2.bit.en_bint       
    BIOZ FIFO
    set if threshold for available samples in FIFO is met
    remains active until the FIFO is read to the extent required by BFIT

  en_int2.bit.en_bovf       
    BIOZ FIFO overflow
    Overlow of FIFO occured. Remains active until FIFO reset.

  en_int2.bit.en_bcgmon     
    BIOZ current monitor
    DRVN/P is in lead off condition
    Remains on as long as condition persist, cleared by reading status
    4 electrode  BIOZ

  en_int2.bit.en_bunder
    BIOZ under range
    Output has exceeded low threshold (BLOFF_LO_IT).
    Remains on as long as condition persists, cleared by reading status
    4  electrode BIOZ with lead off detection enabled

  en_int2.bit.en_bover      
    BIOZ over range
    Output has exceeded high threshold (BLOFF_HI_IT).
    Remains on as long as condition persists, cleared by reading status
    2 or 4 electrode BIOZ with lead off detection enabled

  en_int2.bit.en_pllint     
    PLL unlocked
    PLL has lost its phase or is not yet active
    Can occur when ECG or BIOZ channels are enabled
    Remains on until PLL is locked and status is read

ECG

  en_int2.bit.en_eint       
    ECG FIFO
    set if threshold for evailable samples in FIFO is met
    remains active until the FIFO is read to the extent required by EFIT

  en_int2.bit.en_fstint     
    ECG fast recovery
    Manual or automatic recovery started
    Behavious set by CLR_FAST in MNGR_INT 
  
  en_int2.bit.en_eovf       
    ECG FIFO overflow
    Overflow in the FIFO occured. Remains active until FIFO reset

  en_int2.bit.en_samp       
    Sample synchronization pulse
    To synchronize other external hardware based on ECG sampling instant

  en_int2.bit.en_rrint      
    ECG R to R detection
    Cleared based on CLR_RRINT in MNGR_INT register

ECG & BIOZ

  en_int2.bit.en_dcloffint  
    DC lead off detection
    Remains on until lead on and cleared by status read

  en_int2.bit.en_lonint     
    Ultra low power leads on detection
    EN_ULP_LON in CNFG_GEN determine the condition
    Remains on while leads are on and cleared by status read.

*/

  this->en_int2.all  = this->readRegister24(MAX30001_EN_INT2);

  this->en_int2.bit.int_type      = 2; 
  // 0 disabled, 
  // 1 CMOS, 
  // 2 OpenDrain, 
  // 3 OpenDrain with internal pull up
  
  this->en_int2.bit.en_samp       = 0; // no external hardware needs sampling pulse

  if (ecg) {
    this->en_int2.bit.en_eint       = 1; // ECG FIFO interrupt enable
    this->en_int2.bit.en_fstint     = 1; // ECG fast recovery interrupt enable
    this->en_int2.bit.en_eovf       = 1; // ECG FIFO overflow interrupt enable
    this->en_int2.bit.en_rrint      = 1; // ECG R to R detection interrupt enable
  } else {
    this->en_int2.bit.en_eint       = 0; // ECG FIFO interrupt enable
    this->en_int2.bit.en_fstint     = 0; // ECG fast recovery interrupt enable
    this->en_int2.bit.en_eovf       = 0; // ECG FIFO overflow interrupt enable
    this->en_int2.bit.en_rrint      = 0; // ECG R to R detection interrupt enable
  }

  if (bioz) {
    this->en_int2.bit.en_bint       = 1; // BIOZ FIFO interrupt enable
    this->en_int2.bit.en_bover      = 1; // BIOZ over range interrupt enable
    this->en_int2.bit.en_bovf       = 1; // BIOZ FIFO overflow interrupt enable
    this->en_int2.bit.en_bunder     = fourwire ? 1:0; // BIOZ under range interrupt enable
    this->en_int2.bit.en_pllint     = 0; // PLL unlocked, PLL has lost its phase or is not yet active
  } else {
    this->en_int2.bit.en_bint       = 0; // BIOZ FIFO interrupt enable
    this->en_int2.bit.en_bover      = 0; // BIOZ over range interrupt enable
    this->en_int2.bit.en_bovf       = 0; // BIOZ FIFO overflow interrupt enable
    this->en_int2.bit.en_bunder     = 0; // BIOZ under range interrupt enable
    this->en_int2.bit.en_pllint     = 0; // PLL unlocked, PLL has lost its phase or is not yet active
  }

  this->en_int2.bit.en_lonint     = leadon ? 1:0; // Ultra low power leads on detection interrupt enable

  if (leadoff) {
    this->en_int2.bit.en_dcloffint  = 1; // DC lead off detection interrupt enable
    this->en_int2.bit.en_bcgmon     = fourwire ? 1:0; // BIOZ current monitor
  } else {
    this->en_int2.bit.en_dcloffint  = 0; // DC lead off detection interrupt enable
    this->en_int2.bit.en_bcgmon     = 0; // BIOZ current monitor
  }

  this->writeRegister(MAX30001_EN_INT2, this->en_int2.all);
}

void MAX30001G::setInterruptClearing(){
/*
 * Set defaults for interrupt clearing
 * We will clear automatically or when data register is read
 
 mngr_int_reg.bit.clr_rrint
  Clear RR Interrupt
  0 - on status read
  1 - on R to R read
  2 - self clear after 2-8ms
  3 - do not use
  
 mngr_int_reg.bit.clr_fast
  Fast Mode Clear Behaviour
  0 - clear on read
  1 - clear on read and fast recovery is complete
 
 mngr_int_reg.bit.clr_samp  this->mngr_int_reg.bit.b_fit     = bioz;   // 0-8  BIOZ interrupt treshold

  Sample synchronization pulse clear behaviour
  0 - clear on read
  1 - self clear after 1/4 of data cycle
 
 mngr_int_reg.bit.samp_it
  Sample Synchronization Pulse Frequency
  0 - every sample instant
  1 - every 2nd sample instant
  2 - every 4th sample instant
  3 - every 16th sample instant
 
*/
  
  this->mngr_int_reg.all  = this->readRegister24(MAX30001_MNGR_INT);

  // Interrupt Clearing Behaviour

  this->mngr_int_reg.bit.clr_rrint = 1; // clear on RR register read
  this->mngr_int_reg.bit.clr_fast  = 0; // clear on status register read
  this->mngr_int_reg.bit.clr_samp  = 1; // self clear
  this->mngr_int_reg.bit.samp_it   = 0; // every sample

  this->writeRegister(MAX30001_MNGR_INT, this->mngr_int_reg.all);
}

void MAX30001G::setFIFOInterruptThreshold(uint8_t ecg,  uint8_t bioz) {
/* 
 Number of samples in FIFO that will trigger an interrupt

  ecg  1..32
  bioz 1..8

 mngr_int_reg.bit.e_fit
  ECG interrupt treshold
  value+1 = 1-32 unread samples

 mngr_int_reg.bit.b_fit
  BIOZ interrupt treshold
  value+1 = 1-8 unread samples
*/
  this->mngr_int_reg.all  = this->readRegister24(MAX30001_MNGR_INT);

  if ((bioz <= 8) && (bioz > 0)) {
    this->mngr_int_reg.bit.b_fit = bioz-1; // 1-8  BIOZ interrupt treshold
    LOGln("BIOZ interrupt treshold set to %d - 1", bioz);
  } else {
    LOGE("BIOZ interrupt treshold out of range: 1..8");
  }

  if ((ecg < 32) && (ecg > 0)) {
    this->mngr_int_reg.bit.e_fit = ecg-1; // 1-32  ECG interrupt treshold
    LOGln("ECG interrupt treshold set to %d - 1", ecg);
  } else [
    LOGE("ECG interrupt treshold out of range: 1..32");
  ]

  this->writeRegister(MAX30001_MNGR_INT, this->mngr_int_reg.all);

}

/******************************************************************************************************/
// Initialize
/******************************************************************************************************/

/* ECG Input Stage 
   ---------------
   Hardware:
    MUX connnects input or calibration to AFE
      Calibration Generator
      EMI and ESD protection is built in
      Leads on/off detection
    Instrumentation Amplifier
    Anti aliasing filter
    Programmable Gain Amplifier
    18 bit Sigma Delta ADC

  ESD protection
    +/- 9kV contact discharge
    +/- 15kV air discharge

  DC Lead Off detection
    ECG channel needs to be powered on
    Uses programmable current sources to detect lead off conditions. 
    0,5,10,20,50,100nA are selectable
    if voltage on lead is above or below threshold, lead off is detected 
    VMID +/- 300 (default),400,450, 500 mV options
    To enable DC lead off detection, set EN_DCOFF in CNFG_GEN
    To enable DC lead off interrupt, set threshold to V_MIT +/-300mV, set EN_DCOFF in EN_INT1(default)
    Involved registers: 
      CNFG_GEN and CNFG_EMUX 
      CNFG_EINT or CNFG_ENINT2

    Table 1:
    10nA I_DC any setting of R_BIAS VTH +-300mV

    Setting ON:
      0) ECG Calibration set to off
      1) ECGN and ECGP are connected to VMID
      2) ECG enabled
      3) Current and theshold set
      4) DC lead off detection enabled
      5) Lead off interrupt handling
    Setting OFF:
      0) Lead off interrupt disabled
      1) DC lead off detection disabled

Lead On check (ultra low power)
  Channel needs to be powered off!!

  ECGN is pulled high and ECGP is pulled low with pull up/down resistor, comparator checks if both electrodes are attached

  Involved registers LOINT CNFG_GEN
  Setting ON: 
    currently not supported, would require startup in off conditions and the boot up as soon as lead on is deteted
  Setting OFF: 
    currentlly not supported 

Polarity
  ECGN and ECGP polarity can be switched internally
  ECGN and ECGP can be connected to subject
  Involved registers:
    Setting default: 
      ECGN is connected to VMID, AFE
      ECGP is connected to VMID, AFE
      Calibration is off

Lead Bias to VMID
  Internal or external lead bias can be enabled 500, 100, 200 MOhm to meet common mode range requirements
  We have exteral bias resistor on ??? and we will not enable internal bias
  Invoved registers: ???
  Setting ON: 
    do not enable internal bias
  Default settig: 
    make sure register ??? is set to ??? to enable external bias

Calibration Generator
  +/- 0.25, 0.5 mV uni or bi polar, 1/64 to 256 Hz 

  Involved registers: CNFG_CAL CNFG_EMUX CNFG_ECG
  Make no current flow out of the leads but disconnecting EMUX
  Set On:
    1) ECGN and ECGP are connected to CALN and CALP
    2) ECG enabled
    3) CAL enabled
    4) CAL rate and amplitude set
    5) CALN and CALP are connected to AFE
  Set Off:
    0) ECGN and ECGP are not connected to CALN and CALP ???
    1) CAL disabled
    2) CALN and CALP are disconnected from AFE
  Default waveform
    1) unipolar
    2) 0.5mV
    3) 15mHz to 256Hz
    4) pulsewidth 0.03ms to 62ms or 50% duty cycle

Amplifier
  ECG external filter for differential DC rejection is 10microF resoluting in corner frequency of 0.05Hz allowing best ECG quality but has motion artifacts
  20,40,80,160V/V over all gain selectable

Filters
...

We have external RC on ECG N and ECG P 47nF and 51k.
We have external RC on VCM 47nF and 51k
We might want to exchange external resistor on VCM to 200k
We have external capacitor on CAPN/P of 10uF, therfore our high pass filter is 0.05Hz

*/

void MAX30001G::setupECG(uint8_t speed, uint8_t gain){
/*
 * Initialize AFE for ECG and RtoR detection
 * speed 
 *   0 ~125 sps
 *   1 ~256 sps
 *   2 ~512 sps
 * gain 
 *   0  20 V/V
 *   1  40 V/V
 *   2  80 V/V
 *   3 160 V/V
 */

  // ECG setup
  // ---------

  // ECG sampling rate
  // 0 low 125sps
  // 1 medium 256sps
  // 2 high 512sps
  setECGSamplingRate(speed);

  // ECG low pass and high pass filters
  //   digital lpf for the AFE  
  //   0 bypass ~  0 Hz
  //   1 low    ~ 40 Hz
  //   2 medium ~100 Hz
  //   3 high   ~150 Hz
  // 
  //   digital hpf for the AFE  
  //   0         bypass
  //   1         0.5 Hz 
  //
  // Fixed external analog HPF on C_HPF: 
  //    0.1 uF   5.00 Hz (best motion artifact supression)
  //    1.0 uF   0.50 Hz
  //   10.0 uF   0.05 Hz (MediBrick, highest signal qualtiy)

  switch (speed) {
    case 0:
      setECGfilter(1, 1); // 40Hz and 0.5Hz
      break;
    case 1:
      setECGfilter(2, 1); // 100Hz and 0.5Hz
      break;
    case 2:
      setECGfilter(3, 1); // 150Hz and 0.5Hz
      break;
  } 

  // ECG gain
  //   0  20 V/V
  //   1  40 V/V
  //   2  80 V/V
  //   3 160 V/V
  setECGgain(gain);

  // ECG offset recovery 
  //   set to ~ 98% of max signal
  setECGAutoRecovery(0.98 * this->V_ref / this->ECG_gain);

  // ECG lead polarity
  //   inverted 
  //     0/false: regular
  //     1/true:  swap ECG_N and ECG_P connection to AFE
  //   open 
  //     0/false: connected to AFE
  //     1/true:  isolated, use for internal calibration
  setECGLeadPolarity(false, false);

  // ECG calibration
  setDefaultNoCalibration();

  // R to R
  setDefaultRtoR()

  // Interrupt Clearing Behaviour
  setInterruptClearing();

  // Enable Interrupts
  //   ecg
  //   bioz
  //   leadon
  //   leadoff
  //   fourwire
  // BIOZ undervoltage and lead current monitor only available for 4 wire
  setInterrupt1(true, false, true, true, false);
  setInterrupt2(false, false, false, false, false);

  // ECG, RtoR enable
  // ----------
  enableAFE(true, false, true);

  // These functions need to run after the ECG is enabled
  // -----------------------------------------------------

  // ECG lead bias
  //   ECG bias enabled
  //   BIOZ bias enabled
  //   internal resistance (50,100,200)
  // do not enable if external bias is used on VCM
  setLeadBias(false, false, 100);

  // ECG lead off and lead on detection
  //  Before enabling lead off or lead on:
  //    - Make sure  that ecg or bioz is active.
  //    - Set lead polarity first
  //  Parameters:
  //  - off_detect: Enable/disable lead-off detection.
  //  - on_detect: Enable/disable lead-on detection.
  //  - bioz_4: true = 4 wire, false = 2 wire bioz
  //  - electrode_impedance: Impedance of the electrode, used to determine current magnitude and voltage threshold.
  //      Impedance in MOhm 
  //      0     0        0nA
  //      2  <= 2 MOhm 100nA
  //      4  <= 4 MOhm  50nA
  //      10 <=10 MOhm  20nA
  //      20 <=20 MOhm  10nA
  //      40  >20 MOhm   5nA
  //
  // Porotocentral
  //  lead on off
  //  0 Ohm impedance

  setLeadDetection(true, true, false, 2);
  
  synch(); // synchronize and start the measurements

}

void MAX30001G::setupECGCal(){

/*
 * Initialize AFE for 
 * - ECG 
 * - Internal calibration signal
 * - no RtoR 
 *
 * speed 
 *   0 ~125 sps
 *   1 ~256 sps
 *   2 ~512 sps
 * gain 
 *   0  20 V/V
 *   1  40 V/V
 *   2  80 V/V
 *   3 160 V/V
 */

  // ECG setup
  // ---------

  setECGSamplingRate(speed);
  switch (speed) {
    case 0:
      setECGfilter(1, 1); // 40Hz and 0.5Hz
      break;
    case 1:
      setECGfilter(2, 1); // 100Hz and 0.5Hz
      break;
    case 2:
      setECGfilter(3, 1); // 150Hz and 0.5Hz
      break;
  } 
  setECGgain(gain);

  setECGNormalRecovery(); // No recovery

  setECGLeadPolarity(false, true);

  setDefaultECGCalibration();


  // Interrupt Clearing Behaviour
  setInterruptClearing();

  // Enable Interrupts
  //   ecg
  //   bioz
  //   leadon
  //   leadoff
  //   fourwire
  // BIOZ undervoltage and lead current monitor only available for 4 wire
  setInterrupt1(true, false, false, false, false);
  setInterrupt2(false, false, false, false, false);

  // ECG only enable
  // ----------------
  enableAFE(true,false,false);

  // These functions need to run after the ECG is enabled
  // -----------------------------------------------------

  // ECG lead bias
  //   ECG bias enabled
  //   BIOZ bias enabled
  //   internal resistance (50,100,200)
  // do not enable if external bias is used on VCM
  setLeadBias(false, false, 100);

  // ECG lead off and lead on detection
  //  Before enabling lead off or lead on:
  //    - Make sure  that ecg or bioz is active.
  //    - Set lead polarity first
  //  Parameters:
  //  - off_detect: Enable/disable lead-off detection.
  //  - on_detect: Enable/disable lead-on detection.
  //  - bioz_4: true = 4 wire, false = 2 wire bioz
  //  - electrode_impedance: Impedance of the electrode, used to determine current magnitude and voltage threshold.
  //      Impedance in MOhm 
  //      0     0        0nA
  //      2  <= 2 MOhm 100nA
  //      4  <= 4 MOhm  50nA
  //      10 <=10 MOhm  20nA
  //      20 <=20 MOhm  10nA
  //      40  >20 MOhm   5nA
  //
  // Porotocentral
  //  lead on off
  //  0 Ohm impedance

  setLeadDetection(false, false, false, 0);
  
  synch(); // synchronize and start the measurements

}

void MAX30001G::setupBIOZ(){
/*
 * Enable BIO Impedance Measurement 
 */

 // Set Initial Configuration  
 // FMSTR is set at initialization of the driver to 32768 Hz and not changed later on.

  this->enableAFE(false,true,false);                      // Enable BIOZ readings
  this->setInterrupt1(false, true,  false, false, false); // eanble BIOZ interrupt on interrupt 1
  this->setInterrupt2(false, false, false, false, false); // disable interrupt 2
  this->setInterruptClearing()
  this->setFIFOInteruptThreshold(32,8);                   // trigger interrupt after max values
  this->setBIOZSamplingRate(1);                           // Set BIOZ sampling rate High
  this->setBIOZgain(1, true);                             // Set gain to 20 V/V, enable low noise INA mode
  this->setBIOZfilter(2, 1, 0);                           // Example: AHPF = 500Hz, LPF = 4Hz, HPF = 0. (bypass)
  this->setBIOZmodulation(2);                             // Set BIOZ modulation type to chopped with low pass filter    
  this->setBIOZModulationFrequencybyIndex(3);             // Set BIOZ modulation frequency to 40kHz
  this->setBIOZmag(8000);                                 // Set Current to 8 micro Amps (8000 nano Amps, lowest option for high current, can also use 48uA)
                                                          //   also sets external BIAS resistor when high current mode is selected
  this->setBIOZPhaseOffset(0);                            // Set the phase offset to zero

  BIOZ V_CM resistor, 
     external resistor can be used to drive VCM to commond mode on thrid ECG electrode, We likely will opnly have that if we also use ECG 3 electrodes
     is ther einternal resistor

  BIOZ current monitor
  Lead On/Off 
  BIOZ Calibration OFF
  BIOZ Test Load OFF


  // CONFIG GEN Register Settings
  cnfg_gen.bit.en_ulp_lon   =      0; // ULP Lead-ON Disabled
  //cnfg_gen.bit.fmstr        =   0b00; // 32768 Hz
  //cnfg_gen.bit.en_ecg       =      0; // ECG Enabled
  //cnfg_gen.bit.en_bioz      =      1; // BioZ Enabled
  //cnfg_gen.bit.en_bloff     =      0; // BioZ digital lead off detection disabled
  cnfg_gen.bit.en_dcloff    =      0; // DC Lead-Off Detection Disabled
  cnfg_gen.bit.en_rbias     =   0b00; // RBias disabled
  cnfg_gen.bit.rbiasv       =   0b01; // RBias = 100 Mohm
  cnfg_gen.bit.rbiasp       =   0b00; // RBias Positive Input not connected
  cnfg_gen.bit.rbiasn       =   0b00; // RBias Negative Input not connected
  
  // BioZ Config Settings, 64SPS, current generator 48microA
  //cnfg_bioz.bit.rate        =      0; // 64 SPS
  //cnfg_bioz.bit.ahpf        =  0b010; // 500 Hz

  // cnfg_bioz.bit.ext_rbias   =      0; // internal bias generator used
  //cnfg_bioz.bit.ln_bioz     =      1; // low noise mode
  //cnfg_bioz.bit.gain        =   0b01; // 20 V/V
  //cnfg_bioz.bit.dhpf        =   0b10; // 0.5Hz
  //cnfg_bioz.bit.dlpf        =   0b01; // 4Hz
  //cnfg_bioz.bit.fcgen       = 0b0100; // FMSTR/4 approx 8000z
  cnfg_bioz.bit.cgmon       =      0; // current generator monitor disabled 
  //cnfg_bioz.bit.cgmag       =  0b100; // 48 microA
  // cnfg_bioz.bit.phoff       = 0x0011; // 3*11.25degrees shift 0..168.75 degrees

  // BioZ MUX Settings, connect pins internally to BioZ channel
  cnfg_bmux.bit.openp       =      0; // BIP internally connected to BIOZ channel
  cnfg_bmux.bit.openn       =      0; // BIN internally connected to BIOZ channel
  cnfg_bmux.bit.calp_sel    =   0b00; // No cal signal on BioZ P
  cnfg_bmux.bit.caln_sel    =   0b00; // No cal signal on BioZ N
  cnfg_bmux.bit.cg_mode     =   0b00; // Unchopped with higher noise and excellent 50/60Hz rejection
  cnfg_bmux.bit.en_bist     =      0; // Modulated internal resistance self test disabled
  cnfg_bmux.bit.rnom        =  0b000; // Nominal resistance selection, nominal 5000 Ohm for internal test
  cnfg_bmux.bit.rmod        =  0b100; // No modulation for internal test
  cnfg_bmux.bit.fbist       =   0b00; // FMSTR/2^13 approx 4Hz for internal test

  // Calibration Settings
  cnfg_cal.bit.thigh        = 0b00000000000; // thigh x CAL_RES
  cnfg_cal.bit.fifty        =      0; // Use thigh to select time high for VCALP and VCALN
  cnfg_cal.bit.fcal         =  0b000; // FMSTR/128
  cnfg_cal.bit.vmag         =      1; // 0.5mV
  cnfg_cal.bit.vmode        =      1; // bipolar
  cnfg_cal.bit.vcal         =      1; // calibration sources and modes enabled

  // BioZ LC Settings, turn off low current mode
  cnfg_bioz_lc.bit.cmag_lc  = 0b0000; // 0 nA, turn OFF low current mode
  cnfg_bioz_lc.bit.cmres    = 0b0000; // common mode feedback resistance is off
  cnfg_bioz_lc.bit.bistr    =   0b00; // 27kOhm programmable load value selection
  cnfg_bioz_lc.bit.en_bistr =      0; // disable programmable high resistance load
  cnfg_bioz_lc.bit.lc2x     =      0; // 1x low drive current 55nA to 550nA
  cnfg_bioz_lc.bit.hi_lob   =      1; // bioz drive current is low 55nA to 1100nA

  en_int.bit.intb_type =    0b01; // open drain
  en_int.bit.en_pllint =     0b0; // PLL interrupt disabled
  en_int.bit.en_samp =       0b0; // sample interrupt disabled
  en_int.bit.en_rrint =      0b0; // R to R interrupt enabled
  en_int.bit.en_lonint =     0b0; // lead off interrupt disabled
  en_int.bit.en_bcgmon =     0b0; // BIOZ current generator monitor interrupt disabled
  en_int.bit.en_bunder =     0b0; // BIOZ undervoltage interrupt disabled
  en_int.bit.en_bover =      0b0; // BIOZ overcurrent interrupt disabled
  en_int.bit.en_bovf =       0b0; // BIOZ overvoltage interrupt disabled
  en_int.bit.en_bint =       0b1; // BIOZ FIFO interrupt 
  en_int.bit.en_dcloff =     0b0; // ECG lead off detection interrupt disabled
  en_int.bit.en_eovf =       0b0; // ECG overflow detection interrupt disabled
  en_int.bit.en_eint =       0b0; // ECG FIFO interrupt enabled

  swReset();
  delay(100);

  this->writeRegister(MAX30001_CNFG_GEN, cnfg_gen.all);
  delay(100);
  wirteRegister(CNFG_CAL, cnfg_cal.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_BIOZ, cnfg_bioz.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_BIOZ_LC, 
  cnfg_bioz_lc.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_BMUX, cnfg_bmux.all);
  delay(100);
  this->writeRegister(MAX30001_EN_INT1,    en_int.all);
  delay(100);
  synch();
  delay(100);
        
} // initialize driver for BIOZ

void MAX30001G::setupBIOZcalibrationInternal(){
/*
 Enable BIO Impedance Calibration
*/
}

void MAX30001G::setupBIOZcalibrationExternal(){
/*
 Enable BIO Impedance Measurement on External 100 Ohm resistor
*/
}

void MAX30001G::setupECGandBIOZ() {
/*
 Enable BIO Impedance and ECG measurement
*/

  max30001_cnfg_gen_t     cnfg_gen;
  max30001_cnfg_emux_t    cnfg_emux;
  max30001_cnfg_ecg_t     cnfg_ecg;

  max30001_cnfg_bmux_t    cnfg_bmux;
  max30001_cnfg_bioz_t    cnfg_bioz;
  max30001_cnfg_cal_t     cnfg_cal;

  max30001_cnfg_bioz_lc_t cnfg_bioz_lc;
  max30001_en_int_t       en_int;
  max30001_mngr_int_t     mngr_int;   

  max30001_cnfg_rtor1_t cnfg_rtor1;  // R to R detection
  max30001_cnfg_rtor2_t cnfg_rtor2;  // R to R detection

  // CONFIG GEN Register Settings
  cnfg_gen.bit.rbiasn       =     0b0; // RBias Negative Input not connected
  cnfg_gen.bit.rbiasp       =     0b0; // RBias Positive Input not connected
  cnfg_gen.bit.rbiasv       =    0b01; // RBias = 100 Mohm
  cnfg_gen.bit.en_rbias     =    0b00; // RBias disabled
  cnfg_gen.bit.vth          =    0b00; // lead off threshold +/-300mV
  cnfg_gen.bit.imag         =   0b000; // lead off magnitude 0nA
  cnfg_gen.bit.ipol         =     0b0; // lead off current polarity
  cnfg.gen.bit.en_dcloff    =    0b00; // DC lead off detection enabled
  cnfg_gen.bit.en_bloff     =    0b00; // BioZ digital lead off detection disabled
  cnfg_gen.bit.en_pace      =     0b0; // pace detection disabled
  cnfg_gen.bit.en_bioz      =     0b1; // BioZ Enabled
  cnfg_gen.bit.en_ecg       =     0b1; // ECG Enabled
  cnfg_gen.bit.fmstr        =    0b00; // 32768 Hz
  cnfg_gen.bit.en_ulp_lon   =     0b0; // ULP Lead-ON Disabled
  
  // ECG Config Settings
  cnfg_ecg.bit.rate         =    0b10; // Default, 128SPS
  cnfg_ecg.bit.gain         =    0b10; // 80 V/V
  cnfg_ecg.bit.dhpf         =       1; // 0.5Hz
  cnfg_ecg.bit.dlpf         =    0b01; // 40Hz
  
  // ECG MUX Settings
  cnfg_emux.bit.openp       =       0; // ECGP connected to ECGP AFE Channel
  cnfg_emux.bit.openn       =       0; // ECGN connected to ECGN AFE Channel
  cnfg_emux.bit.pol         =       0; // Non inverted polarity
  cnfg_emux.bit.calp_sel    =       0; // No calibration signal on ECG_P
  cnfg_emux.bit.caln_sel    =       0; // No calibration signal on ECG_N

  // BioZ Config Settings, 64SPS, current generator 48microA
  cnfg_bioz.bit.rate        =       0; // 64 SPS
  cnfg_bioz.bit.ahpf        =   0b010; // 500 Hz
  cnfg_bioz.bit.ext_rbias   =       0; // internal bias generator used
  cnfg_bioz.bit.ln_bioz     =       1; // low noise mode
  cnfg_bioz.bit.gain        =    0b01; // 20 V/V
  cnfg_bioz.bit.dhpf        =    0b10; // 0.5Hz
  cnfg_bioz.bit.dlpf        =    0b01; // 4Hz
  cnfg_bioz.bit.fcgen       =  0b0100; // FMSTR/4 approx 8000z
  cnfg_bioz.bit.cgmon       =     0b0; // current generator monitor disabled 
  cnfg_bioz.bit.cgmag       =   0b100; // 48 microA
  cnfg_bioz.bit.phoff       =  0x0011; // 3*11.25degrees shift 0..168.75 degrees

  // BioZ MUX Settings, connect pins internally to BioZ channel
  cnfg_bmux.bit.openp       =       0; // BIP internally connected to BIOZ channel
  cnfg_bmux.bit.openn       =       0; // BIN internally connected to BIOZ channel
  cnfg_bmux.bit.calp_sel    =    0b00; // No cal signal on BioZ P
  cnfg_bmux.bit.caln_sel    =    0b00; // No cal signal on BioZ N
  cnfg_bmux.bit.cg_mode     =    0b00; // Unchopped with higher noise and excellent 50/60Hz rejection
  cnfg_bmux.bit.en_bist     =       0; // Modulated internal resistance self test disabled
  cnfg_bmux.bit.rnom        =   0b000; // Nominal resistance selection, nominal 5000 Ohm for internal test
  cnfg_bmux.bit.rmod        =   0b100; // No modulation for internal test
  cnfg_bmux.bit.fbist       =    0b00; // FMSTR/2^13 approx 4Hz for internal test

  // Calibration Settings
  cnfg_cal.bit.thigh        = 0b00000000000; // thigh x CAL_RES
  cnfg_cal.bit.fifty        =       0; // Use thigh to select time high for VCALP and VCALN
  cnfg_cal.bit.fcal         =   0b000; // FMSTR/128
  cnfg_cal.bit.vmag         =       1; // 0.5mV
  cnfg_cal.bit.vmode        =       1; // bipolar
  cnfg_cal.bit.vcal         =       1; // calibration sources and modes enabled

  // BioZ LC Settings, turn off low current mode
  cnfg_bioz_lc.bit.cmag_lc  =  0b0000; // 0 nA, turn OFF low current mode
  cnfg_bioz_lc.bit.cmres    =  0b0000; // common mode feedback resistance is off
  cnfg_bioz_lc.bit.bistr    =    0b00; // 27kOhm programmable load value selection
  cnfg_bioz_lc.bit.en_bistr =       0; // disable programmable high resistance load
  cnfg_bioz_lc.bit.lc2x     =       0; // 1x low drive current 55nA to 550nA
  cnfg_bioz_lc.bit.hi_lob   =       1; // bioz drive current is low 55nA to 1100nA

  // Interrupt Settings
  en_int.bit.intb_type      =    0b01; // open drain
  en_int.bit.en_pllint      =     0b0; // PLL interrupt disabled
  en_int.bit.en_samp        =     0b0; // sample interrupt disabled
  en_int.bit.en_rrint       =     0b0; // R to R interrupt enabled
  en_int.bit.en_lonint      =     0b0; // lead off interrupt disabled
  en_int.bit.en_bcgmon      =     0b0; // BIOZ current generator monitor interrupt disabled
  en_int.bit.en_bunder      =     0b0; // BIOZ undervoltage interrupt disabled
  en_int.bit.en_bover       =     0b0; // BIOZ overcurrent interrupt disabled
  en_int.bit.en_bovf        =     0b0; // BIOZ overvoltage interrupt disabled
  en_int.bit.en_bint        =     0b1; // BIOZ FIFO interrupt 
  en_int.bit.en_dcloff      =     0b0; // ECG lead off detection interrupt disabled
  en_int.bit.en_eovf        =     0b0; // ECG overflow detection interrupt disabled
  en_int.bit.en_eint        =     0b1; // ECG FIFO interrupt enabled

  // Interrupt clearing
  mngr_int.bit.samp_it      =    0b00; //
  mngr_int.bit.clr_samp     =     0b0; //
  mngr_int.bit.clr_pedge    =     0b0; //
  mngr_int.bit.clr_rrint    =    0b00; // RtoR clear
  mngr_int.bit.clr_fast     =     0b0; // fast mode interrupt clear
  mngr_int.bit.b_fit        =   0b011; // BIOZ interrupt threshold
  mngr_int.bit.e_fit        = 0b01111; // ECG interrupt threshold

  // CNFG_RTOR1
  cnfg_rtor1.bit.ptsf       =  0b0110; // (0b0110 + 1)/16
  cnfg_rtor1.bit.pavg       =    0b00; // 2
  cnfg_rtor1.bit.en_rtor    =     0b1; // R to R detection enabled
  cnfg_rtor1.bit.gain       =  0b1111; // autoscale
  cnfg_rtor1.bit.wndw       =  0b0011; // 12 x RTOR resolution 

  // CNFG_RTOR2
  cnfg_rtor2.bit.rhsf       =    0b100; // 4/8 
  cnfg_rtor2.bit.ravg       =     0b10; // 4 
  cnfg_rtor2.bit.hoff       = 0b100000; // 32*T_RTOR

  swReset();
  delay(100);
  this->writeRegister(MAX30001_CNFG_GEN, cnfg_gen.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_CAL, cnfg_cal.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_ECG, cnfg_ecg.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_EMUX, cnfg_emux.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_BIOZ, cnfg_bioz.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_BIOZ_LC, cnfg_bioz_lc.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_BMUX, cnfg_bmux.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_RTOR1, cnfg_rtor1.all);
  delay(100);
  this->writeRegister(MAX30001_CNFG_RTOR2, cnfg_rtor2.all);
  delay(100);
  this->writeRegister(MAX30001_EN_INT1, en_int.all);
  delay(100);
  this->writeRegister(MAX30001_MNGR_INT, mngr_int.all);
  delay(100);
  synch();
  delay(100);

} // end initialize driver for ECG and BIOZ

void MAX30001G::scanBIOZ() {
/* 
 * Measures BIOZ by scanning over all valid frequencies and phase offsets
 * 
 * Measurements start a 8 micro Ampere and current is decreased if signal is satured
 * or increased if signal is below 10% of the ADC range.
 * Since low frequencies have a reduced current range, current adjusment is limited. 
 *
 * We measure at 8 frequencies and 16 phase offsets
 * We will not measure below 1kHz to enable sufficient 60Hz noise suppression by ananlog high pass filter
 * 
 * After completed measurements, magnitude and phase are extracted at each frequency
 * 
 * Result will be placed in 
 *   this->impedance_magnitude
 *   this->impedance_phase
 *   this->impedance_frequency
 * 
 * Magnitude and phase will be 0.0 if measurement was unsuccessful.
 */  

  // high current
  // ---------------------------------------------------------------------
  // BioZ Current Generator Magnitude: 
  // cnfg_bioz.cgmag
  // 000 = Off (DRVP and DRVN floating, Current Generators Off)
  // 001 = 8µA (also use this setting when BIOZ_HI_LOB = 0)
  // 010 = 16µA
  // 011 = 32µA
  // 100 = 48µA
  // 101 = 64µA
  // 110 = 80µA
  // 111 = 96µA
  //
  // The following currents can be achied based on the selected modulation frequency: 

  // BIOZ CURRENT GENERATOR MODULATION FREQUENCY (Hz)                                     CNFG_BIOZ
  // BIOZ_FCGEN[3:0]  FMSTR[1:0] = 00 FMSTR[1:0] = 01  FMSTR[1:0] = 10  FMSTR[1:0] = 11   CGMAG allowed
  //                  fMSTR= 32,768Hz fMSTR = 32,000Hz fMSTR = 32,000Hz fMSTR = 31,968Hz  
  // 0000             131,072         128,000          128,0001        127,872            all
  // 0001              81,920          80,000           80,000          81,920            all
  // 0010              40,960          40,000           40,000          40,960            all
  // 0011              18,204          17,780           17,780          18,204            all
  // 0100               8,192           8,000            8,000           7,992            not 111
  // 0101               4,096           4,000            4,000           3,996            000 to 011
  // 0110               2,048           2,000            2,000           1,998            000 to 010
  // 0111               1,024           1,000            1,000             999            000, 001
  // 1000                 512             500              500             500            000, 001
  // 1001                 256             250              250             250            000, 001
  // 101x,11xx            128             125              125             125            000, 001

  // low current
  // ---------------------------------------------------------------------
  //   cmag == 001 
  //   cnfg_bioz_lc.bit.hi_lob == 0 
  //   cnfg_bioz_lc.bit.lc2x == 0 55-1100nA
  //   cnfg_bioz_lc.bit.lc2x == 1 110-1100nA
  //   set the common mode resistance as recommended in BIOZ_CMRES.
  //
  // BIOZ Low Current Generator Magnitude: 
  // cnfg_bioz_lc.bit.cmag_lc
  //         LC2X = 0 LC2X = 1
  // 0000    0        0
  // 0001    55nA     110nA
  // 0010    110nA    220nA
  // 0011    220nA    440nA
  // 0100    330nA    660nA
  // 0101    440nA    880nA
  // 0110    550nA    1100nA

  float frequency[8];      // Array for modulation frequencies
  int32_t current[8];      // Array for current used at each frequency
  float impedance[8][16];  // 2D array for impedance magnitudes
  float phase[8][16];      // 2D array for phase values

  int32_t threshold_min = 524288 * 0.1; // Minimum magnitude threshold to trigger current increase
  int32_t threshold_max = 524288 * 0.9; // Maximum magnitude threshold to trigger current decrease
  int32_t adc_target    = 524288 * 0.6; // Desired ADC signal level

  uint8_t num_points = 0; // number of phase offsets available

  // Initialize arrays
  for (int i = 0; i < 11; i++) {
    for (int j = 0; j < 16; j++) {
      impedance[i][j] = NAN; // unmeasured, set to not a finite number
      phase[i][j] = -1.0;    // unmeasured, set to invalid phase
    }
    frequency[i] = -1.0; // unmeasured, set to invalid frequency
    current[i] = 8000;   // 8 microAmpere, set to default current
  }

  // Set Initial Configuration  
  this->setInterrupt1(false, true,  false, false, false); // eanble BIOZ interrupt on interrupt 1
  this->setInterrupt2(false, false, false, false, false); // disable interrupt 2
  this->setFIFOInteruptThreshold(1,1);                    // trigger interrupt after one value
  this->setBIOZSamplingRate(1);                           // Set BIOZ sampling rate High
  this->setBIOZgain(1, true);                             // Set gain to 20 V/V, enable low noise INA mode
  this->setBIOZfilter(2, 1, 0);                           // Example: AHPF = 500Hz, LPF = 4Hz, HPF = 0. (bypass)
  this->setBIOZmodulation(2);                             // Set BIOZ modulation type to chopped with low pass filter    

  // In order to measure BIOZ at 125Hz analog highpass should be set to 60Hz 
  //   however that will introduce 60Hz noise from AC main into the BIOZ signal. 
  // The default setting for the analog highpass filter is 500Hz cut on. 
  // If we choose the following cut on frequencies 60Hz suppression will be:
  //  60Hz cut on: 60Hz is supressed by -3dB  or reduced to 70%
  // 150Hz cut on: 60Hz is supressed by -10dB or reduced to 30%
  // 500Hz cut on: 60Hz is supressed by -20dB or reduced to 10%
  // Therefore to have some 60Hz supression we can not measure below 1kHz.
  //
  // If 60Hz noise is making it on to the BIOZ signal, after demodulation that noise will appear
  //   at 60Hz and also at modulation_frequency +/- 60Hz. The digial low pass filter will 
  //   remove that noise but only if the 60Hz noise is significanlty lower in power than 
  //   the BIOZ signal and not saturating the BIOZ channel.
  //
  // At later time we can experiement with 60Hz or 0Hz analog high pass and verify if we have same
  // readings at high modulation frequencies.

  // Iterate over all valid frequencies (0b0000 to 0b0111)
  // 128kHz to 1kHz
  for (uint8_t freq = 0; freq <= 0b0111; freq++) {
    float max_magnitude = 0.0;

    this->setBIOZModulationFrequencybyIndex(freq); // Set BIOZ modulation frequency
    frequency[freq] = this->BIOZ_frequency ;       // Update frequency 

    if freq == 0b0000 {
      num_points = 4; // 4 offsets, 128kHz
    } else if (freq == 0b0001) {
      num_points = 8; // 8 offsets, 80kHz
    } else if (freq == 0b0010) {
      num_points = 16; // 16 offsets 40kHz and lower
    }

    // First Pass
    // ----------------------------------------------------------------
    // Handels saturation

    do {
      bool phaseSweepRestart = false;

      this->setBIOZmag(current[freq]); // Set Current
      current[freq] = this->BIOZ_cgmag; // Update current based on the actual set value

      // Iterate over all 16 phase offsets (0 to 15)
      for (uint8_t phase_selector = 0; phase_selector < num_points; phase_selector++) {
        this->setBIOZPhaseOffset(phase_selector);  // Set the phase offset
        this->enableAFE(false,true,false);         // Enable BIOZ readings
        this->synch();                             // Synchronize the AFE clock
        this->FIFOReset();                         // Reset the FIFO
        this->BIOZ_available = false;        // Reset BIOZ_available

        // Poll until BIOX data is available
        while (!this->BIOZ_available) {
          delay(1);  // Short delay to prevent blocking
        }
        this->BIOZ_available = false; // Reset the flag

        int32_t mag = int32_t( this->readBIOZ_FIFO(true) );  // Read BIOZ FIFO data in raw ADC mode

        this->enableAFE(false,false,false);         // Disable BIOZ readings after the data is available

        // Error checking
        if (!this->EOF_data_detected)     {LOGE("Should have received End of FIFO tag.");}
        if (this->over_voltage_detected)  {LOGE("Over-voltage detected.");}
        if (this->under_voltage_detected) {LOGE("Under-voltage detected.");}

        if (this->valid_data_detected) {

          if (abs(mag) > threshold_max) {
            // Stop sweep and reduce current, restart the phase sweep
            current[freq] = closestCurrent(current[freq] / 2);
            phaseSweepRestart = true;
            break; // Exit phase loop to restart with adjusted current
          } 

          // Store the impedance magnitude and phase
          impedance[freq][phase_selector] = mag;
          phase[freq][phase_selector] = this->BIOZ_phase;

          // Track maximum magnitude for this frequency
          if (abs(mag) > max_magnitude) {
            max_magnitude = abs(mag);
          }

          LOGD("Frequency: %.2f Hz, Phase: %.2f degrees, Impedance: %.1f, Current: %d", 
                frequency[freq], phase[freq][phase_selector], impedance[freq][phase_selector], current[freq]);
        } else { 
          // no valid data
          impedance[freq][phase_selector] = NAN;
          phase[freq][phase_selector] = this->BIOZ_phase;
          LOGE("No valid data detected.");
        }
      } // end of phase loop
    } while(phaseSweepRestart);

    // Second Pass
    // ----------------------------------------------------------------
    // Handels low magnitude

    if (max_magnitude == 0.0) { LOGE("Max impedance magnitude is zero."); }

    bool remeasured = false;

    // Remasure with more current if the magnitude is too low
    if ((max_magnitude < threshold_min) && (max_magnitude > 0.0)) {
      // Increase the current
      int32_t current_temp = current[freq]; // previous current

      current[freq] = closestCurrent((current[freq] * adc_target) / max_magnitude); // desired new current
      setBIOZmag(current[freq]); // Set new current
      current[freq] = this->BIOZ_cgmag; // Update current with what we were able to set

      if curren[freq] != current_temp {
        // There is valid new current setting, therefore remeasure
        remeasured = true;
        // Measure over all phases
        for (uint8_t phase_selector = 0; phase_selector < num_points; phase_selector++) {
          this->setBIOZPhaseOffset(phase_selector);
          this->enableAFE(false,true,false);
          this->synch(); // Synchronize the AFE clock
          this->FIFOReset(); // Reset the FIFO
          this->BIOZ_available = false; // there should be no value in the FIFO

          while (!this->BIOZ_available) {
            delay(1);  // Short delay to prevent blocking
          }
          this->BIOZ_available = false; // Reset the flag

          impedance[freq][phase_selector] = readBIOZ_FIFO(false); // Read calibrated data
          phase[freq][phase_selector] = this->BIOZ_phase;

          this->enableAFE(false,false,false);

          LOGD("Frequency: %.2f Hz, Phase: %.2f degrees, Impedance: %.2f Ohm, Current: %d nA", 
                frequency[freq], phase[freq][phase_selector], impedance[freq][phase_selector], current[freq]);

          if (!this->EOF_data_detected)     {LOGE("Should have received End of FIFO tag.");}
          if (this->over_voltage_detected)  {LOGE("Over-voltage detected.");}
          if (this->under_voltage_detected) {LOGE("Under-voltage detected.");}
          if (!this->valid_data_detected)   {LOGE("No valid data detected.");}
        } // end of phase loop
      } // end of remeasure
    } // end of adjusted current measurement 

    if (remeasured == false) {
      // We did not remeasure
      // Calibrate previous readings and store the impedance magnitudes and phases
      for (uint8_t phase_selector = 0; phase_selector < num_points; phase_selector++) {
        impedance[freq][phase_selector] = (impedance[freq][phase_selector] * this->V_ref * 1e9) / float(524288 * this->BIOZ_cgmag * this->BIOZ_gain);
        phase[freq][phase_selector] = this->BIOZ_phase;  // Phase set by setBIOZPhaseOffset
        LOGD("Frequency: %.2f Hz, Phase: %.2f degrees, Impedance: %.2f Ohm, Current: %d nA", 
              frequency[freq], phase[freq][phase_selector], impedance[freq][phase_selector], current[freq]);
      }
    }
      
  } // end of frequency loop
  
  // Fit the measurements to magnitude and phase
  // Iterate over all valid frequencies (0b0000 to 0b0111)
  for (uint8_t freq = 0; freq <= 0b0111; freq++) {

    if freq == 0b0000 {
      num_points = 4; // 4 offsets
    } else if (freq == 0b0001) {
      num_points = 8; // 9 offsets
    } else if (freq == 0b0010) {
      num_points = 16; // 16 offsets
    }

    ImpedanceModel result = fitImpedance(phase[freq][0], impedance[freq][0], num_points);
    this->impedance_magnitude[freq] = result.magnitude;
    this->impedance_phase[freq] = result.phase;
    this->impedance_frequency[freq] = frequency[freq];
  }

}

int32_t closestCurrent(int32_t input) {
// Closest current calculation using a fixed array
  // The list of selectable values sorted in ascending order
  int32_t currentValues[] = {0, 55, 110, 220, 330, 440, 550, 660, 880, 1100, 8000, 16000, 32000, 48000, 64000, 80000, 96000};
  int size = sizeof(currentValues) / sizeof(currentValues[0]);

  // If the input is smaller than or equal to the smallest element, return 0
  if (input <= currentValues[0]) {
    return currentValues[0];
  }

  // Iterate over the array to find the closest smaller or equal value
  for (int i = 0; i < size; i++) {
    if (currentValues[i] >= input) {
      return currentValues[i - 1];
    }
  }

  // If input is larger than the largest value, return the largest value
  return currentValues[size - 1];
}
  
ImpedanceModel fitImpedance(const float* phase_offsets, const float* measurements, int num_points) {
/*
 * Fit the measured impedance data (measurements, phase_offsetes) to a model of the form:
 * 
 * Z = A * cos(theta) + B * sin(theta)
 * 
 * Measurements represent: V/I = |Z| cos(theta - phase_offset) and are in Ohms
 * 
 * V:             voltage measured by the AFE (BIOZ signal)
 * I:             current set for the AFE current generator
 * phase_offset:  phase offset of the demodulator compared to the current generator 
 * Z:             impedance
 * |Z|:           magntidue of impedance
 * theta:         phase
 * A:             resistance
 * B:             reactance
 * 
 * The function returns the magnitude (|Z|) and phase (theta) of the impedance model via a last squares approach.
 * 
 * This function handles all impedance measurement approaches:
 * - 2 phase offset of 0 and 90 degrees (quadrature demodulation) 
 *     [sin_sin, cos_cos = 1, cos_sin, sin_cos = 0]
 * - phase offsets at uniform intervals covering 0 to 360 degrees 
 *     [cos_sin, sin_cos = 0]
 * - phase offsets at arbitrary intervals
 * 
 * Urs Utzinger and ChatGPT o1 preview
*/
    // Initialize sums for least squares
    float sum_cos_cos = 0.0f;
    float sum_sin_sin = 0.0f;
    float sum_cos_sin = 0.0f;
    float sum_measurements_cos = 0.0f;
    float sum_measurements_sin = 0.0f;
    int num_valid_points = 0;
    
    for (int i = 0; i < num_points; ++i) {
        if (isfinite(measurements[i])) {
            num_valid_points++;
            float cos_theta = cos(phase_offsets[i] * PI / 180.0f);
            float sin_theta = sin(phase_offsets[i] * PI / 180.0f);
            sum_cos_cos += cos_theta * cos_theta;
            sum_sin_sin += sin_theta * sin_theta;
            sum_cos_sin += cos_theta * sin_theta;
            sum_measurements_cos += measurements[i] * cos_theta;
            sum_measurements_sin += measurements[i] * sin_theta;
        }
    }
    
    if (num_valid_points == 0) {
        return {0.0f, 0.0f};  // Return default values if no valid points
    }
    
    // Compute the determinant
    float denom = sum_cos_cos * sum_sin_sin - sum_cos_sin * sum_cos_sin;
    if (denom == 0.0f) {
        // Handle singularity (e.g., insufficient variation in phase offsets)
        return {0.0f, 0.0f};
    }
    
    // Solve for A and B
    float A = (sum_measurements_cos * sum_sin_sin - sum_measurements_sin * sum_cos_sin) / denom;
    float B = (sum_measurements_sin * sum_cos_cos - sum_measurements_cos * sum_cos_sin) / denom;
    
    // Compute magnitude and phase
    float magnitude = sqrt(A * A + B * B);
    float phase = atan2(B, A);  // Phase in radians
    
    ImpedanceModel result = {magnitude, phase};
    return result;
}

/******************************************************************************************************/
/******************************************************************************************************/
// Change Settings
/******************************************************************************************************/
/******************************************************************************************************/

/*

The MAX30001 Evaluation Kit User Interface has the following options:

  BOTH ECG and BIOZ Tab
  ======================
  Master Clock FMSTR
  ECG  enable
  BIOZ enable
  RtoR enable

  ECG Channel Tab
  ===============
  EFIT number of samples until interrupt
    setFIFOInteruptThreshold(uint8_t  ECG, uint8_t  BIOZ)

  ECG sampling rate
    void MAX30001G::setECGSamplingRate(uint8_t  ECG)

  ECG digital low pass and high pass filters
    void MAX30001G::setECGfilter(uint8_t lpf, uint8_t hpf)
    The allowable values will be applied
    ECG_lpf and ECG_hpf have frequency

  ECG channel gain
    void MAX30001G::setECGgain(uint8_t gain)
    Set the gain stage
    ECG_gain global variable is updated  Application:

  ECG offset recovery
      setECGNormalRecovery
      setECGAutoRecovery
      startECGManualRecovery
      stopECGManualRecovery
    fast receiver normal 
    fast recovery threshold

  RtoR
    Gain: Autoscale
    Window averaging with: 12*8ms
    Minimum hold off 32
    Peak averaging weight factor 8
    Peak threshold scaling factor 4/16
    Interval hold off scaling factor 4/8
    Interval averaging factor 8

  ECG MUX Tab
  ===========

  ECG Lead Off Detection
    Enable/Disable
    Polarity Pullup/down
    Current magnitude nA
    Voltage threshold: vmid +- 300mV

  ECG ULP Lead On Detection
    enable/disable

  ECG Lead Polarity
    Isolate
    Inverted/NonInverted

  ECG Lead Bias
    BIOZ bias enable/disable
    Bias resistance 100MOhm
    Positive input bias enable
    Negative input bias enable

  ECG Calibration
    ECGN
    ECGP 
    Enable/Disable
    Unipolar/Bipolar
    Voltage 0.25mV
    Freqeuncy
    Dutycyle or Time High

  BIOZ Channel Tab
  ================
  - BIOZ analog highpass and digital low and high pass filter
    A HP 60Hz
    LPF 4Hz
    HPF Bypass

  - BIOZ INA power mode
    low/high

  - BIOZ Phase Offset
    0-160 degrees

  - BIOZ gain
    void MAX30001G::setBIOZgain(uint8_t gain)
    20V/V

  - BIOZ sample rate
    void MAX30001G::setBIOZSamplingRate(uint8_t  BIOZ)
    32sps
 
  BIOZ external bias resistor 
    Internal / External

  BIOZ modulation frequency
    void MAX30001G::setBIOZmodulationfrequency(uint16_t frequency)    

  - BIOZ modulation mode
    choppped with resistive CM

  BIOZ CM Resistor
     40MOhm

  BIOZ Current Monitor
     Enable / Disable

  - BIOZ current
    void MAX30001G::setBIOZmag(uint32_t current)
    low 55-1100nA
    low 1x or 2X 
    low magnitude
    high mangitude

    adjust BIOZ current feedback resistor depening on low or high current mode

  BIOZ digital lead off
    checks measurement values
    enable/disable
    AC over range threshold +/- 2048*255
    AC under range threshold +/- 32*255

  BIOZ MUX Tab
  ============
  This is the same as ECG MUX for leadon/leadoff

  BIOZ analog lead off
    measures voltage
    enable/disable
    current polairyt pullup/down
    currnt magnitude nA
    votlage threshold VMID+/-300mV

  BIOZ analog lead on
    enable/disable

  BIOZ connected
    connected/isolated
    resitive load
  
  BIOZ lead bias
    resitive bias value 100MOhm
    positive/negative enable/dsiable

  BIOZ calibration
    BIP/BIN calibration mode enable/disable
    enable/disable
    mode unipolar
    voltage 0.25mV
    frequency fmstr/2^15
    duty cyloe or time high

  BIOZ Load Tab
  =============
  BIOZ builitin resistance test
    BIST enable/disable
    resitance 5000 Ohm
    modulate resistance on/off1
    modutale resistance frequency

*/

/******************************************************************************************************/
// ECG
/******************************************************************************************************/

void MAX30001G::setECGSamplingRate(uint8_t  ECG) {
  /*
  Set the ECG  sampling rate for the AFE, 0=low, 1=medium, 2=high
  approx [0] 125, [1] 256, [2] 512 sps
  Based on FMSTR, adjusts global variable ECG_samplingRate
  */

  this->cnfg_ecg.all  = this->readRegister24(MAX30001_CNFG_ECG);

  switch (ECG) {
    case 0:
      this->cnfg_ecg.bit.rate = 0b10; // low
      break;
    case 1:
      this->cnfg_ecg.bit.rate = 0b01; // medium
      break;
    case 2:
      this->cnfg_ecg.bit.rate = 0b00; // high
      break;
    default:
      this->cnfg_ecg.bit.rate = 0b10; // low
      break;
  }

  this->writeRegister(MAX30001_CNFG_ECG, this->cnfg_ecg.all);
  delay(100);
  updateGlobalECG_samplingRate();

}

void MAX30001G::setECGfilter(uint8_t lpf, uint8_t hpf) {
/*
  Set the ECG lpf for the AFE  
    0 bypass
    1 low
    2 medium
    3 high

  Set the ECG hpf for the AFE  
    0 bypass
    1 0.5Hz 

  Fixed HPF on C_HPF, 0.1 1.0 10uF for <=5 0.5 0.05Hz
  5Hz lowest signal quality but best motion artifact supression
  MediBrick uses 10uF
*/

  this->cnfg_ecg.all = this->readRegister24(MAX30001_CNFG_ECG);
  this->cnfg_gen.all = this->readRegister24(MAX30001_CNFG_GEN);

  // Digital High Pass
  if hpf == 0:
    this->ECG_hpf = 0.;
    this->cnfg_ecg.dhpf = 0; // bypass
  else:
    this->ECG_hpf = 0.5;
    this->cnfg_ecg.dhpf = 0; // 0.5Hz

  // Digital Low Pass
  switch (lpf) {
    case 0:
      this->cnfg_ecg.bit.dlpf = 0b00; // bypass
      break;
    case 1:
      this->cnfg_ecg.bit.dlpf = 0b01; // low
      break;
    case 2:
      this->cnfg_ecg.bit.dlpf = 0b10; // medium
      break;
    case 3:
      this->cnfg_ecg.bit.dlpf = 0b11; // high
      break;
    default:
      this->cnfg_ecg.bit.dlpf = 0b01; // low
      break;
  }
  this->writeRegister(MAX30001_CNFG_ECG, this->cnfg_ecg.all);
  delay(100);

  updateGlobalECG_lpf();

}

void MAX30001G::setECGgain(uint8_t gain) {
/*
  Set the ECG gain for the AFE
    0 20V/V
    1 40V/V
    2 80V/V
    3 160V/V

    max DC differential is +-650mV
    max AC differential is +/- 32mV
    usually ECG is smaller than 3mV

    V_ECG = ADC * V_REF / (2^17 * ECG_GAIN)
    V_REF is typ 1000mV

*/

  if (gain <=3) {
    this->cnfg_ecg.all = this->readRegister24(MAX30001_CNFG_ECG);
    this->cnfg_ecg.bit.gain = gain;
    this->writeRegister(MAX30001_CNFG_ECG, this->cnfg_ecg.all);
  }

  switch (gain){
    case 0:
      this->ECG_gain = 20;
    case 1:
      this->ECG_gain = 40;
    case 2:
      this->ECG_gain = 80;
    case 3:
      this->ECG_gain = 160;
    default:
      this->ECG_gain = 0;
  }
}

void MAX30001G::setECGLeadPolarity(bool inverted, bool open) {
  /*
  inverted 0: regular
  inverted 1: swap ECG_N and ECG_P connection to AFE

  open 0: connected to AFE
  open 1: isolated, use for internal calibration
  */

  this->cnfg_emux.all  = this->readRegister24(MAX30001_CNFG_EMUX);
  
  this->cnfg_emux.bit.ecg_pol = inverted;
  this->cnfg_emux.bit.ecg_openp = open;
  this->cnfg_emux.bit.ecg_openn = open;

  this->writeRegister(MAX30001_CNFG_EMUX, this->cnfg_emux.all);
 
}

void MAX30001G::setECGAutoRecovery(int threshold_voltage) {
/*

  Threshold voltage in mV. 
  
  Default threhshold is 98% of max signal. If larger threshold_voltage is requested, it will be set to 98%.

  ECG signal range is +/-2^17 [ADC] 
  with ADC = ECG_V * ECG_GAIN / V_ref * 2^17 and
  with ECG_Gain of 20V/V and V_ref 1000mV is +-50mV

  Input INA has ability to automticall recover after exessive overdrive
  
  automatic:
    actiavted if input exceeds recovery threshold in mngrc_dyn.bit.fast_th
    if fast recovery starts after 125ms saturation and takeds about 500ms
    ECG signals are tagged with FAST ETAG in FIFO buffer
  
  ECG offset recovery
    fast receiver normal
    fast recovery threshold
  
  fast
  ECG Channel Fast Recovery Mode Selection (ECG High Pass Filter Bypass):
  00 = Normal Mode (Fast Recovery Mode Disabled)
  01 = Manual Fast Recovery Mode Enable (remains active until disabled)
  10 = Automatic Fast Recovery Mode Enable (Fast Recovery automatically activated when/while ECG outputs are saturated, using FAST_TH).
  11 = Reserved. Do not use.

  fast_th
  Automatic Fast Recovery Threshold:
  If fast is enabled (0b10) and the output of an ECG measurement exceeds the 
    symmetric thresholds defined by 2048*FAST_TH for more than 125ms, the 
    Fast Recovery mode will be automatically engaged and remain active for 500ms.
  For example, the default value (FAST_TH = 0x3F) corresponds to an ECG output upper threshold of 0x1F800, and an ECG output lower threshold of 0x20800.

  Threshold Voltage * ECG_gain = FAST_TH x 2048 × V_ref / ADC_FULLSCALE  
  with
    ADC_FULLSCALE 2^17 = 131072​
    x3F = 63 (is default FASV/V
  Threshold Voltage is:
    63*1000*2048/131072/20 = +/-49.22mV 

  ADC = Threshold_Voltage * ECG_GAIN / V_REF  *  2^17
  fast_th = ADC / 2048

  */

  // Ensure boundary
  if ((threshold_voltage * this->ECG_gain) > this->V_ref) { threshold_voltage = this->V_ref / this->ECG_gain; }

  // Calculate FAST_TH value
  uint8_t fast_th = static_cast<uint8_t>((threshold_voltage * this->ECG_gain * 131072) / (2048 * this->V_ref));

  // Ensure not to set threshold higher than 98% of signal.
  if (fast_th > 0x3F) {fast_th = 0x3F;}

  this->mngr_dyn.all  = this->readRegister24(MAX30001_MNGR_DYN);
  this->mngr_dyn.bit.fast = 0b10; // auto
  this->mngr_dyn.bit.fast_th = fast_th; // threshold
  this->writeRegister(MAX30001_MNGR_DYN, this->mngr_dyn.all);

}

void MAX30001G::setECGNormalRecovery() {
  /*

  Input INA has ability to automticall, manually or fast recover after ecessive overdrive
    
  fast
  ECG Channel Fast Recovery Mode Selection (ECG High Pass Filter Bypass):
  00 = Normal Mode (Fast Recovery Mode Disabled)
  01 = Manual Fast Recovery Mode Enable (remains active until disabled)
  10 = Automatic Fast Recovery Mode Enable (Fast Recovery automatically activated when/while ECG outputs are saturated, using FAST_TH).
  11 = Reserved. Do not use.

  */

  this->mngr_dyn.all  = this->readRegister24(MAX30001_MNGR_DYN);
  this->mngr_dyn.bit.fast = 0b00; // normal
  this->writeRegister(MAX30001_MNGR_DYN, this->mngr_dyn.all);

}

void MAX30001G::startECGManualRecovery() {
  /*

  Input INA has ability to be forcefully set to revoer after ecessive overdrive
  For manual recovery settin manual bit triggers recovery.
  One needs to turn it off manually otherwise ECG signal is incorrect.

    fast
    ECG Channel Fast Recovery Mode Selection (ECG High Pass Filter Bypass):
    00 = Normal Mode (Fast Recovery Mode Disabled)
    01 = Manual Fast Recovery Mode Enable (remains active until disabled)
    10 = Automatic Fast Recovery Mode Enable (Fast Recovery automatically activated when/while ECG outputs are saturated, using FAST_TH).
    11 = Reserved. Do not use.

  */

  this->mngr_dyn.all  = this->readRegister24(MAX30001_MNGR_DYN);
  this->mngr_dyn.bit.fast = 0b01; // start manual revcovery
  this->writeRegister(MAX30001_MNGR_DYN, this->mngr_dyn.all);
}

void MAX30001G::stopECGManualRecovery() {
  /* 
  Stop the manual revovery.
  We can not leave the AFE in recovery mode
  */

  this->mngr_dyn.all  = this->readRegister24(MAX30001_MNGR_DYN);
  this->mngr_dyn.bit.fast = 0b00;  // Resets to normal mode
  this->writeRegister(MAX30001_MNGR_DYN, this->mngr_dyn.all);
}

/******************************************************************************************************/
// ECG & BIOZ
/******************************************************************************************************/

void MAX30001G::enableAFE(bool enableECG, bool enableBIOZ, bool enableRtoR) {
  this->cnfg_gen.all  = this->readRegister24(MAX30001_CNFG_GEN);
  this->cnfg_rtor1.all  = this->readRegister24(MAX30001_CNFG_RTOR1);
  this->cnfg_gen.bit.en_ecg  = enableECG ? 1 : 0;
  this->cnfg_gen.bit.en_bioz = enableBIOZ ? 1 : 0;  
  this->cnfg_rtor1.bit.en_rtor = enableRtoR ? 1 : 0;
  if (enableRtoR && !enableECG) {
    this->cnfg_gen.bit.en_ecg  = 1;
    LOGW("To enable RtoR you must enable ECG. ECG enabled");
  }
  this->writeRegister(MAX30001_CNFG_GEN, this->cnfg_gen.all);
  this->writeRegister(MAX30001_CNFG_RTOR1, this->cnfg_rtor1.all);
}

void MAX30001G::setLeadDetection(bool off_detect, bool on_detect, bool bioz_4, int electrode_impedance) {
  /*

    Configures lead on/off detection for the ECG or BioZ channels depending on which is active.
    Before running this routine
      - Make sure  that ecg or bioz is active.
      - Set lead polarity first

    Parameters:
    - off_detect: Enable/disable lead-off detection.
    - on_detect: Enable/disable lead-on detection.
    - bioz_4: true = 4 wire, false = 2 wire bioz
    - electrode_impedance: Impedance of the electrode, used to determine current magnitude and voltage threshold.
        Impedance in MOhm 
        0     0        0nA    
        2  <= 2 MOhm 100nA
        4  <= 4 MOhm  50nA
        10 <=10 MOhm  20nA
        20 <=20 MOhm  10nA
        40  >20 MOhm   5nA

    Lead Off Detection from Spec Sheet

      - Enable/Disable
      - Current Polarity Pull Up / Down
      - Current magnitude 0, 5, 10, 20, 50, 100nA for dry to wet electrode range
      - Voltage threshold: vmid +- 300 recommended, 400 V_AVDD>1.45, 450V_AVDD>1.55, 500mV V_AVDD>1.65V

      V_AVDD Medibrick is 1.8V
      Takes 115-140ms to trigger LDOFF_XX interrupt 

      Table 1
      Electrode Impedance
      <2MOhm  2-4MOhmn 4-10MOhm  10-20MOhm
      IDC 10nA: All Settings of Rb VTH = VMID+/-300, 400
      IDC 20nA: All Settings of Rb and VTH if Electrode Impedance < 10MOhm
      IDC 20nA: All Settings of Rb VTH 400,450,500mV if Impedance > 10MOhm
      IDC 50nA: All Settings of Rb and VTH if Electrode Impedance < 4mOhm
      IDC 50nA: All Settings of Rb VTH 450,500mV if Electrode Impedance < 10MOhm
      IDC 50nA: No settings for Electrode Impedance > 10MOhm
      IDC 100nA: All Settings of Rb and VTH if ELectrode Impedance < 2MOhm
      IDC 100nA: All Settings of Rb and VTH 400,450,500mV
      IDC 100nA: No settings if Electrode Impedance >4MOhm

    Lead On Detection for Spec Sheet
      - enable/disable

    DC Lead Off /ON
    ---------------

    cnfg_gen.bit.vth
      DC Lead-Off Voltage Threshold Selection
      00 = VMID ± 300mV
      01 = VMID ± 400mV
      10 = VMID ± 450mV
      11 = VMID ± 500mV

    cnfg_gen.bit.imag
      DC Lead-Off Current Magnitude Selection
      000 = 0nA (Disable and Disconnect Current Sources)
      001 = 5nA
      010 = 10nA
      011 = 20nA
      100 = 50nA
      101 = 100nA
      110 = Reserved. Do not use.
      111 = Reserved. Do not use.
          
    cnfg_gen.bit.ipol
      DC Lead-Off Current Polarity (if current sources are enabled/connected)
        0 = ECGP - Pullup ECGN – Pulldown
        1 = ECGP - Pulldown ECGN – Pullup

    cnfg_gen.bit.en_dcloff
      DC Lead-Off Detection Enable
      00 = DC Lead-Off Detection disabled
      01 = DCLOFF Detection applied to the ECGP/N pins
      10 = Reserved. Do not use.
      11 = Reserved. Do not use.
      DC Method, requires active selected channel, enables DCLOFF interrupt and status bit behavior.
      Uses current sources and comparator thresholds set below.    

    cnfg_gen.bit.en_bloff
      BioZ Digital Lead Off Detection Enable
      00 = Digital Lead Off Detection disabled
      01 = Lead Off Under Range Detection, 4 electrode BioZ applications
      10 = Lead Off Over Range Detection, 2 and 4 electrode BioZ applications
      11 = Lead Off Over & Under Range Detection, 4 electrode BioZ applications
      AC Method, requires active BioZ Channel , enables BOVER & BUNDR interrupt behavior.
      Uses BioZ excitation current set in CNFG_BIOZ with digital thresholds set in NGR_DYN.

    cnfg_gen.bit.en_ulp_lon
      Ultra-Low Power Lead-On Detection Enable
      00 = ULP Lead-On Detection disabled
      01 = ECG ULP Lead-On Detection enabled
      10 = Reserved. Do not use.
      11 = Reserved. Do not use.
      ULP mode is only active when the ECG channel is powered down/disabled.

  BIOZ AC Lead off
  ----------------

    mngr_dyn.bit.bloff_lo_it
      BioZ AC Lead Off Over-Range Threshold
      If EN_BLOFF[1:0] = 1x and the ADC output of a BioZ measurement exceeds the
      symmetric thresholds defined by ±2048*BLOFF_HI_IT for over 128ms, the BOVER
      interrupt bit will be asserted.
      For example, the default value (BLOFF_IT= 0xFF) corresponds to a BioZ output upper
      threshold of 0x7F800 or about 99.6% of the full scale range, and a BioZ output lower
      threshold of 0x80800 or about 0.4% of the full scale range with the LSB weight ≈ 0.4%.

    mngr_dyn.bit.bloff_hi_it : 8; //  8-15  BIOZ lead off over range threshold
      BioZ AC Lead Off Under-Range Threshold
      If EN_BLOFF[1:0] = 1x and the output of a BioZ measurement is bounded by the
      symmetric thresholds defined by ±32*BLOFF_LO_IT for over 128ms, the BUNDR
      interrupt bit will be asserted.

  */

  // Read the necessary registers
  this->cnfg_gen.all  = this->readRegister24(MAX30001_CNFG_GEN);
  this->cnfg_emux.all = this->readRegister24(MAX30001_CNFG_EMUX);
  this->mngr_dyn.all  = this->readRegister24(MAX30001_MNGR_DYN);

  // Determine if ECG or BioZ is active
  bool ecg_active = this->cnfg_gen.bit.en_ecg;
  bool bioz_active = this->cnfg_gen.bit.en_bioz;

  // Ensure that at least one mode is enabled
  if (!ecg_active && !bioz_active) {
    LOGE("MAX30001G: No ECG or BioZ channel is active. Please enable ECG or BioZ before configuring lead detection.");
    return;
  }

  // Check if ECG polarity is inverted
  bool ecg_polarity_inverted = (this->cnfg_emux.bit.ecg_pol == 1);

  // Lead-Off Detection Configuration
  if (off_detect) {

      // Set the appropriate current magnitude and voltage threshold based on electrode impedance
      if (electrode_impedance <= 0) {
          this->cnfg_gen.bit.imag = 0b000;   // 0nA
          this->cnfg_gen.bit.vth  =  0b00;   // VMID ± 300mV (default)
      } else if (electrode_impedance <= 2) {
          this->cnfg_gen.bit.imag = 0b101;   // 100nA
          if        (this->V_AVDD > 1650) {
            this->cnfg_gen.bit.vth = 0b11;   // VMID ± 500mV
          } else if (this->V_AVDD > 1550) {
            this->cnfg_gen.bit.vth = 0b10;   // VMID ± 450mV
          } else if (this->V_AVDD > 1450) {
            this->cnfg_gen.bit.vth = 0b01;   // VMID ± 400mV
          } else {
            this->cnfg_gen.bit.vth = 0b00;   // VMID ± 300mV (default)
          }           
      } else if (electrode_impedance <= 4) {
          this->cnfg_gen.bit.imag = 0b100;   // 50nA
          if        (this->V_AVDD > 1550) {
            this->cnfg_gen.bit.vth = 0b10;   // VMID ± 450mV
          } else if (this->V_AVDD > 1450) {
            this->cnfg_gen.bit.vth = 0b01;   // VMID ± 400mV
          } else {
            this->cnfg_gen.bit.vth = 0b00;   // VMID ± 300mV (default)
          }           
      } else if (electrode_impedance <= 10) {
          this->cnfg_gen.bit.imag = 0b011;   // 20nA
          if (this->V_AVDD > 1450) {
            this->cnfg_gen.bit.vth = 0b01;   // VMID ± 400mV
          } else {
            this->cnfg_gen.bit.vth = 0b00;   // VMID ± 300mV (default)
          }           
      } else if (electrode_impedance <= 20) {
          this->cnfg_gen.bit.imag = 0b010;   // 10nA
          this->cnfg_gen.bit.vth  =  0b00;   // VMID ± 300mV
      } else {
          this->cnfg_gen.bit.imag = 0b001;   // 5nA (default for high impedance)
          this->cnfg_gen.bit.vth  =  0b00;   // VMID ± 300mV (default)
      }

      // Set lead-off detection current polarity based on ECG polarity
      this->cnfg_gen.bit.ipol = ecg_polarity_inverted ? 1 : 0;  // Inverted current polarity: ECGN pullup, ECGP pulldown

      // Enable DC Lead-Off Detection for ECG or BioZ
      if (ecg_active || bioz_active) {
          this->cnfg_gen.bit.en_dcloff = 0b01; // Enable DCLOFF for ECG
      } else {
          this->cnfg_gen.bit.en_dcloff = 0b00; // 
      }

      // Lead Off AC Detection for BioZ (if BioZ is active)
      if (bioz_active) {
        this->cnfg_gen.bit.en_bloff = bioz_4 ? 0b11 : 0b10; // For 4 wire enable under and over range detection, otherwise only over range detection

        // High: ±2048*BLOFF_HI_IT
        // Low:  ±32*BLOFF_LO_IT
        if (electrode_impedance <= 2) {
          this->mngr_dyn.bit.bloff_lo_it = 0xFF;  // Set to maximum threshold
          this->mngr_dyn.bit.bloff_hi_it = 0xFF; 
        } else if (electrode_impedance <= 10) {
          this->mngr_dyn.bit.bloff_lo_it = 0x7F;  // Intermediate threshold
          this->mngr_dyn.bit.bloff_hi_it = 0x7F; 
        } else {
          this->mngr_dyn.bit.bloff_lo_it = 0x3F;  // Lower threshold for higher impedances
          this->mngr_dyn.bit.bloff_hi_it = 0x3F; 
        }

      }

  } else {
    
    // No Lead OFF detection

    // Default Values
    this->cnfg_gen.bit.imag = 0b000;   // 0nA (default)
    this->cnfg_gen.bit.vth  =  0b00;   // VMID ± 300mV (default)
    // this->cnfg_gen.bit.ipol = ecg_polarity_inverted ? 1 : 0;  // Inverted current polarity: ECGN pullup, ECGP pulldown
    // this->mngr_dyn.bit.bloff_lo_it = 0xFF;  // default
    // this->mngr_dyn.bit.bloff_hi_it = 0xFF;  // default

    // Disable lead-off detection
    this->cnfg_gen.bit.en_dcloff = 0b00;
    this->cnfg_gen.bit.en_bloff = 0b00;

  }

   // Lead-On Detection Configuration
  this->cnfg_gen.bit.en_ulp_lon = on_detect && ecg_active ? 0b01 : 0b00;  // Enable ULP lead-on if ECG is active

  // Write the updated configuration back to the registers
  this->writeRegister(MAX30001_CNFG_GEN, this->cnfg_gen.all);
  this->writeRegister(MAX30001_MNGR_DYN, this->mngr_dyn.all);

}

void MAX30001G::setLeadBias(bool enableECG, bool enableBIOZ, uint8_t resistance) {
/* 

  Common voltage on subject can be maintained by an internal or external feedback resistor.

  This routine needs to be run after ECG or BIOZ are enabled.

  enableECG: enables internal ECG bias
  enableBIOZ: enables internal BIOZ bias
  internal resistance: 50,100,200MOhm

  Restrictions:
   - do not enable both BIOZ and ECG lead bias
   - do not enable lead bias if you have external resistor attached to VCM and 3rd electrode

  If V_ADD is 1.8V (MediBrick): max Input Common Mode is VMID+/-550mV
  
  External lead bias resistior is attached to VCM and can be used as body bias through a third electrode.
  The default external resistance is 200kOhm resistor connected to the third electrode.
  With 200kOhm, bioas current stays within the EC60601 standard.


  cnfg_gen.bit.rbiasn 
    0: not resistivly connected to VMID
    1: resistivly connected to VMID
  
  cnfg_gen.bit.rbiasp 
    0: not resistivly connected to VMID
    1: resistivly connected to VMID

  cnfg_gen.bit.en_rbias 
    00 disabled
    01 ECG bias enabled if ecg is actibe
    10 BIOZ bias enabled if bioz is active
    11 not valid

  cnfg_gen.bit.rbiasv 
    00  50 MOhm
    01 100 MOhm default
    10 200 MOhm
    11 not valid

*/

  this->cnfg_gen.all  = this->readRegister24(MAX30001_CNFG_GEN);

  // Determine if ECG or BioZ is active
  bool ecg_active = this->cnfg_gen.bit.en_ecg;
  bool bioz_active = this->cnfg_gen.bit.en_bioz;

  // Reset lead bias settings
  this->cnfg_gen.bit.rbiasn   = 0;
  this->cnfg_gen.bit.rbiasp   = 0;
  this->cnfg_gen.bit.en_rbias = 0b00;

  if (resistance >= 150) {
    this->cnfg_gen.bit.rbiasv = 0b10; // 200MOhm
  } else if (resistance >  75) {
    this->cnfg_gen.bit.rbiasv = 0b01; // 100MOhm
  } else { 
    this->cnfg_gen.bit.rbiasv = 0b00; //  50MOhm
  }

  // Ensure that the AFE is active before applying lead bias
  if (!ecg_active && enableECG) {
      LOGE("ECG AFE is not active. Please enable ECG before applying lead bias.");
      enableECG = false; // Prevent lead bias from being applied
  }

  if (!bioz_active && enableBIOZ) {
      LOGE("BIOZ AFE is not active. Please enable BIOZ before applying lead bias.");
      enableBIOZ = false; // Prevent lead bias from being applied
  }

  // Ensure only one of ECG or BIOZ lead bias is enabled
  if (enableECG && enableBIOZ) {
      LOGE("Cannot set both ECG and BIOZ lead bias simultaneously! Setting ECG bias only.");
      enableBIOZ = false;  // Disable BIOZ if both are requested
  }

  // Set lead bias for ECG
  if (enableECG) {
    this->cnfg_gen.bit.rbiasn = 1;
    this->cnfg_gen.bit.rbiasp = 1;
    this->cnfg_gen.bit.en_rbias = 0b01; // ECG bias enabled
    LOGD("MAX30001G: ECG lead bias enabled.");
  }

  // Set lead bias for BIOZ
   else if (enableBIOZ) {
    this->cnfg_gen.bit.rbiasn = 1;
    this->cnfg_gen.bit.rbiasp = 1;
    this->cnfg_gen.bit.en_rbias = 0b10; // BIOZ bias enabled
    LOGD("MAX30001G: BIOZ lead bias enabled.");
  }

  // Log if no bias is applied
  if (!enableECG && !enableBIOZ) {
    LOGD("No lead bias applied as neither ECG nor BIOZ AFE is enabled.");
  }

  this->writeRegister(MAX30001_CNFG_GEN, this->cnfg_gen.all);

}

void MAX30001G::setCalibration(bool enableECGCal, bool enableBIOZCal, bool unipolar, bool cal_vmag, uint8_t freq, uint8_t dutycycle) {
/*
  Configures ECG calibration settings.

  This function disconnects ECGP and ECGN from the subject and applies the calibration settings.
  
  Parameters:
  - enableECGCal: Enable or disable ECG calibration
  - enableBIOZCal: Enable or disable BIOZ calibration
  - unipolar: Set to true for unipolar, false for bipolar
  - cal_vmag: Set the calibration voltage magnitude: true for ±0.5 mV, false for ±0.25 mV (CAL_VMAG).
  - freq: Calibration frequency (value=0..7) will result in FMSTR / (2^(7+value*2)) Hz or approx 250..0.01 Hz
  - dutycycle: Duty cycle percentage (default 50%)

ECG Calibration
  - ECGN
  - ECGP 
  - Enable/Disable
  - Unipolar/Bipolar CAL_VMODE
  - Voltage +/-0.25 pr 0.5 mV CAL_VMAG
  - Freqeuncy 0.015625 to 256 Hz
  - Dutycyle 1-99%, 50% is default 

BIOZ Calibration
  - BIOZN
  - BIOZP

Registers

  cnfg_emux.bit.openp
    Open ECGP input switch
    0 = Connect ECGP to AFE
    1 = Disconnect ECGP from AFE, e.g. for calibration

  cnfg_emux.bit.openn
    Open ECGN input switch
    0 = Connect ECGN to AFE
    1 = Disconnect ECGN from AFE, e.g. for calibration

  cnfg_emux.bit.ecg_calp_sel
    ECGP Calibration Selection
      00 = No calibration signal applied
      01 = Input is connected to VMID
      10 = Input is connected to VCALP (only available if CAL_EN_VCAL = 1)
      11 = Input is connected to VCALN (only available if CAL_EN_VCAL = 1)

  cnfg_emux.bit.ecg_caln_sel
    ECGN Calibration Selection
      00 = No calibration signal applied
      01 = Input is connected to VMID
      10 = Input is connected to VCALP (only available if CAL_EN_VCAL = 1)
      11 = Input is connected to VCALN (only available if CAL_EN_VCAL = 1)

  // Enable or disable calibration
  cnfg_cal.bit.vcal = enable ? 1 : 0;

  cnfg_cal.bit.vmode = unipolar ? 0 : 1;   // Set calibration mode: 0 = Unipolar, 1 = Bipolar
  cnfg_cal.bit.vmag = cal_vmag ? 1 : 0;    // Set voltage magnitude: 0 = ±0.25 mV, 1 = ±0.5 mV

  cnfg_cal.bit.fcal = freq & 0x07;         // Set calibration frequency (0.015625 to 256 Hz)
    Calibration Source Frequency Selection (FCAL) 
    000 = FMSTR /128   (     256, 250,        or 249.75Hz)
    001 = FMSTR /512   (      64,  62.5,      or  62.4375   Hz)
    010 = FMSTR /2048  (      16,  15.625,    or  15.609375 Hz)
    011 = FMSTR /8192  (       4,   3.90625,  or   3.902344 Hz)
    100 = FMSTR /2^15  (       1,   0.976563, or   0.975586 Hz)
    101 = FMSTR /2^17  (    0.25,   0.24414,  or   0.243896 Hz)
    110 = FMSTR /2^19  (  0.0625,   0.061035, or   0.060974 Hz)
    111 = FMSTR /2^21  (0.015625,   0.015259, or   0.015244 Hz)
    Actual frequencies are determined by FMSTR selection (see CNFG_GEN for details),
    frequencies in parenthesis are based on 32,768, 32,000, or 31,968Hz clocks (FMSTR[1:0] = 00). TCAL = 1/FCAL

  cnfg_cal.bit.fifty = dutycycle==50 ? 1 : 0;  // Set duty cycle: 1 = 50%, 0 = Custom time high duration

  cnfg_cal.bit.thigh (11 bit)
    Calibration Source Time High Selection
    If FIFTY = 1, tHIGH = 50% (and THIGH[10:0] are ignored), 
    otherwise THIGH = THIGH[10:0] x CAL_resolution
    CAL_RES is determined by FMSTR selection (see CNFG_GEN for details);
    for example, if FMSTR[1:0] = 00, CAL_resolution = 30.52µs

  */

    // Read current settings
    this->cnfg_emux.all = this->readRegister24(MAX30001_CNFG_EMUX);
    this->cnfg_bmux.all = this->readRegister24(MAX30001_CNFG_BMUX);
    this->cnfg_cal.all  = this->readRegister24(MAX30001_CNFG_CAL);

    if (enableECGCal) {
      // Disconnect ECGP and ECGN by setting the openp and openn bits
      this->cnfg_emux.bit.openp = 1; // Open ECGP input switch
      this->cnfg_emux.bit.openn = 1; // Open ECGN input switch
      // Connect internally
      this->cnfg_emux.bit.ecg_calp_sel = 0b01; // CALP
      this->cnfg_emux.bit.ecg_caln_sel = 0b11; // CALN 
    } else {
      // Disconnect calibration signal internally
      this->cnfg_emux.bit.ecg_calp_sel = 0b00; // CALP
      this->cnfg_emux.bit.ecg_caln_sel = 0b00; // CALN       
      // Connect ECGP and ECGN by setting the openp and openn bits
      this->cnfg_emux.bit.openp = 0; // Connect ECGP input
      this->cnfg_emux.bit.openn = 0; // Connect ECGN input
    }

    if (enableBIOZCal) {
      // Disconnect BIOZP and BIOZN by setting the openp and openn bits
      this->cnfg_bmux.bit.openp = 1; // Open BIOZP input switch
      this->cnfg_bmux.bit.openn = 1; // Open BIOZN input switch
      // Connect internally
      this->cnfg_bmux.bit.bioz_calp_sel = 0b01; // CALP
      this->cnfg_bmux.bit.bioz_caln_sel = 0b11; // CALN 
    } else {
      // Disable BIOZ Calibration, close the switches
      this->cnfg_bmux.bit.bioz_calp_sel = 0b00; // No calibration signal applied
      this->cnfg_bmux.bit.bioz_caln_sel = 0b00;
      this->cnfg_bmux.bit.openp = 0; // Close BIOZP switch
      this->cnfg_bmux.bit.openn = 0; // Close BIOZN switch      
    }

    // Apply calibration settings
    this->cnfg_cal.bit.vmode = unipolar ? 0 : 1;   // Set calibration mode: 0 = Unipolar, 1 = Bipolar
    this->cnfg_cal.bit.vmag = cal_vmag ? 1 : 0;    // Set voltage magnitude: 0 = ±0.25 mV, 1 = ±0.5 mV
    this->cnfg_cal.bit.fcal = freq & 0x07;         // Set calibration frequency (0.015625 to 256 Hz)
    // update gloabl variable
    updateGlobalCAL_fcal();

    if (dutycyle == 50) {
      this->cnfg_cal.bit.fifty = 1;
    } else {
      this->cnfg_cal.bit.fifty = 0; 
      // cnfg_cal.bit.thigh = static_cast<uint16_t>(((float(dutycyle) / 100.0) * 1000000. / CAL_resolution) / CAL_fcal );
      // dutycyle is in percent e.g. 50
      // CAL_resultion is in micro seconds
      // CAL_fcal is in Hz
      // 1/CAL_fcal is period in seconds
      // duty cycle * 1/CAL_fcal is the time high in seconds
      // time high in seconds *1,000,000 / CAL_resolution is the time high in resolution units 
      this->cnfg_cal.bit.thigh = static_cast<uint16_t>((float(dutycyle) * 10000.0) / this->CAL_fcal / this->CAL_resolution);
    }

    // Enable or disable calibration
    if ( ECGenable || BIOZenable ) {
      this->cnfg_cal.bit.vcal = 1;
    } else {
      LOGD("Both ECG and BIOZ calibration are disabled. No calibration will be applied.");
      this->cnfg_cal.bit.vcal = 0;
    } 

    // Write back the configuration
    this->writeRegister(MAX30001_CNFG_EMUX, this->cnfg_emux.all);
    this->writeRegister(MAX30001_CNFG_BMUX, this->cnfg_bmux.all);
    this->writeRegister(MAX30001_CNFG_CAL, this->cnfg_cal.all);

}

void MAX30001G::setDefaultNoCalibration() {
/* 
 * Do not enable calibration signal
 */

    setCalibration(
        false,      // Enable ECG calibration
        false,      // Disable BIOZ calibration
        true,       // Unipolar mode
        true,       // ±0.5 mV calibration voltage
        0b100,      // Frequency: 1 Hz
        50          // 50% duty cycle
    );
}

void MAX30001G::setDefaultECGCalibration() {
    /*
    Sets default values for ECG calibration.
    - enable: true (calibration enabled)
    - unipolar: true (Unipolar mode)
    - cal_vmag: true (±0.5 mV)
    - freq: 0b100 (1 Hz) (default)
    - dutycycle: 50% (default)
    */

    setCalibration(
        true,       // Enable ECG calibration
        false,      // Disable BIOZ calibration
        true,       // Unipolar mode
        true,       // ±0.5 mV calibration voltage
        0b100,      // Frequency: 1 Hz (FMSTR/2^15)
        50          // 50% duty cycle
    );
}

void MAX30001G::setDefaultBIOZCalibration() {
    /*
    Sets default values for BIOZ calibration.
    - enable: true (calibration enabled)
    - unipolar: true (Unipolar mode)
    - cal_vmag: true (±0.5 mV)
    - freq: 0b100 (1 Hz) (default)
    - dutycycle: 50% (default)
    */

    setCalibration(
        false,      // Disable ECG calibration
        true,       // Enable BIOZ calibration
        true,       // Unipolar mode
        true,       // ±0.5 mV calibration voltage
        0b100,      // Frequency: 1 Hz (FMSTR/2^15)
        50          // 50% duty cycle
    );
}

/******************************************************************************************************/
// RtoR
/******************************************************************************************************/

void MAX30001G::setRtoR(uint8_t ptsf, uint8_t pavg, uint8_t gain, uint8_t wndw, uint8_t hoff, uint8_t ravg, uint8_t rhsf) {
/*

Configures R-to-R detection for the ECG channel with detailed settings.
  Timing resolutin is about 8ms
    delay is about 105ms
    RRINT interrupt is triggered when QRS waveforms is detected
    RTOR register holds time between R waves in RTOR resolution units

  Parameters:
  - enable: Enable or disable RtoR detection.
  - ptsf: Peak Threshold Scaling Factor    (0 to 15).
  - pavg: Peak Averaging Weight Factor     (0 to  3).
  - gain: Gain setting for RtoR            (0 to 15).
  - wndw: Width of the averaging window    (0 to 15).
  - hoff: Minimum Hold Off                 (0 to 63).
  - ravg: Interval Averaging Weight Factor (0 to  3).
  - rhsf: Interval Hold Off Scaling Factor (0 to  7).
  
  For example:
  - enable: true
  - ptsf: A value of      4 (1/16) might be typical for the threshold.
  - pavg: A value of   0b10 (8) for some averaging.
  - gain: A value of 0b1111 for autoscale.
  - wndw: A value of     12 (96ms) for the window width.
  - hoff: A value of     32 for the hold off.
  - ravg: A value of   0b10 (8) for scaling factor.
  - rhsf: A value of  0b100 (4/8) for hold off scalting factor.

cnfg_rtor1.bit.wndw
  This is the width of the averaging window, which adjusts the algorithm sensitivity to the width of the QRS complex.
  R-to-R Window Averaging (Window Width = WNDW[3:0]*8ms) 
    0000 =  6 x RTOR_RES
    0001 =  8 x RTOR_RES
    0010 = 10 x RTOR_RES
    0011 = 12 x RTOR_RES  (default = 96ms)
    0100 = 14 x RTOR_RES
    0101 = 16 x RTOR_RES
    0110 = 18 x RTOR_RES
    0111 = 20 x RTOR_RES
    1000 = 22 x RTOR_RES
    1001 = 24 x RTOR_RES
    1010 = 26 x RTOR_RES
    1011 = 28 x RTOR_RES
    1100 = Reserved. Do not use.
    1101 = Reserved. Do not use.
    1110 = Reserved. Do not use.
    1111 = Reserved. Do not use.
  The value of RTOR_RES is approximately 8ms, see Table 28. updateGlobalRTOR_RES() computes it.

cnfg_rtor1.bit.gain

  R-to-R Gain (where Gain = 2^RGAIN[3:0], plus an auto-scale option). 
  This is used to maximize the dynamic range of the algorithm.
  0000 =    1
  0001 =    2
  0010 =    4
  0011 =    8
  0100 =   16
  0101 =   32
  0110 =   64 (initial for auto)
  0111 =  128
  1000 =  256
  1001 =  512
  1010 = 1024
  1011 = 2048
  1100 = 4096
  1101 = 8192
  1110 = 16384
  1111 = Auto-Scale (default)
  In Auto-Scale mode, the initial gain is set to 64.

cnfg_rtor1.bit.pavg
  R-to-R Peak Averaging Weight Factor
    This is the weighting factor for the current R-to-R peak observation vs. past peak
    observations when determining peak thresholds. Lower numbers weight current peaks more heavily.
    00 = 2
    01 = 4
    10 = 8 (default)
    11 = 16
    Peak_Average(n) = [Peak(n) + (PAVG-1) x Peak_Average(n-1)] / PAVG.

cnfg_rtor1.bit.ptsf
  R-to-R Peak Threshold Scaling Factor
    This is the fraction of the Peak Average value used in the Threshold computation. 
    Values of 1/16 to 16/16 are selected by (PTSF[3:0]+1)/16, default is 4/16.

cnfg_rtor2.bit.hoff
  R-to-R Minimum Hold Off
    This sets the absolute minimum interval used for the static portion of the Hold Off criteria. 
    Values of 0 to 63 are supported, default is 32
      tHOLD_OFF_MIN = HOFF[5:0] * tRTOR, where tRTOR is approximately 8ms, as determined by
      FMSTR[1:0] in the CNFG_GEN register.
      (representing approximately ¼ second).
      The R-to-R Hold Off qualification interval is
      tHold_Off = MAX(tHold_Off_Min, tHold_Off_Dyn) (see below).

cnfg_rtor2.bit.ravg
  R-to-R Interval Averaging Weight Factor
    This is the weighting factor for the current R-to-R interval observation vs. the past interval
    observations when determining dynamic holdoff criteria. Lower numbers weight current intervals
    more heavily.
      00 = 2
      01 = 4
      10 = 8 (default)
      11 = 16
      Interval_Average(n) = [Interval(n) + (RAVG-1) x Interval_Average(n-1)] / RAVG.

cnfg_rtor2.bit.rhsf
  R-to-R Interval Hold Off Scaling Factor
    This is the fraction of the R-to-R average interval used for the dynamic portion of the holdoff criteria (tHOLD_OFFDYN).
    Values of 0/8 to 7/8 are selected by RTOR_RHSF[3:0]/8, default is 4/8.
    If 000 (0/8) is selected, then no dynamic factor is used and the holdoff criteria is determined by
    HOFF[5:0] only (see above).

 */

    // Ensure the values are within the allowed range
    if (ptsf > 15)  ptsf = 15;
    if (pavg > 3)   pavg = 3;
    if (gain > 15)  gain = 15;
    if (wndw > 15)  wndw = 15;
    if (hoff > 63)  hoff = 63;
    if (ravg > 3)   ravg = 3;
    if (rhsf > 7)   rhsf = 7;

    // Read the current settings
    this->cnfg_rtor1.all = this->readRegister24(MAX30001_CNFG_RTOR1);
    this->cnfg_rtor2.all = this->readRegister24(MAX30001_CNFG_RTOR2);

    // Configure RtoR detection parameters in CNFG_RTOR1
    this->cnfg_rtor1.bit.ptsf = ptsf;              // Peak Threshold Scaling Factor
    this->cnfg_rtor1.bit.pavg = pavg;              // Peak Averaging Weight Factor
    this->cnfg_rtor1.bit.gain = gain;              // R-to-R Gain setting
    this->cnfg_rtor1.bit.wndw = wndw;              // Width of the averaging window

    // Configure RtoR detection parameters in CNFG_RTOR2
    this->cnfg_rtor2.bit.hoff = hoff;              // Minimum Hold Off
    this->cnfg_rtor2.bit.ravg = ravg;              // Interval Averaging Weight Factor
    this->cnfg_rtor2.bit.rhsf = rhsf;              // Interval Hold Off Scaling Factor

    // Write the updated settings back to the registers
    this->writeRegister(MAX30001_CNFG_RTOR1, this->cnfg_rtor1.all);
    this->writeRegister(MAX30001_CNFG_RTOR2, this->cnfg_rtor2.all);
}

void MAX30001G::setDefaultRtoR() {
    /*
    Sets default values for R-to-R detection.
    - ptsf: 0b0011 (4) (4/16)
    - pavg: 0b10 (8)
    - gain: 0b1111 (Auto-Scale)
    - wndw: 0b011 12 (96ms)
    - hoff: 0b100000 (32)
    - ravg: 0b10 (8)
    - rhsf: 0b100 (4/8)

    ptsf of 0b0110 (6) is also a common value for the threshold.
    */
    
    setRtoR(
        0b0011,    // ptsf: Peak Threshold Scaling Factor (4/16)
        0b10,      // pavg: Peak Averaging Weight Factor (8)
        0b1111,    // gain: Auto-Scale
        0b011,     // wndw: Window width (96ms)
        0b100000,  // hoff: Minimum Hold Off
        0b10,      // ravg: Interval Averaging Weight Factor (8)
        0b100      // rhsf: Interval Hold Off Scaling Factor (4/8)
    );
}

/******************************************************************************************************/
// BioZ
/******************************************************************************************************/

void MAX30001G::setBIOZSamplingRate(uint8_t BIOZ) {
  /*
  Set the BIOZ sampling rate for the AFE, 
  0 =  low  25-32 * default
  1 = high  50-64
  Based on FMSTR, sets global variable BIOZ_samplingRate
  */

  this->cnfg_bioz.all = this->readRegister24(MAX30001_CNFG_BIOZ);
  this->cnfg_gen.all  = this->readRegister24(MAX30001_CNFG_GEN);

  // Set the BIOZ sampling rate based on the input
  if (BIOZ == 0) {
    this->cnfg_bioz.bit.rate = 0b1; // low
  } else if (BIOZ == 1) {
    this->cnfg_bioz.bit.rate = 0b0; // high
  } else {
    LOGE("Invalid BIOZ sampling rate input. Only 0 or 1 is allowed.");
    return; // Early return for invalid input
  }  

  this->writeRegister(MAX30001_CNFG_BIOZ, this->cnfg_bioz.all);

  // Set the BIOZ sampling rate based on the FMSTR frequency
  switch (this->cnfg_gen.bit.fmstr) {
    case 0b00: // FMSTR = 32768Hz
      this->BIOZ_samplingRate = (this->cnfg_bioz.bit.rate == 0) ? 64.0 : 32.0;
      break;
    case 0b01: // FMSTR = 32000Hz
      this->BIOZ_samplingRate = (this->cnfg_bioz.bit.rate == 0) ? 62.5 : 31.25;
      break;
    case 0b10: // FMSTR = 32000Hz (for lower rates)
      this->BIOZ_samplingRate = (this->cnfg_bioz.bit.rate == 0) ? 50.0 : 25.0;
      break;
    case 0b11: // FMSTR = 31968.78Hz
      this->BIOZ_samplingRate = (this->cnfg_bioz.bit.rate == 0) ? 49.95 : 24.98;
      break;
    default:
      LOGE("Invalid FMSTR setting. Could not determine BIOZ sampling rate.");
      this->BIOZ_samplingRate = 0.0;
      break;
  }

  LOGD("BIOZ Sampling Rate Set: %.2f Hz", this->BIOZ_samplingRate);

}

void MAX30001G::setBIOZgain(uint8_t gain), bool lowNoise {
/*
  Set the BIOZ gain for the AFE
    0 = 10V/V * default
    1 = 20V/V
    2 = 40V/V
    3 = 80V/V

  Set Low Noise or Low Power mode
    0 = Low Power * default
    1 = Low Noise
*/

  // Read the BIOZ configuration register
  this->cnfg_bioz.all = this->readRegister24(MAX30001_CNFG_BIOZ);

  if (gain <= 3) {
    
    // Set the gain value
    this->cnfg_bioz.bit.gain = gain;
    
    // Set the global variable based on the selected gain
    switch (gain) {
      case 0:
        this->BIOZ_gain = 10;  // 10V/V
        break;
      case 1:
        this->BIOZ_gain = 20;  // 20V/V
        break;
      case 2:
        this->BIOZ_gain = 40;  // 40V/V
        break;
      case 3:
        this->BIOZ_gain = 80;  // 80V/V
        break;
    }

    LOGI("BIOZ gain set to %u V/V", this->BIOZ_gain);
  } else {
    LOGE("Invalid BIOZ gain value: %u. Valid range is 0-3.", gain);
  }

  if (lowNoise) {    
    // Set the low noise value
    this->cnfg_bioz.bit.ln_bioz = 1;
    LOGI("BIOZ set to Low Noise mode.");
  } else {
    // Set the low noise value
    this->cnfg_bioz.bit.ln = 0;
    LOGI("BIOZ set to Low Power mode.");
  }

  // Write the updated register back
  this->writeRegister(MAX30001_CNFG_BIOZ, this->cnfg_bioz.all);

}

void MAX30001G::setBIOZModulationFrequencyByFrequency(uint16_t frequency) {

/*
  Set the BIOZ modulation frequency.
  See function comment for detailed explanation of frequency ranges and their corresponding settings.

  Select numer in this range results in following frequency:
   Range          Frequency         Currents
         > 104000 approx 128kHz     55 - 96000
   60000 - 104000 approx 80kHz      55 - 96000
   29000 - 60000  approx 40kHz      55 - 96000
   13000 - 29000  approx 18kHz      55 - 96000
    6000 - 13000  approx  8kHz      55 - 80000
    3000 - 6000   approx  4kHz      55 - 32000
    1500 - 3000   approx  2kHz      55 - 16000
     750 - 1500   approx  1kHz      55 -  8000
     375 - 750    approx  0.5 kHz   55 -  8000
     187 - 375    approx  0.25 kHz  55 -  8000
      >0 - 187    approx  0.125 kHz 55 -  8000
       0          OFF 

  After setting the the frequency, the current magnitude will need to be set.

*/

  // BIOZ CURRENT GENERATOR MODULATION FREQUENCY (Hz)
  // BIOZ_FCGEN[3:0]  FMSTR[1:0] = 00 FMSTR[1:0] = 01  FMSTR[1:0] = 10  FMSTR[1:0] = 11   CMAG allowed
  //                  fMSTR= 32,768Hz fMSTR = 32,000Hz fMSTR = 32,000Hz fMSTR = 31,968Hz  
  // 0000             131,072         128,000          128,000         127,872            all
  // 0001              81,920          80,000           80,000          81,920            all
  // 0010              40,960          40,000           40,000          40,960            all
  // 0011              18,204          17,780           17,780          18,204            all
  // 0100               8,192           8,000            8,000           7,992            not 111
  // 0101               4,096           4,000            4,000           3,996            000 to 011
  // 0110               2,048           2,000            2,000           1,998            000 to 010
  // 0111               1,024           1,000            1,000             999            000, 001
  // 1000                 512             500              500             500            000, 001
  // 1001                 256             250              250             250            000, 001
  // 101x,11xx            128             125              125             125            000, 001

  this->cnfg_bioz.all = this->readRegister24(MAX30001_CNFG_BIOZ);
  this->cnfg_gen.all  = this->readRegister24(MAX30001_CNFG_GEN);

  // Helper lambda to assign BIOZ_frequency based on FMSTR
  auto setBIOZFrequency = [&](uint32_t f32768, uint32_t f32000_1, uint32_t f32000_2, uint32_t f31968) {
    switch (this->cnfg_gen.bit.fmstr) {
      case 0b00: this->BIOZ_frequency = f32768;   break;
      case 0b01: this->BIOZ_frequency = f32000_1; break;
      case 0b10: this->BIOZ_frequency = f32000_2; break;
      case 0b11: this->BIOZ_frequency = f31968;   break;
      default: this->BIOZ_frequency = 0; break;
    }
  };

  if (frequency >= 104000) {
    this->cnfg_bioz.bit.fcgen = 0b0000;
    setBIOZFrequency(131072, 128000, 128000, 127872);
  } else if (frequency > 60000) {
    this->cnfg_bioz.bit.fcgen = 0b0001;
    setBIOZFrequency( 81920,  80000,  80000,  81920);
  } else if (frequency > 29000) {
    this->cnfg_bioz.bit.fcgen = 0b0010;
    setBIOZFrequency( 40960,  40000,  40000,  40960);
  } else if (frequency > 13000) {
    this->cnfg_bioz.bit.fcgen = 0b0011;
    setBIOZFrequency( 18204,  17780,  17780,  18204);
  } else if (frequency > 6000) {
    this->cnfg_bioz.bit.fcgen = 0b0100;
    setBIOZFrequency(  8192,   8000,   8000,   7992);
  } else if (frequency > 3000) {
    this->cnfg_bioz.bit.fcgen = 0b0101;
    setBIOZFrequency(  4096,   4000,   4000,   3996);
  } else if (frequency > 1500) {
    this->cnfg_bioz.bit.fcgen = 0b0110;
    setBIOZFrequency(  2048,   2000,   2000,   1998);
  } else if (frequency > 750) {
    this->cnfg_bioz.bit.fcgen = 0b0111;
    setBIOZFrequency(  1024,   1000,   1000,    999);
  } else if (frequency > 375) {
    this->cnfg_bioz.bit.fcgen = 0b1000;
    setBIOZFrequency(   512,    500,    500,    500);
  } else if (frequency > 187) {
    this->cnfg_bioz.bit.fcgen = 0b1001;
    setBIOZFrequency(   256,    250,    250,    250);
  } else if (frequency > 0) {
    this->cnfg_bioz.bit.fcgen = 0b1010;
    setBIOZFrequency(   128,    125,    125,    125);
  } else {
    // Frequency off or invalid
    LOGE("Invalid BIOZ frequency: %u. Frequency must be greater than 0.", frequency);
    return;
  }

  // Write to the register
  this->writeRegister(MAX30001_CNFG_BIOZ, this->cnfg_bioz.all);
  LOGln("BIOZ frequency set to %5u [Hz]", this->BIOZ_frequency);

}

void MAX30001G::setBIOZModulationFrequencybyIndex(uint8_t frequency_selector) {

/*
  Set the BIOZ modulation frequency using a selector index.
  // BIOZ CURRENT GENERATOR MODULATION FREQUENCY (Hz)
  // BIOZ_FCGEN[3:0]  FMSTR[1:0] = 00 FMSTR[1:0] = 01  FMSTR[1:0] = 10  FMSTR[1:0] = 11   CMAG allowed
  //                  fMSTR= 32,768Hz fMSTR = 32,000Hz fMSTR = 32,000Hz fMSTR = 31,968Hz  
  // 0000             131,072         128,000          128,000         127,872            all
  // 0001              81,920          80,000           80,000          81,920            all
  // 0010              40,960          40,000           40,000          40,960            all
  // 0011              18,204          17,780           17,780          18,204            all
  // 0100               8,192           8,000            8,000           7,992            not 111
  // 0101               4,096           4,000            4,000           3,996            000 to 011
  // 0110               2,048           2,000            2,000           1,998            000 to 010
  // 0111               1,024           1,000            1,000             999            000, 001
  // 1000                 512             500              500             500            000, 001
  // 1001                 256             250              250             250            000, 001
  // 101x,11xx            128             125              125             125            000, 001
*/

  this->cnfg_bioz.all = this->readRegister24(MAX30001_CNFG_BIOZ);
  this->cnfg_gen.all  = this->readRegister24(MAX30001_CNFG_GEN);

  if (frequency_selector <= 0b1010) {
    this->cnfg_bioz.bit.fcgen = frequency_selector;

    // Helper lambda to assign BIOZ_frequency based on FMSTR
    auto setBIOZFrequency = [&](uint32_t f32768, uint32_t f32000_1, uint32_t f32000_2, uint32_t f31968) {
      switch (this->cnfg_gen.bit.fmstr) {
        case 0b00: this->BIOZ_frequency = f32768; break;
        case 0b01: this->BIOZ_frequency = f32000_1; break;
        case 0b10: this->BIOZ_frequency = f32000_2; break;
        case 0b11: this->BIOZ_frequency = f31968; break;
        default:   this->BIOZ_frequency = 0; break;
      }
    };

    switch (frequency_selector) {
      case 0b0000: setBIOZFrequency(131072, 128000, 128000, 127872); break;
      case 0b0001: setBIOZFrequency( 81920,  80000,  80000,  81920); break;
      case 0b0010: setBIOZFrequency( 40960,  40000,  40000,  40960); break;
      case 0b0011: setBIOZFrequency( 18204,  17780,  17780,  18204); break;
      case 0b0100: setBIOZFrequency(  8192,   8000,   8000,   7992); break;
      case 0b0101: setBIOZFrequency(  4096,   4000,   4000,   3996); break;
      case 0b0110: setBIOZFrequency(  2048,   2000,   2000,   1998); break;
      case 0b0111: setBIOZFrequency(  1024,   1000,   1000,    999); break;
      case 0b1000: setBIOZFrequency(   512,    500,    500,    500); break;
      case 0b1001: setBIOZFrequency(   256,    250,    250,    250); break;
      case 0b1010: setBIOZFrequency(   128,    125,    125,    125); break;
      default: 
        LOGE("Invalid frequency selector: %u", frequency_selector);
        return;
    }

    this->writeRegister(MAX30001_CNFG_BIOZ, this->cnfg_bioz.all);
    LOGln("BIOZ frequency set to %5u [Hz]", this->BIOZ_frequency);
  } else {
    LOGE("Invalid frequency selector: %u", frequency_selector);
  }
}

void MAX30001G::setBIOZmag(uint32_t current){
/*
  Setting current magnitude
    range is 50 to 96000 nA
  This adjusts current to be in the range allowed by modulation frequency
  8 microA can be used at all frequencies
  This also programs the BIOZ common mode current feeback resistor
  Before using this function, modualtion frequency needs to be set
*/
  
  this->cnfg_bioz.all    = this->readRegister24(MAX30001_CNFG_BIOZ);
  this->cnfg_bioz_lc.all = this->readRegister24(MAX30001_CNFG_BIOZ_LC);

  // high current 
  // --------------------------------------------------------------------
  //   cmag >= 001
  //   and cnfg_bioz_lc.bit.hi_lob == 1 
  //
  //BioZ Current Generator Magnitude: cnfg_bioz.cgmag
  // 000 = Off (DRVP and DRVN floating, Current Generators Off)
  // 001 = 8µA (also use this setting when BIOZ_HI_LOB = 0)
  // 010 = 16µA
  // 011 = 32µA
  // 100 = 48µA
  // 101 = 64µA
  // 110 = 80µA
  // 111 = 96µA

  // low current
  // ---------------------------------------------------------------------
  //   cmag == 001 
  //   and cnfg_bioz_lc.bit.hi_lob == 0 
  //   cnfg_bioz_lc.bit.lc2x == 0 55-1100nA
  //   cnfg_bioz_lc.bit.lc2x == 1 110-1100nA
  //   set the common mode resistance as recommended in BIOZ_CMRES.
  //
  // BIOZ Low Current Generator Magnitude: cnfg_bioz_lc.bit.cmag_lc
  //         LC2X = 0 LC2X = 1
  // 0000    0        0
  // 0001    55nA     110nA
  // 0010    110nA    220nA
  // 0011    220nA    440nA
  // 0100    330nA    660nA
  // 0101    440nA    880nA
  // 0110    550nA    1100nA

  // Make sure requested current is achievable with current frequency 
  // ----------------------------------------------------------------
  // BIOZ CURRENT GENERATOR MODULATION FREQUENCY (Hz)                                     CNFG_BIOZ
  // BIOZ_FCGEN[3:0]  FMSTR[1:0] = 00 FMSTR[1:0] = 01  FMSTR[1:0] = 10  FMSTR[1:0] = 11   CGMAG allowed
  //                  fMSTR= 32,768Hz fMSTR = 32,000Hz fMSTR = 32,000Hz fMSTR = 31,968Hz  
  // 0000             131,072         128,000          128,0001        127,872            all
  // 0001              81,920          80,000           80,000          81,920            all
  // 0010              40,960          40,000           40,000          40,960            all
  // 0011              18,204          17,780           17,780          18,204            all
  // 0100               8,192           8,000            8,000           7,992            not 111
  // 0101               4,096           4,000            4,000           3,996            000 to 011
  // 0110               2,048           2,000            2,000           1,998            000 to 010
  // 0111               1,024           1,000            1,000             999            000, 001
  // 1000                 512             500              500             500            000, 001
  // 1001                 256             250              250             250            000, 001
  // 101x,11xx            128             125              125             125            000, 001

  // Limit current based on the modulation frequency settings (fcgen)
  if (this->cnfg_bioz.bit.fcgen >= 0b0111) {
    if (current > 8000) {
      current = 8000;
      LOGW("Current cannot exceed 8000 nA at this frequency.");
    }
  } else if (this->cnfg_bioz.bit.fcgen == 0b0110) {
    if (current > 16000) {
      current = 16000;
      LOGW("Current cannot exceed 16000 nA at this frequency.");
    }
  } else if (this->cnfg_bioz.bit.fcgen == 0b0101) {
    if (current > 32000) {
      current = 32000;
      LOGW("Current cannot exceed 32000 nA at this frequency.");
    }
  } else if (this->cnfg_bioz.bit.fcgen == 0b0100) {
    if (current > 96000) {
      current = 96000;
      LOGW("Current cannot exceed 96000 nA at this frequency.");
    }
  }

  // Disable if current is 0
  if (current == 0) {
    this->cnfg_bioz.cmag = 0b000;  // Turn off current generators
    this->BIOZ_cgmag = 0;
    LOGln("BioZ current generator disabled.");
    return;  // Exit early as no other operations are needed
  }

  // Set High Current Mode --------------------------------        // 11       0  NA

  // Values are in Nano Amps
  if (current >= 8000) {
    this->cnfg_bioz_lc.bit.hi_lob = 1;
    if        (current < 12000){
      this->cnfg_bioz.cmag = 0b001;
      this->BIOZ_cgmag = 8000;
    } else if (current < 24000) {
      this->cnfg_bioz.cmag = 0b010;
      this->BIOZ_cgmag = 16000;
    } else if (current < 40000) {
      this->cnfg_bioz.cmag = 0b011;
      this->BIOZ_cgmag = 32000;
    } else if (current < 56000) {
      this->cnfg_bioz.cmag = 0b100;
      this->BIOZ_cgmag = 48000;
    } else if (current < 72000) {
      this->cnfg_bioz.cmag = 0b101;
      this->BIOZ_cgmag = 64000;
    } else if (current < 88000) {
      this->cnfg_bioz.cmag = 0b110;
      this->BIOZ_cgmag = 80000;
    } else if (current == 96000) {
      this->cnfg_bioz.cmag = 0b111;
      this->BIOZ_cgmag = 96000;
    }
  } 
  
  // Set Low current Mode ------------------------------
  // values are in Nano Amps
  else {
    this->cnfg_bioz.cmag = 0b001;
    this->cnfg_bioz_lc.bit.hi_lob = 0;

    if          (current < 80){
      this->cnfg_bioz_lc.bit.lc2x = 0;
      this->cnfg_bioz_lc.bit.cmag_lc = 0b001;
      this->BIOZ_cgmag = 55;
    } else {
      this->cnfg_bioz_lc.bit.lc2x = 1;
      if        (current < 165){
        this->cnfg_bioz_lc.bit.cmag_lc = 0b001;
        this->BIOZ_cgmag = 110;
      } else if (current < 330){
        this->cnfg_bioz_lc.bit.cmag_lc = 0b010;
        this->BIOZ_cgmag = 220;
      } else if (current < 550){
        this->cnfg_bioz_lc.bit.cmag_lc = 0b011;
        this->BIOZ_cgmag = 440;
      } else if (current < 770){
        this->cnfg_bioz_lc.bit.cmag_lc = 0b100;
        this->BIOZ_cgmag = 660;
      } else if (current < 990){
        this->cnfg_bioz_lc.bit.cmag_lc = 0b101;
        this->BIOZ_cgmag = 880;
      } else {
        this->cnfg_bioz_lc.bit.cmag_lc = 0b110;
        this->BIOZ_cgmag = 1100;
      }
    } 
  }
  
  // Adjust Common Mode Current Feedback 
  // -----------------------------------
  //
  // There is feedback for high current mode
  // There is feedback for low current mode
  // BIOZ_cmag is in [nA]
  //
  // High Current 8000 - 96000 nA
  // ---
  // active and passive/resistive current mode feedback BMUX_CG_MODE
  //
  // Options
  // 1) external 324kOhm resistor on R_BIAS to A_GND, EXT_RBIAS = on
  //    MediBrick board has resistor in place
  // 2) BISTR [0] 27k, [1] 108k, [2] 487k, [3] 1029k, EN_BISTR = on
  //    This is usually used for calibration
  // 3) no external no internal, uses active
  //
  // Low Current 55 - 1100 nA
  // ---
  // only resistive common mode current feedback 
  //
  // cnfg_bioz_lc.cmres needs to be matched to current magnitude
  // It should be approximately  5000 / (drive current [µA]) [kΩ]
  // available are: 5, 5.5, 6.5, 7.5, 10, 12.5, 20, 40, 80, 100, 160, 320, MOhm

  // Adjust Common Mode Current Feedback based on the current mode (high or low)
  if (this->cnfg_bioz_lc.bit.hi_lob == 1){
    // High Current Common Mode: enable external resistor
    this->cnfg_bioz.bit.ext_bias = 1;
    LOGln("External BioZ common mode current feedback resistor for high current enabled.");
  } else {
    // Low Current Common Mode: select apprropriate internal resistor
    uint32_t cmres = 5000 * 1000 / this->BIOZ_cgmag; // in kOhm
    if        (cmres <= 5250) {
      this->cnfg_bioz_lc.bit.cmres = 0b1111; 
      this->BIOZ_cmres = 5000;
    } else if (cmres <= 6000) {
      this->cnfg_bioz_lc.bit.cmres = 0b1110; 
      this->BIOZ_cmres = 5500;
    } else if (cmres <= 7000) {
      this->cnfg_bioz_lc.bit.cmres = 0b1101; 
      this->BIOZ_cmres = 6500;
    } else if (cmres <= 8750) {
      this->cnfg_bioz_lc.bit.cmres = 0b1100; 
      this->BIOZ_cmres = 7500;
    } else if (cmres <= 11250) {
      this->cnfg_bioz_lc.bit.cmres = 0b1011; 
      this->BIOZ_cmres = 10000;
    } else if (cmres <= 16250) {
      this->cnfg_bioz_lc.bit.cmres = 0b1010; 
      this->BIOZ_cmres = 12500;
    } else if (cmres <= 30000) {
      this->cnfg_bioz_lc.bit.cmres = 0b1001; 
      this->BIOZ_cmres = 20000;
    } else if (cmres <= 60000) {
      this->cnfg_bioz_lc.bit.cmres = 0b1000; 
      this->BIOZ_cmres = 40000;
    } else if (cmres <= 90000) {
      this->cnfg_bioz_lc.bit.cmres = 0b0111; 
      this->BIOZ_cmres = 80000;
    } else if (cmres <= 13000) {
      this->cnfg_bioz_lc.bit.cmres = 0b0101; 
      this->BIOZ_cmres = 100000;
    } else if (cmres <= 240000) {
      this->cnfg_bioz_lc.bit.cmres = 0b0011; 
      this->BIOZ_cmres = 160000;
    } else if (cmres >240000) {
      this->cnfg_bioz_lc.bit.cmres = 0b0001; 
      this->BIOZ_cmres = 320000;
    }

    LOGln("BioZ common mode current feedback resistance for low current set to %5u [kΩ]", this->BIOZ_cmres);
  }

  this->writeRegister(MAX30001_CNFG_BIOZ,    this->cnfg_bioz.all);
  this->writeRegister(MAX30001_CNFG_BIOZ_LC, this->cnfg_bioz_lc.all);

  LOGln("BioZ current set to %5u [nA]", this->BIOZ_cgmag);
}

void MAX30001G::setBIOZPhaseOffset(uint8_t selector){
/**
  * Freq   FCGEN  Phase               Phase selector.
  * lower  other  0.. 11.25 ..168.75  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
  * 80kHz  0b0001 0.. 22.5  ..157.50  0,1,2,3,4,5,6,7
  * 128kHz 0b0000 0.. 45.0  ..135     0,1,2,3
  * 
  * BIOZ_phase is a global variable representing current phase setting
  * 
  * For quadrature measurement we want phase at 0 and 90deg
  *   magnitude is sqrt(R_0^2 + R_90^2)
  *   phase is atan(R_90/R_0)
  * 
  * If we measure multiple phases we need to fit curve
  *   measured R(offset) = mag*cos(phase+offset) 
  */

  // Ensure selector is within valid range (0 to 15)
  if (selector > 15) {
    LOGW("Invalid phase offset selector. Must be between 0 and 15.");
    return;
  }

  this->cnfg_bioz.all = this->readRegister24(MAX30001_CNFG_BIOZ);
  
  this->cnfg_bioz.bit.phoff = selector;

  switch (this->cnfg_bioz.bit.fcgen) {
    case 0b0000: // 45-degree phase steps
      // 128kHz
      if selector > 3 {
        selector = 3;
        LOGW("Phase offset selector limited to 3 for 128kHz modulation frequency.");
      }
      this->BIOZ_phase = selector * 45.0;
      break;
    case 0b0001: // 22.5-degree phase steps
      // 80kHz
      if selector > 7 {
        selector = 7;
        LOGW("Phase offset selector limited to 7 for 80kHz modulation frequency.");
      }
      this->BIOZ_phase = selector * 22.5;
      break;
    default:     // Other values use 11.25-degree phase steps
      // 40kHz, 18kHz, 8kHz, 4kHz, 2kHz, 1kHz, 0.5kHz, 0.25kHz, 0.125kHz
      if selector > 15 {
        selector = 15;
        LOGW("Phase offset selector limited to 15 for 40kHz and lower modulation frequencies.");
      }
      this->BIOZ_phase = selector * 11.25;
      break;
  }

  // Store register
  this->writeRegister(MAX30001_CNFG_BIOZ, this->cnfg_bioz.all);
  
  // Log the updated phase for debugging
  LOGln("BIOZ phase set to %.2f degrees", this->BIOZ_phase);
}

void MAX30001G::setBIOZfilter(uint8_t ahpf, uint8_t lpf, uint8_t hpf) {
/*
 analog high pass is 
  [6,7]  0 Hz
  [0]   60 Hz
  [1]  150 Hz
  [2]  500 Hz * default
  [3] 1000 Hz
  [4] 2000 Hz 
  [5] 4000 Hz
  analog filter is located before demodulator

 digital low pass is about 
  [0] 0 
  [1] 4 * default
  [2] 8
  [3] 16Hz 
  depending on FMSTR and BIOZ sampling rate

 digital high pass is 
  [0] 0 Hz * default
  [1] 0.05 Hz 
  [2] 0.5 Hz
 
 
 digital filters are after demodulator
*/

  this->cnfg_bioz.all  = this->readRegister24(MAX30001_CNFG_BIOZ);

  if (hpf == 0) {
    this->cfg_bioz.bit.dhpf = 0b00;
    this->BIOZ_hpf = 0.;
  } else if (hpf ==1) {
    this->cfg_bioz.bit.dhpf = 0b01;
    this->BIOZ_hpf = 0.05;
  } else if (hpf >= 2) {
    this->cfg_bioz.bit.dhpf = 0b10;
    this->BIOZ_hpf = 0.5;
  }

  if (lpf == 0) {
    this->cfg_bioz.bit.dlpf = 0b00;
  } else if (hpf == 1) {
    this->cfg_bioz.bit.dlpf = 0b01;
  } else if (hpf == 2) {
    this->cfg_bioz.bit.dlpf = 0b10;
  } else if (hpf == 3) {
    this->cfg_bioz.bit.dlpf = 0b10;
  }
  updateGlobalBIOZ_lpf();

  switch (ahpf) {
   case 0:
    this->cfg_bioz.bit.ahpf = 0b000;
    this->BIOZ_ahpf = 60.;
    break;

   case 1:
    this->cfg_bioz.bit.ahpf = 0b001;
    this->BIOZ_ahpf = 150.;
    break;

   case 2:
    this->cfg_bioz.bit.ahpf = 0b010;
    this->BIOZ_ahpf = 500.;
    break;

   case 3:
    this->cfg_bioz.bit.ahpf = 0b011;
    this->BIOZ_ahpf = 1000.;
    break;

   case 4:
    this->cfg_bioz.bit.ahpf = 0b100;
    this->BIOZ_ahpf = 2000.;
    break;

   case 5:
    this->cfg_bioz.bit.ahpf = 0b101;
    this->BIOZ_ahpf = 4000.;
    break;

   default:
    this->cnfg_bioz.bit.ahpf = 0b110; // Set to 0 Hz, bypass filter
    this->BIOZ_ahpf = 0.0;
    break;
  }

  this->writeRegister(MAX30001_CNFG_BIOZ, this->cnfg_bioz.all);

  LOGln("BIOZ filter configured: HPF=%.2f Hz, LPF=%.2f Hz, AHPF=%.2f Hz", this->BIOZ_hpf, this->BIOZ_lpf, this->BIOZ_ahpf);

}

void MAX30001G::setBIOZmodulation(uint8_t mode) {
/*
 0 = Unchopped Sources with Low Pass Filter * default
     (higher noise, excellent 50/60Hz rejection, recommended for ECG, BioZ applications)
 1 = Chopped Sources without Low Pass Filter
     (low noise, no 50/60Hz rejection, recommended for BioZ applications with digital LPF, possibly battery powered ECG, BioZ applications)
 2 = Chopped Sources with Low Pass Filter
     (low noise, excellent 50/60Hz rejection)
 3 = Chopped Sources with Resistive CM
     (Not recommended to be used for drive currents >32µA, low noise, excellent 50/60Hz rejection, lower input impedance)
 */

  if (mode > 3) {
    LOGln("Invalid mode value. It must be between 0 and 3.");
    return; // Exit the function if the mode is invalid
  }
  
  // Read the current BMUX register
  this->cnfg_bmux.all = this->readRegister24(MAX30001_CNFG_BMUX);

  // Set the modulation mode to chopped with resistive CM
  this->cnfg_bmux.bit.cg_mode = mode; // Assuming 0b11 is for "chopped with resistive CM"

  // Write back the modified register
  this->writeRegister(MAX30001_CNFG_BMUX, this->cnfg_bmux.all);

  switch (mode) {
    case 0:
      LOGln("BIOZ modulation mode set to: Unchopped Sources with Low Pass Filter.");
      break;
    case 1:
      LOGln("BIOZ modulation mode set to: Chopped Sources without Low Pass Filter.");
      break;
    case 2:
      LOGln("BIOZ modulation mode set to: Chopped Sources with Low Pass Filter.");
      break;
    case 3:
      LOGln("BIOZ modulation mode set to: Chopped Sources with Resistive CM.");
      break;
  }
}

void MAX30001G::setBIOZImpedanceTest(bool enable, bool useHighResistance, bool enableModulation, uint8_t resistance, uint8_t rmodValue,  uint8_t modFreq) {
    /*
    Configures the BIOZ impedance test.
    This function attaches an internal resistance to the BIOZ pins (BIP and BIN).

    Parameters:
    - enable/disable use of any internal impedance test
    - useHighResistance: true to use high-resistance load (27 kΩ to 1029 kΩ), false to use low-resistance modulated load.
    - resistance
        high resistance
          0 00 =   27 kΩ
          1 01 =  108 kΩ
          2 10 =  487 kΩ
          3 11 = 1029 kΩ
        low resistance
          0 000 =    5 k
          1 001 =    2.5k
          2 010 =    1.667k
          3 011 =    1.25k
          4 100 =    1k
          5 101 =    0.833k
          6 110 =    0.714k
          7 111 =    0.625k

    - enableModulation: true to enable modulated resistance, false to disable.
        For low resistance values between 625Ω and 5kΩ, the resistance can be modulated. 
        Rewsistance will switch between RNOM and (RNOM - RMOD) at the selected modulation rate. 
    - rnomValue: Nominal resistance setting (0 to 7). Only applicable when using low resistance.
    - rmodValue: Modulated resistance setting (0 to 7). Only applicable when using low resistance.    
    - modFreq: resistance modulation frequency (0 to 3). Only applicable when using low resistance.

    cnfg_bioz_lc.bit.en_bistr
      Enable High-Resistance Programmable Load Value
        0 = Disable high-resistance programmable load
        1 = Enable high-resistance programmable load

    cnfg_bioz_lc.bit.bistr
      Select High-Resistance Programmable Load Value
        00 = 27 kΩ
        01 = 108 kΩ
        10 = 487 kΩ
        11 = 1029 kΩ

    cnfg_bmux.bit.en_bist
      BioZ Modulated Resistance Built-In-Self-Test (RMOD BIST) Mode Enable
        0 = RMOD BIST Disabled
        1 = RMOD BIST Enabled
      Note: Available only when CNFG_CAL -> EN_VCAL = 0 (Calibration Source (VCALP and VCALN) disabled)
      To avoid body interference, the BIP/N switches should be open in this mode.
      When enabled, the DRVP/N isolation switches are opened and the DRVP/N-to-BIP/N internal switches are engaged. 
      Also, the lead bias resistors are applied to the BioZ inputs in 200MΩ mode.

    cnfg_bmux.bit.fbist
      BioZ RMOD BIST Frequency Selection. Not applicable for the high-resistance (27kΩ to 1029kΩ) values.
      Calibration Source Frequency Selection (FCAL)
      00 = fMSTR/213 (Approximately 4 Hz)
      01 = fMSTR/215 (Approximately 1 Hz)
      10 = fMSTR/217 (Approximately 1/4 Hz) 
      11 = fMSTR/219 (Approximately 1/16 Hz)
      Actual frequencies are determined by FMSTR selection (see CNFG_GEN for details),
       approximate frequencies are based on a 32,768 Hz clock (FMSTR[1:0]=00). All selections
       use 50% duty cycle


    cnfg_bmux.bit.rmod
    cnfg_bmux.bit.rnorm
      BioZ RMOD BIST Nominal Resistance Selection. For higher resistance values, see BIOZ_CNFG_LC.
      See RMOD BIST Settings Table for details.

      BMUX_RNOM[2:0]  BMUX_RMOD[2:0]  NOMINAL RESISTANCE (Ω)  MODULATED RESISTANCE (mΩ)
      000             000             5000                    2960
                      001                                     980.5
                      010                                     247.5
                      1xx                                     none
      001             000             2500                    740.4
                      001                                     245.2
                      010                                     61.9
                      1xx                                     none
      010             000             1667                    329.1
                      001                                     109.0
                      010                                     27.5
                      1xx                                     none
      011             000             1250                    185.1
                      001                                     61.3
                      010                                     none
                      1xx                                     none
      100             000             1000                    118.5
                      001                                     39.2
                      010                                     none
                      1xx                                     none
      101             000              833                    82.3
                      001                                     27.2
                      010                                     none
                      1xx                                     none
      110             000              714                    60.5
                      001                                     20.0
                      010                                     none
                      1xx                                     none
      111             000              625                    46.3
                      001                                     15.3
                      010                                     none
                      1xx                                     none

    cnfg_cal.bit.en_vcal
      Calibration Source (VCALP and VCALN) Enable
      0 = Calibration sources and modes disabled
      1 = Calibration sources and modes enabled

  */


    // Read the current register settings
    this->cnfg_bmux.all = this->readRegister24(MAX30001_CNFG_BMUX);
    this->cnfg_bioz_lc.all = this->readRegister24(MAX30001_CNFG_BIOZ_LC);
    this->cnfg_cal.all = this->readRegister24(MAX30001_CNFG_CAL);

    if (enable) {

      this->cnfg_cal.bit.en_cal= 0 // disable calibration voltage source VCALP and VCALN 

      if (useHighResistance) {
        // Enable built-in high resistance test
        this->cnfg_bioz_lc.bit.en_bistr = 1;
        // Set resistance value
        this->cnfg_bioz_lc.bit.enableModulationt.bistr = resistance & 0x03;
        if (enableModulation) {
          LOGW("High resistance test does not support modulation.Disabling modulation");
        }
        // Modulation is not possible
        this->cnfg_bmux.bit.rmod = 0; // Dsiable modulation resistor byu setting it to 0
        this->cnfg_bmux.bit.en_bist = 0; // Disable modulation
        LOGln("High resistance test enabled with resistance value: %u", resistance);

     } else {
        // useLow Resistance
        this->cnfg_bmux.bit.en_bist = 1;
        this->cnfg_bmux.bit.rnorm = resistance & 0x07; // nominal resistance 0..7
        LOGln("Low resistance test enabled with nominal resistance: %u", resistance);

        // Modulate resistance if requested
        if (modulate) {
            // Enable modulation, set modulation value and frequency
            this->cnfg_bmux.bit.rmod = rmodValue & 0x07; // Set modulated resistance value
            this->cnfg_bmux.bit.fbist = modFreq & 0x03; // Set modulation frequency
            updateGlobalRCAL_freq(); // Update the frequency for calibration
            LOGln("Modulation enabled with rmodValue: %u and modFreq: %u", rmodValue, modFreq);
        } else {
            // Disable modulation
            this->cnfg_bmux.bit.rmod = 0;
            LOGln("Modulation disabled.");
        }
      }

    } else {
        // Disable built-in resistance test
        this->cnfg_bioz_lc.bit.en_bistr = 0;
        this->cnfg_bmux.bit.en_bist =0;
        LOGln("BIOZ impedance test disabled.");
    }

    // Write back the modified registers
    this->writeRegister(MAX30001_CNFG_BMUX, this->cnfg_bmux.all);
    this->writeRegister(MAX30001_CNFG_BIOZ_LC, this->cnfg_bioz_lc.all);
    this->writeRegister(MAX30001_CNFG_CAL, this->cnfg_cal.all);
}

void MAX30001G::setDefaultBIOZImpedanceTest() {
  setBIOZImpedanceTest(
    true,  // enable 
    false, // useHighResistance
    true,  // enableModulation
    0b000, // resistance 5kOhm 
    0b000, // 3 Ohm, switches between 5k Ohm and 5k - 3 Ohm
    0b01,  // modFreq 1 Hz
    );
}

/******************************************************************************************************/
// Registers
/******************************************************************************************************/
void MAX30001G::readAllRegisters() {
  this->status.all       = this->readRegister24(MAX30001_STATUS);
  this->en_int1.all      = this->readRegister24(MAX30001_EN_INT1);
  this->en_int2.all      = this->readRegister24(MAX30001_EN_INT2);
  this->mngr_int.all     = this->readRegister24(MAX30001_MNGR_INT);
  this->mngr_dyn.all     = this->readRegister24(MAX30001_MNGR_DYN);
  this->info.all         = this->readRegister24(MAX30001_INFO);
  this->cnfg_gen.all     = this->readRegister24(MAX30001_CNFG_GEN);
  this->cnfg_cal.all     = this->readRegister24(MAX30001_CNFG_CAL);
  this->cnfg_emux.all    = this->readRegister24(MAX30001_CNFG_EMUX);
  this->cnfg_ecg.all     = this->readRegister24(MAX30001_CNFG_ECG);
  this->cnfg_bmux.all    = this->readRegister24(MAX30001_CNFG_BMUX);
  this->cnfg_bioz.all    = this->readRegister24(MAX30001_CNFG_BIOZ);
  this->cnfg_bioz_lc.all = this->readRegister24(MAX30001_CNFG_BIOZ_LC);
  this->cnfg_rtor1.all   = this->readRegister24(MAX30001_CNFG_RTOR1);
  this->cnfg_rtor2.all   = this->readRegister24(MAX30001_CNFG_RTOR2);
}

void MAX30001G::dumpRegisters() {
  /*
  Read and report all known register
  */
  printStatus();
  printEN_INT(en_int1);
  printEN_INT(en_int2);
  printMNGR_INT();
  printMNGR_DYN();
  printInfo();
  printCNFG_GEN();
  printCNFG_CAL();
  printCNFG_EMUX();
  printCNFG_ECG();
  printCNFG_BMUX();
  printCNFG_BIOZ();
  printCNFG_BIOZ_LC();
  printCNFG_RTOR1();
  printCNFG_RTOR2();
}

void MAX30001::readInfo(void)
{
    /*
    Read content of information register.
    This should contain the revision number of the chip in info.revision.
    It should also include part ID but it's not clear how this is stored.
    */
    this->info.all = this->readRegister24(MAX30001_INFO);
}

void MAX30001G::readStatusRegisters() {
  // Read status registers to check for over/under-voltage conditions
  this->status.all = this->readRegister24(MAX30001_STATUS);

  // Check over-voltage and under-voltage conditions
  this->over_voltage_detected = (this->status.bit.bover == 1);
  this->under_voltage_detected = (this->status.bit.bunder == 1);
  //this-> = (this->status.bit.bovf == 1);
  //this-> = (this->status.bit.eovf == 1);

}

void MAX30001::printStatus() {
  LOGln("MAX30001 Status Register:");
  LOGln("----------------------------");

  LOGln("ECG ------------------------");

  if (this->status.bit.dcloffint == 1) {
    LOGln("Lead ECG leads are off:");
    if (this->status.bit.ldoff_nl == 1) {
      LOGln("Lead ECGN below VTHL");
    } else if (this->status.bit.ldoff_nh == 1) {
      LOGln("Lead ECGN above VTHH");
    }
    if (this->status.bit.ldoff_pl == 1) {
      LOGln("Lead ECGP below VHTL");
    } else if (this->status.bit.ldoff_ph == 1) {
      LOGln("Lead ECGP above VHTH");
    }
  } else {
    LOGln("Lead ECG leads are on.");
  }

  LOGln("ECG FIFO interrupt is %s", this->status.bit.eint ? "on" : "off");
  LOGln("ECG FIFO overflow is %s",  this->status.bit.eovf ? "on" : "off");
  LOGln("ECG Sample interrupt %s",  this->status.bit.samp ? "occurred" : "not present");
  LOGln("ECG R to R interrupt %s",  this->status.bit.print ? "occurred" : "not present");
  LOGln("ECG Fast Recovery interrupt is %s", this->status.bit.fstint ? "on" : "off");

  LOGln("BIOZ -----------------------");

  if (this->status.bit.bcgmon == 1) {
    LOGln("BIOZ leads are off.");
    if (this->status.bit.bcgmn == 1) { LOGln("BIOZ N lead off"); }
    if (this->status.bit.bcgmp == 1) { LOGln("BIOZ P lead off"); }
    if (this->status.bit.bundr == 1) { LOGln("BIOZ output magnitude under BIOZ_LT (4 leads)"); }
    if (this->status.bit.bover == 1) { LOGln("BIOZ output magnitude over BIOZ_HT (4 leads)"); }
  } else {
    LOGln("BIOZ leads are on.");
  }

  LOGln("BIOZ ultra low power leads interrupt is %s", this->status.bit.lonint ? "on" : "off");
  LOGln("PLL %s", this->status.bit.pllint ? "has lost signal" : "is working");
}

void MAX30001::printEN_INT(void) {
  LOGln("MAX30001 Interrupts:");
  LOGln("----------------------------");
  if (this->en_int.bit.int_type == 0) {
    LOGln("Interrupts are disabled");
  } else if (this->en_int.bit.int_type == 1) {
    LOGln("Interrupt is CMOS driver");
  } else if (this->en_int.bit.int_type == 2) {
    LOGln("Interrupt is Open Drain driver");
  } else if (this->en_int.bit.int_type == 3) {
    LOGln("Interrupt is Open Drain with 125k pullup driver");
  }
  LOGln("PLL interrupt is                                %s", this->en_int.bit.en_pllint    ? "enabled" : "disabled");
  LOGln("Sample synch pulse is                           %s", this->en_int.bit.en_samp      ? "enabled" : "disabled");
  LOGln("R to R detection interrupt is                   %s", this->en_int.bit.en_rrint     ? "enabled" : "disabled");
  LOGln("Ultra low power leads on detection interrupt is %s", this->en_int.bit.en_lonint    ? "enabled" : "disabled");
  LOGln("BIOZ current monitor interrupt is               %s", this->en_int.bit.en_bcgmon    ? "enabled" : "disabled");
  LOGln("BIOZ under range interrupt is                   %s", this->en_int.bit.en_bunder    ? "enabled" : "disabled");
  LOGln("BIOZ over range interrupt is                    %s", this->en_int.bit.en_bover     ? "enabled" : "disabled");
  LOGln("BIOZ FIFO overflow interrupt is                 %s", this->en_int.bit.en_bovf      ? "enabled" : "disabled");
  LOGln("BIOZ FIFO interrupt is                          %s", this->en_int.bit.en_bint      ? "enabled" : "disabled");
  LOGln("ECG dc lead off interrupt is                    %s", this->en_int.bit.en_dcloffint ? "enabled" : "disabled");
  LOGln("ECG fast recovery interrupt is                  %s", this->en_int.bit.en_fstint    ? "enabled" : "disabled");
  LOGln("ECG FIFO overflow interrupt is                  %s", this->en_int.bit.en_eovf      ? "enabled" : "disabled");
  LOGln("ECG FIFO interrupt is                           %s", this->en_int.bit.en_eint      ? "enabled" : "disabled");
}

void MAX30001::printMNGR_INT() {
  LOGln("MAX30001 Interrupt Management:");
  LOGln("----------------------------");

  if (this->mngr_int.bit.samp_it == 0) {
    LOGln("Sample interrupt on every sample");
  } else if (this->mngr_int.bit.samp_it == 1) {
    LOGln("Sample interrupt on every 2nd sample");
  } else if (this->mngr_int.bit.samp_it == 2) {
    LOGln("Sample interrupt on every 4th sample");
  } else if (this->mngr_int.bit.samp_it == 3) {
    LOGln("Sample interrupt on every 16th sample");
  }

  LOGln("Sample synchronization pulse is cleared %s", this->mngr_int.bit.clr_samp ? "automatically" : "on status read");

  if (this->mngr_int.bit.clr_rrint == 0) {
    LOGln("RtoR interrupt is cleared on status read");
  } else if (this->mngr_int.bit.clr_rrint == 1) {
    LOGln("RtoR interrupt is cleared on RTOR read");
  } else if (this->mngr_int.bit.clr_rrint == 2) {
    LOGln("RtoR interrupt is cleared automatically");
  } else {
    LOGln("RtoR interrupt clearance is not defined");
  }

  LOGln("FAST_MODE interrupt %s", this->mngr_int.bit.clr_fast ? "remains on until FAST_MODE is disengaged" : "is cleared on status read");
  LOGln("BIOZ FIFO interrupt after %u samples", this->mngr_int.bit.b_fit + 1);
  LOGln("ECG FIFO interrupt after %u samples", this->mngr_int.bit.e_fit + 1);
}

void MAX30001::printMNGR_DYN() {
  LOGln("MAX30001 Dynamic Modes:");
  LOGln("----------------------------");

  if (this->cnfg_gen.bit.en_bloff >= 2) {
    LOGln("BIOZ lead off high threshold: +/- %u * 32", this->mngr_dyn.bit.bloff_hi_it);
    LOGln("BIOZ lead off low threshold: +/- %u * 32", this->mngr_dyn.bit.bloff_lo_it);
  } else {
    LOGln("BIOZ lead off thresholds not applicable");
  }

  if (this->mngr_dyn.bit.fast == 0) {
    LOGln("ECG fast recovery is disabled");
  } else if (this->mngr_dyn.bit.fast == 1) {
    LOGln("ECG manual fast recovery is enabled");
  } else if (this->mngr_dyn.bit.fast == 2) {
    LOGln("ECG automatic fast recovery is enabled");
    LOGln("ECG fast recovery threshold is %u", 2048 * this->mngr_dyn.bit.fast_th);
  } else {
    LOGln("ECG fast recovery is not defined");
  }
}

void MAX30001::printInfo()
{
  /*
  Print the information register
  */
  LOGln("MAX30001 Information Register:");
  LOGln("----------------------------");
  LOGln("Nibble 1:       %u", this->info.bit.n1);
  LOGln("Nibble 2:       %u", this->info.bit.n2);
  LOGln("Nibble 3:       %u", this->info.bit.n3);
  LOGln("Constant 1: (should be 1) %u", this->info.bit.c1);
  LOGln("2 Bit Value:    %u", this->info.bit.n4);
  LOGln("Revision:       %u", this->info.bit.revision);
  LOGln("Constant 2: (should be 5) %u", this->info.bit.c2);
}

void MAX30001::printCNFG_GEN()
{
  LOGln("MAX30001 General Config:");
  LOGln("----------------------------");
  LOGln("ECG  is %s", this->cnfg_gen.bit.en_ecg ? "enabled" : "disabled");
  LOGln("BIOZ is %s", this->cnfg_gen.bit.en_bioz ? "enabled" : "disabled");

  switch (this->cnfg_gen.bit.fmstr) {
    case 0:
      LOGln("FMSTR is 32768Hz, global var: %f", this->fmstr);
      LOGln("TRES is 15.26us, global var: %f", this->tres);
      LOGln("ECG progression is 512Hz, global var: %f", this->ECG_progression);
      break;
    case 1:
      LOGln("FMSTR is 32000Hz, global var: %f", this->fmstr);
      LOGln("TRES is 15.63us, global var: %f", this->tres);
      LOGln("ECG progression is 500Hz, global var: %f", this->ECG_progression);
      break;
    case 2:
      LOGln("FMSTR is 32000Hz, global var: %f", this->fmstr);
      LOGln("TRES is 15.63us, global var: %f", this->tres);
      LOGln("ECG progression is 200Hz, global var: %f", this->ECG_progression);
      break;
    case 3:
      LOGln("FMSTR is 31968.78Hz, global var: %f", this->fmstr);
      LOGln("TRES is 15.64us, global var: %f", this->tres);
      LOGln("ECG progression is 199.8049Hz, global var: %f", this->ECG_progression);
      break;
    default:
      LOGln("FMSTR is undefined");
      break;
  }

  LOGln("--------------------------");

  if (this->cnfg_gen.bit.en_rbias > 0) {
    if (this->cnfg_gen.bit.en_rbias == 1 && this->cnfg_gen.bit.en_ecg == 1) {
      LOGln("ECG bias resistor is enabled");
    } else if (this->cnfg_gen.bit.en_rbias == 2 && this->cnfg_gen.bit.en_bioz == 1) {
      LOGln("BIOZ bias resistor is enabled");
    } else {
      LOGln("ECG and BIOZ bias resistors are undefined");
    }
    LOGln("N bias resistance is %s", this->cnfg_gen.bit.rbiasn ? "enabled" : "disabled");
    LOGln("P bias resistance is %s", this->cnfg_gen.bit.rbiasp ? "enabled" : "disabled");
    switch (this->cnfg_gen.bit.rbiasv) {
      case 0: LOGln("bias resistor is 50MOhm"); break;
      case 1: LOGln("bias resistor is 100MOhm"); break;
      case 2: LOGln("bias resistor is 200MOhm"); break;
      default: LOGln("bias resistor is not defined"); break;
    }
  } else {
    LOGln("ECG and BIOZ bias resistors are disabled");
  }

  LOGln("--------------------------");

  if (this->cnfg_gen.bit.en_dcloff == 1) {
    LOGln("ECG lead off detection is enabled");
    switch (this->cnfg_gen.bit.vth) {
      case 0: LOGln("ECG lead off/on threshold is VMID +/-300mV"); break;
      case 1: LOGln("ECG lead off/on threshold is VMID +/-400mV"); break;
      case 2: LOGln("ECG lead off/on threshold is VMID +/-450mV"); break;
      case 3: LOGln("ECG lead off/on threshold is VMID +/-500mV"); break;
      default: LOGln("ECG lead off/on threshold is not defined"); break;
    }
    switch (this->cnfg_gen.bit.imag) {
      case 0: LOGln("ECG lead off/on current source is 0nA"); break;
      case 1: LOGln("ECG lead off/on current source is 5nA"); break;
      case 2: LOGln("ECG lead off/on current source is 10nA"); break;
      case 3: LOGln("ECG lead off/on current source is 20nA"); break;
      case 4: LOGln("ECG lead off/on current source is 50nA"); break;
      case 5: LOGln("ECG lead off/on current source is 100nA"); break;
      default: LOGln("ECG lead off/on current source is not defined"); break;
    }
    LOGln("ECG lead off/on polarity is %s", this->cnfg_gen.bit.ipol ? "P is pull down" : "P is pull up");
  } else {
    LOGln("ECG lead off detection is disabled");
  }

  LOGln("--------------------------");

  switch (this->cnfg_gen.bit.en_ulp_lon) {
    case 0: LOGln("ECG Ultra low power leads on detection is disabled"); break;
    case 1: LOGln("ECG Ultra low power leads on detection is enabled"); break;
    default: LOGln("ECG Ultra low power leads on detection is not defined"); break;
  }

  LOGln("--------------------------");

  if (this->cnfg_gen.bit.en_bloff > 0) {
    LOGln("BIOZ lead off detection is enabled");
    switch (this->cnfg_gen.bit.en_bloff) {
      case 1: LOGln("BIOZ lead off/on is under range detection for 4 electrodes operation"); break;
      case 2: LOGln("BIOZ lead off/on is over range detection for 2 and 4 electrodes operation"); break;
      case 3: LOGln("BIOZ lead off/on is under and over range detection for 4 electrodes operation"); break;
      default: LOGln("BIOZ lead off/on detection is not defined"); break;
    }
  } else {
    LOGln("BIOZ lead off detection is disabled");
  }
}

void MAX30001::printCNFG_CAL() {
    LOGln("MAX30001 Internal Voltage Calibration Source:");
    LOGln("---------------------------------------------");

    if (this->cnfg_cal.bit.vcal == 1) {
      LOGln("Internal voltage calibration source is enabled");
      LOGln("Voltage calibration source is %s", this->cnfg_cal.bit.vmode ? "bipolar" : "unipolar");
      LOGln("Magnitude is %s", this->cnfg_cal.bit.vmag ? "0.5mV" : "0.25mV");
      switch (this->cnfg_cal.bit.fcal) {
        case 0: LOGln("Frequency is %u Hz", this->fmstr / 128); break;
        case 1: LOGln("Frequency is %u Hz", this->fmstr / 512); break;
        case 2: LOGln("Frequency is %u Hz", this->fmstr / 2048); break;
        case 3: LOGln("Frequency is %u Hz", this->fmstr / 8192); break;
        case 4: LOGln("Frequency is %u Hz", this->fmstr / 32768); break;
        case 5: LOGln("Frequency is %u Hz", this->fmstr / 131072); break;
        case 6: LOGln("Frequency is %u Hz", this->fmstr / 524288); break;
        case 7: LOGln("Frequency is %u Hz", this->fmstr / 2097152); break;
        default: LOGln("Frequency is not defined"); break;
      }
  } else {
    LOGln("Internal voltage calibration source is disabled");
  }

  if (this->cnfg_cal.bit.fifty == 1) {
    LOGln("50%% duty cycle");
  } else {
    LOGln("Pulse length %u [us]", this->cnfg_cal.bit.thigh * this->CAL_resolution);
  }
}

void MAX30001::printCNFG_EMUX() {
  LOGln("MAX30001 ECG multiplexer:");
  LOGln("-------------------------");

  if (this->cnfg_emux.bit.caln_sel == 0) {
      LOGln("ECG N is not connected to calibration signal");
  } else if (this->cnfg_emux.bit.caln_sel == 1) {
      LOGln("ECG N is connected to V_MID");
  } else if (this->cnfg_emux.bit.caln_sel == 2) {
      LOGln("ECG N is connected to V_CALP");
  } else if (this->cnfg_emux.bit.caln_sel == 3) {
      LOGln("ECG N is connected to V_CALN");
  }

  if (this->cnfg_emux.bit.calp_sel == 0) {
      LOGln("ECG P is not connected to calibration signal");
  } else if (this->cnfg_emux.bit.calp_sel == 1) {
      LOGln("ECG P is connected to V_MID");
  } else if (this->cnfg_emux.bit.calp_sel == 2) {
      LOGln("ECG P is connected to V_CALP");
  } else if (this->cnfg_emux.bit.calp_sel == 3) {
      LOGln("ECG P is connected to V_CALN");
  }

  LOGln("ECG N is %s from AFE", this->cnfg_emux.bit.openn ? "disconnected" : "connected");
  LOGln("ECG P is %s from AFE", this->cnfg_emux.bit.openp ? "disconnected" : "connected");
  LOGln("ECG input polarity is %s", this->cnfg_emux.bit.pol ? "inverted" : "not inverted");
}

void MAX30001::printCNFG_ECG() {
  LOGln("MAX30001 ECG settings:");
  LOGln("----------------------");

  // Checking digital low pass filter settings
  if (this->cnfg_gen.bit.fmstr == 0) {
    if (this->cnfg_ecg.bit.rate == 0) {
        LOGln("ECG digital low pass filter is bypassed, global: %.2f", this->ECG_lpf);
    } else if (this->cnfg_ecg.bit.rate == 1) {
      if (this->cnfg_ecg.bit.dlpf == 0) {
        LOGln("ECG digital low pass filter is bypassed, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 1) {
        LOGln("ECG digital low pass filter is 40.96Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 2) {
        LOGln("ECG digital low pass filter is 102.4Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 3) {
        LOGln("ECG digital low pass filter is 153.6Hz, global: %.2f", this->ECG_lpf);
      }
    } else if (this->cnfg_ecg.bit.rate == 2) {
      if (this->cnfg_ecg.bit.dlpf == 0) {
        LOGln("ECG digital low pass filter is bypassed, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 1) {
        LOGln("ECG digital low pass filter is 40.96Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 2) {
        LOGln("ECG digital low pass filter is 102.4Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 3) {
        LOGln("ECG digital low pass filter is 40.96Hz, global: %.2f", this->ECG_lpf);
      }
    }
  } else if (this->cnfg_gen.bit.fmstr == 1) {
    if (this->cnfg_ecg.bit.rate == 0) {
        LOGln("ECG digital low pass filter is bypassed, global: %.2f", this->ECG_lpf);
    } else if (this->cnfg_ecg.bit.rate == 1) {
      if (this->cnfg_ecg.bit.dlpf == 0) {
        LOGln("ECG digital low pass filter is bypassed, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 1) {
        LOGln("ECG digital low pass filter is 40.00Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 2) {
        LOGln("ECG digital low pass filter is 100.0Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 3) {
        LOGln("ECG digital low pass filter is 150.0Hz, global: %.2f", this->ECG_lpf);
      }
    } else if (this->cnfg_ecg.bit.rate == 2) {
      if (this->cnfg_ecg.bit.dlpf == 0) {
        LOGln("ECG digital low pass filter is bypassed, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 1) {
        LOGln("ECG digital low pass filter is 27.68Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 2) {
        LOGln("ECG digital low pass filter is 27.68Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 3) {
        LOGln("ECG digital low pass filter is 27.68Hz, global: %.2f", this->ECG_lpf);
      }
    }
  } else if (this->cnfg_gen.bit.fmstr == 2) {
    if (this->cnfg_ecg.bit.rate == 2) {
      if (this->cnfg_ecg.bit.dlpf == 0) {
        LOGln("ECG digital low pass filter is bypassed, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 1) {
        LOGln("ECG digital low pass filter is 40.00Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 2) {
        LOGln("ECG digital low pass filter is 40.00Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 3) {
        LOGln("ECG digital low pass filter is 40.00Hz, global: %.2f", this->ECG_lpf);
      }
    } else {
      LOGln("ECG digital low pass filter is not set, global: %.2f", this->ECG_lpf);
    }
  } else if (this->cnfg_gen.bit.fmstr == 3) {
    if (this->cnfg_ecg.bit.rate == 2) {
      if (this->cnfg_ecg.bit.dlpf == 0) {
        LOGln("ECG digital low pass filter is bypassed, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 1) {
        LOGln("ECG digital low pass filter is 39.96Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 2) {
        LOGln("ECG digital low pass filter is 39.96Hz, global: %.2f", this->ECG_lpf);
      } else if (this->cnfg_ecg.bit.dlpf == 3) {
       LOGln("ECG digital low pass filter is 39.93Hz, global: %.2f", this->ECG_lpf);
      }
    } else {
      LOGln("ECG digital low pass filter is not set, global: %.2f", this->ECG_lpf);
    }
  }

  // Checking digital high pass filter settings
  LOGln("ECG digital high pass filter is %s Hz, global: %.2f", this->cnfg_ecg.bit.dhpf ? "bypassed" : "0.5", this->ECG_hpf);

  // Checking gain settings
  LOG("ECG gain is ");
  if (this->cnfg_ecg.bit.gain == 0) {
    LOGln("20V/V, global %d", this->ECG_gain);
  } else if (this->cnfg_ecg.bit.gain == 1) {
    LOGln("40V/V, global %d", this->ECG_gain);
  } else if (this->cnfg_ecg.bit.gain == 2) {
    LOGln("80V/V, global %d", this->ECG_gain);
  } else if (this->cnfg_ecg.bit.gain == 3) {
    LOGln("160V/V, global %d", this->ECG_gain);
  }

  // Checking data rate settings
  LOGln("ECG data rate is ");
  if (this->cnfg_ecg.bit.rate == 0) {
    if (this->cnfg_gen.bit.fmstr == 0b00) {
      LOGln("512SPS, global %d", this->ECG_samplingRate);
    } else if (this->cnfg_gen.bit.fmstr == 0b01) {
      LOGln("500SPS, global %d", this->ECG_samplingRate);
    } else {
      LOGln("not defined, global %d", this->ECG_samplingRate);
    }
  } else if (this->cnfg_ecg.bit.rate == 1) {
    if (this->cnfg_gen.bit.fmstr == 0b00) {
      LOGln("256SPS, global %d", this->ECG_samplingRate);
    } else if (this->cnfg_gen.bit.fmstr == 0b01) {
      LOGln("250SPS, global %d", this->ECG_samplingRate);
    } else {
      LOGln("not defined, global %d", this->ECG_samplingRate);
    }
  } else if (this->cnfg_ecg.bit.rate == 2) {
    if (this->cnfg_gen.bit.fmstr == 0b00) {
      LOGln("128SPS, global %d", this->ECG_samplingRate);
    } else if (this->cnfg_gen.bit.fmstr == 0b01) {
      LOGln("125SPS, global %d", this->ECG_samplingRate);
    } else if (this->cnfg_gen.bit.fmstr == 0b10) {
      LOGln("200SPS, global %d", this->ECG_samplingRate);
    } else if (this->cnfg_gen.bit.fmstr == 0b11) {
      LOGln("199.8SPS, global %d", this->ECG_samplingRate);
    }
  }
}

void MAX30001::printCNFG_BMUX() {
  LOGln("BIOZ MUX Configuration");
  LOGln("----------------------");

  LOGln("Calibration");
  LOG("BIOZ Calibration source frequency is ");
  if (this->cnfg_bmux.bit.fbist == 0) {
    LOGln("%5u", this->fmstr / (1 << 13));
  } else if (this->cnfg_bmux.bit.fbist == 1) {
    LOGln("%5u", this->fmstr / (1 << 15));
  } else if (this->cnfg_bmux.bit.fbist == 2) {
    LOGln("%5u", this->fmstr / (1 << 17));
  } else if (this->cnfg_bmux.bit.fbist == 3) {
    LOGln("%5u", this->fmstr / (1 << 19));
  }

  LOG("BIOZ nominal resistance is ");
  switch (this->cnfg_bmux.bit.rnom) {
    case 0:
      LOG("5000 Ohm with modulated resistance of ");
      switch (this->cnfg_bmux.bit.rmod) {
        case 0:
          LOGln("2960.7 mOhm");
          break;
        case 1:
          LOGln("980.6 mOhm");
          break;
        case 2:
          LOGln("247.5 mOhm");
          break;
        default:
          LOGln("unmodulated");
          break;
      }
      break;
    case 1:
      LOG("2500 Ohm with modulated resistance of ");
      switch (this->cnfg_bmux.bit.rmod) {
        case 0:
          LOGln("740.4 mOhm");
          break;
        case 1:
          LOGln("245.2 mOhm");
          break;
        case 2:
          LOGln("61.9 mOhm");
          break;
        default:
          LOGln("unmodulated");
          break;
      }
      break;
    case 2:
      LOG("1667 Ohm with modulated resistance of ");
      switch (this->cnfg_bmux.bit.rmod) {
        case 0:
          LOGln("329.1 mOhm");
          break;
        case 1:
          LOGln("109.0 mOhm");
          break;
        case 2:
          LOGln("27.5 mOhm");
          break;
        default:
          LOGln("unmodulated");
          break;
      }
      break;
    case 3:
      LOG("1250 Ohm with modulated resistance of ");
      switch (this->cnfg_bmux.bit.rmod) {
        case 0:
          LOGln("185.1 mOhm");
          break;
        case 1:
          LOGln("61.3 mOhm");
          break;
        default:
          LOGln("unmodulated");
          break;
      }
      break;
    case 4:
      LOG("1000 Ohm with modulated resistance of ");
      switch (this->cnfg_bmux.bit.rmod) {
        case 0:
          LOGln("118.5 mOhm");
          break;
        case 1:
          LOGln("39.2 mOhm");
          break;
        default:
          LOGln("unmodulated");
          break;
      }
      break;
    case 5:
      LOG("833 Ohm with modulated resistance of ");
      switch (this->cnfg_bmux.bit.rmod) {
        case 0:
          LOGln("82.3 mOhm");
          break;
        case 1:
          LOGln("27.2 mOhm");
          break;
        default:
          LOGln("unmodulated");
          break;
      }
      break;
    case 6:
      LOG("714 Ohm with modulated resistance of ");
      switch (this->cnfg_bmux.bit.rmod) {
        case 0:
          LOGln("60.5 mOhm");
          break;
        case 1:
          LOGln("20.0 mOhm");
          break;
        default:
          LOGln("unmodulated");
          break;
      }
      break;
    case 7:
      LOG("625 Ohm with modulated resistance of ");
      switch (this->cnfg_bmux.bit.rmod) {
        case 0:
          LOGln("46.3 mOhm");
          break;
        case 1:
          LOGln("15.3 mOhm");
          break;
        default:
          LOGln("unmodulated");
          break;
      }
      break;
  }

  LOGln("Built-in self-test %s", this->cnfg_bmux.bit.en_bist ? "enabled, BIP/N should be open." : "disabled");

  LOG("BIOZ current generator for ");
  switch (this->cnfg_bmux.bit.cg_mode) {
    case 0:
      LOGln("unchopped sources (ECG & BIOZ application), or low current range mode");
      break;
    case 1:
      LOGln("chopped sources, BIOZ without low pass filter");
      break;
    case 2:
      LOGln("chopped sources, BIOZ with low pass filter");
      break;
    case 3:
      LOGln("chopped sources, with resistive CM setting, low impedance");
      break;
  }

  LOGln("Connections");
  LOG("BIOZ N is connected to ");
  switch (this->cnfg_bmux.bit.caln_sel) {
    case 0:
      LOGln("no calibration");
      break;
    case 1:
      LOGln("V_MID");
      break;
    case 2:
      LOGln("V_CALP");
      break;
    case 3:
      LOGln("V_CALN");
      break;
  }
  
  LOG("BIOZ P is connected to ");
  switch (this->cnfg_bmux.bit.calp_sel) {
    case 0:
      LOGln("no calibration");
      break;
    case 1:
      LOGln("V_MID");
      break;
    case 2:
      LOGln("V_CALP");
      break;
    case 3:
      LOGln("V_CALN");
      break;
  }

  LOGln("BIOZ N is %s from AFE", this->cnfg_bmux.bit.openn ? "disconnected" : "connected");
  LOGln("BIOZ P is %s from AFE", this->cnfg_bmux.bit.openp ? "disconnected" : "connected");
}

void MAX30001::printCNFG_BIOZ() {
  LOGln("BIOZ Configuration");
  LOGln("----------------------");

  LOG("BIOZ phase offset is ");
  if (this->cnfg_bioz.bit.fcgen == 0) {
    LOGln("%f", this->cnfg_bioz.bit.phoff * 45.0);
  } else if (this->cnfg_bioz.bit.fcgen == 1) {
    LOGln("%f", this->cnfg_bioz.bit.phoff * 22.5);
  } else if (this->cnfg_bioz.bit.fcgen >= 2) {
    LOGln("%f", this->cnfg_bioz.bit.phoff * 11.25);
  }

  LOG("BIOZ current generator ");
  switch (this->cnfg_bioz.bit.cgmag) {
    case 0:
      LOGln("is off");
      break;
    case 1:
      LOGln("magnitude is 8uA or low current mode");
      break;
    case 2:
      LOGln("magnitude is 16uA");
      break;
    case 3:
      LOGln("magnitude is 32uA");
      break;
    case 4:
      LOGln("magnitude is 48uA");
      break;
    case 5:
      LOGln("magnitude is 64uA");
      break;
    case 6:
      LOGln("magnitude is 80uA");
      break;
    case 7:
      LOGln("magnitude is 96uA");
      break;
  }

  LOGln("BIOZ current generator monitor(lead off) is %s", this->cnfg_bioz.bit.cgmon ? "enabled" : "disabled");

  LOG("Modulation frequency is ");
  switch (this->cnfg_bioz.bit.fcgen) {
    case 0:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "131072 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "128000 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "128000 Hz" : "127872 Hz");
      break;
    case 1:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "81920 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "80000 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "80000 Hz" : "81920 Hz");
      break;
    case 2:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "40960 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "40000 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "40000 Hz" : "40960 Hz");
      break;
    case 3:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "18204 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "17780 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "17780 Hz" : "18204 Hz");
      break;
    case 4:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "8192 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "8000 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "8000 Hz" : "7992 Hz");
      break;
    case 5:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "4096 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "4000 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "4000 Hz" : "3996 Hz");
      break;
    case 6:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "2048 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "2000 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "2000 Hz" : "1998 Hz");
      break;
    case 7:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "1024 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "1000 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "1000 Hz" : "999 Hz");
      break;
    case 8:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "512 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "500 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "500 Hz" : "500 Hz");
      break;
    case 9:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "256 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "250 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "250 Hz" : "250 Hz");
      break;
    default:
      LOGln(this->cnfg_gen.bit.fmstr == 0 ? "128 Hz" : this->cnfg_gen.bit.fmstr == 1 ? "125 Hz" : this->cnfg_gen.bit.fmstr == 2 ? "125 Hz" : "125 Hz");
      break;
  }

  LOG("Low pass cutoff frequency is ");
  if (this->cnfg_bioz.bit.dlpf == 0) {
    LOGln("bypassed");
  } else if (this->cnfg_bioz.bit.dlpf == 1) {
    LOGln("4.0 Hz");
  } else if (this->cnfg_bioz.bit.dlpf == 2) {
    LOGln("8.0 Hz");
  } else if (this->cnfg_bioz.bit.dlpf == 3) {
    if (this->cnfg_bioz.bit.rate == 0) {
      LOGln("16.0 Hz");
    } else {
      LOGln("4.0 Hz");
    }
  }

  LOG("High pass cutoff frequency is ");
  switch (this->cnfg_bioz.bit.dhpf) {
    case 0:
      LOGln("bypassed");
      break;
    case 1:
      LOGln("0.05 Hz");
      break;
    default:
      LOGln("0.5 Hz");
      break;
  }

  LOG("BIOZ gain is ");
  switch (this->cnfg_bioz.bit.gain) {
    case 0:
      LOGln("10 V/V");
      break;
    case 1:
      LOGln("20 V/V");
      break;
    case 2:
      LOGln("40 V/V");
      break;
    case 3:
      LOGln("80 V/V");
      break;
  }

  LOGln("BIOZ INA is in %s mode", this->cnfg_bioz.bit.ln_bioz ? "low noise" : "low power");
  LOGln("BIOZ external bias resistor is %s", this->cnfg_bioz.bit.ext_rbias ? "enabled" : "disabled");

  LOG("BIOZ analog high pass cutoff frequency is ");
  switch (this->cnfg_bioz.bit.ahpf) {
    case 0:
      LOGln("60.0 Hz");
      break;
    case 1:
      LOGln("150 Hz");
      break;
    case 2:
      LOGln("500 Hz");
      break;
    case 3:
      LOGln("1000 Hz");
      break;
    case 4:
      LOGln("2000 Hz");
      break;
    case 5:
      LOGln("4000 Hz");
      break;
    default:
      LOGln("bypassed");
      break;
  }

  LOG("BIOZ data rate is ");
  if (this->cnfg_bioz.bit.rate == 0) {
    switch (this->cnfg_gen.bit.fmstr) {
      case 0:
        LOGln("64 SPS");
        break;
      case 1:
        LOGln("62.5 SPS");
        break;
      case 2:
        LOGln("50 SPS");
        break;
      default:
        LOGln("49.95 SPS");
        break;
      }
  } else {
    switch (this->cnfg_gen.bit.fmstr) {
      case 0:
        LOGln("32 SPS");
        break;
      case 1:
        LOGln("31.25 SPS");
        break;
      case 2:
        LOGln("25 SPS");
        break;
      default:
        LOGln("24.98 SPS");
        break;
    }
  }
}

void MAX30001::printCNFG_BIOZ_LC() {
  LOGln("BIOZ Low Current Configuration");
  LOGln("------------------------------");

  LOG("BIOZ low current magnitude is ");
  if (this->cnfg_bioz_lc.bit.cmag_lc == 0) {
    LOGln("0nA");
  } else if (this->cnfg_bioz_lc.bit.cmag_lc == 1) {
    LOGln(this->cnfg_bioz_lc.bit.lc2x == 0 ? "55nA" : "110nA");
  } else if (this->cnfg_bioz_lc.bit.cmag_lc == 2) {
    LOGln(this->cnfg_bioz_lc.bit.lc2x == 0 ? "110nA" : "220nA");
  } else if (this->cnfg_bioz_lc.bit.cmag_lc == 3) {
    LOGln(this->cnfg_bioz_lc.bit.lc2x == 0 ? "220nA" : "440nA");
  } else if (this->cnfg_bioz_lc.bit.cmag_lc == 4) {
    LOGln(this->cnfg_bioz_lc.bit.lc2x == 0 ? "330nA" : "660nA");
  } else if (this->cnfg_bioz_lc.bit.cmag_lc == 5) {
    LOGln(this->cnfg_bioz_lc.bit.lc2x == 0 ? "440nA" : "880nA");
  } else if (this->cnfg_bioz_lc.bit.cmag_lc == 6) {
    LOGln(this->cnfg_bioz_lc.bit.lc2x == 0 ? "550nA" : "1100nA");
  }

  LOG("BIOZ common mode feedback resistance for current generator is ");
  switch (this->cnfg_bioz_lc.bit.cmres) {
    case 0:
      LOGln("off");
      break;
    case 1:
      LOGln("320MOhm");
      break;
    case 3:
      LOGln("160MOhm");
      break;
    case 5:
      LOGln("100MOhm");
      break;
    case 7:
      LOGln("80MOhm");
      break;
    case 8:
      LOGln("40MOhm");
      break;
    case 9:
      LOGln("20MOhm");
      break;
    case 10:
      LOGln("12.5MOhm");
      break;
    case 11:
      LOGln("10MOhm");
      break;
    case 12:
      LOGln("7.5MOhm");
      break;
    case 13:
      LOGln("6.5MOhm");
      break;
    case 14:
      LOGln("5.5MOhm");
      break;
    case 15:
      LOGln("5.0MOhm");
      break;
  }

  LOGln("BIOZ high resistance is %s", this->cnfg_bioz_lc.bit.en_bistr ? "enabled" : "disabled");

  LOG("BIOZ high resistance load is ");
  switch (this->cnfg_bioz_lc.bit.bistr) {
    case 0:
      LOGln("27kOhm");
      break;
    case 1:
      LOGln("108kOhm");
      break;
    case 2:
      LOGln("487kOhm");
      break;
    case 3:
      LOGln("1029kOhm");
      break;
  }

  LOGln("BIOZ low current mode is %s", this->cnfg_bioz_lc.bit.lc2x ? "2x" : "1");
  LOGln("BIOZ drive current range is %s", this->cnfg_bioz_lc.bit.hi_lob ? "high [uA]" : "low [nA]");
}

void MAX30001::printCNFG_RTOR1() {
  LOGln("R to R configuration");
  LOGln("--------------------");

  LOGln("R to R peak detection is %s", this->cnfg_rtor1.bit.en_rtor ? "enabled" : "disabled");
  LOGln("R to R threshold scaling factor is %d/16", this->cnfg_rtor1.bit.ptsf + 1);

  LOG("R to R peak averaging weight factor is ...");
  if (this->cnfg_rtor1.bit.pavg == 0) {
    LOGln("2");
  } else if (this->cnfg_rtor1.bit.pavg == 1) {
    LOGln("4");
  } else if (this->cnfg_rtor1.bit.pavg == 2) {
    LOGln("8");
  } else if (this->cnfg_rtor1.bit.pavg == 3) {
    LOGln("16");
  }

  if (this->cnfg_rtor1.bit.gain < 0b1111) {
    LOGln("R to R gain %d", (int)pow(2, this->cnfg_rtor1.bit.gain));
  } else {
    LOGln("R to R gain is auto scale");
  }

  LOG("R to R window length is ...");
  switch (this->cnfg_rtor1.bit.wndw) {
    case 0:
      LOGln("%f", 6 * this->RtoR_resolution);
      break;
    case 1:
      LOGln("%f", 8 * this->RtoR_resolution);
      break;
    case 2:
      LOGln("%f", 10 * this->RtoR_resolution);
      break;
    case 3:
      LOGln("%f", 12 * this->RtoR_resolution);
      break;
    case 4:
      LOGln("%f", 14 * this->RtoR_resolution);
      break;
    case 5:
      LOGln("%f", 16 * this->RtoR_resolution);
      break;
    case 6:
      LOGln("%f", 18 * this->RtoR_resolution);
      break;
    case 7:
      LOGln("%f", 20 * this->RtoR_resolution);
      break;
    case 8:
      LOGln("%f", 22 * this->RtoR_resolution);
      break;
    case 9:
      LOGln("%f", 24 * this->RtoR_resolution);
      break;
    case 10:
      LOGln("%f", 26 * this->RtoR_resolution);
      break;
    case 11:
      LOGln("%f", 28 * this->RtoR_resolution);
      break;
    default:
      LOGln("-1");
      break;
  }
}

void MAX30001::printCNFG_RTOR2(){
  LOGln("R to R Configuration");
  LOGln("--------------------");

  if (this->cnfg_rtor2.bit.rhsf > 0) {
    LOGln("R to R hold off scaling is %d/8", this->cnfg_rtor2.bit.rhsf);
  } else {
    LOGln("R to R hold off interval is determined by minimum hold off only");
  }

  LOG("R to R interval averaging weight factor is ");
  switch (this->cnfg_rtor2.bit.ravg) {
    case 0:
      LOGln("2");
      break;
    case 1:
      LOGln("4");
      break;
    case 2:
      LOGln("8");
      break;
    case 3:
      LOGln("16");
      break;
    default:
      LOGln("Unknown");
      break;
  }

  LOGln("R to R minimum hold off is %.2f ms", this->cnfg_rtor2.bit.hoff * this->RtoR_resolution);
}

/******************************************************************************************************/
// Run Diagnostics
/******************************************************************************************************/

bool MAX30001G::spiCheck(){
  /*
    Perform a self check to verify connections and chip clock integrity
    Returns true if the self-check passes
  */

  max30001_info_t original 
  original.all = readInfo();

  for (int i = 0; i < 5; i++) {
    if ( readInfo().all != original.all) { return false; }
  }
  if ( original.all == 0 ) { return false; }
  return true;
}
