/******************************************************************************************************/
// Driver for the MAX30001G Potential and Impednace Front End from MAXIM/Analog.
//
// Urs Utzinger, July 2024
// ChatGPT, July 2024
/******************************************************************************************************/
#include "max3001g.h"
#include "logger.h"

// SPI Settings
SPISettings SPI_SETTINGS(MAX30001_SPI_SPEED, MSBFIRST, SPI_MODE0); 

// The AFE device
MAX30001G::MAX30001G(int csPin, )
    : _csPin(csPin) { // assing pins
}

void MAX30001G::begin() {
  // Define I/O
  LOGD("AFE: Pins CS, %u", _csPin);
  pinMode(_csPin,  OUTPUT);       // chip select

  LOGD("AFE: CS pin HIGH/Float");
  digitalWrite(_csPin,HIGH);      // CS float

  // SPI
  LOGD("AFE: SPI start");
  SPI.begin();
}

/******************************************************************************************************/
// SPI service functions
/******************************************************************************************************/

void MAX30001G::writeRegister(uint8_t address, uint32_t data) {
  /*
  Service routine to write to register with SPI
  */
  LOGD("SPI write %3u, %3u", address, data);
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(_csPin, LOW);
  delay(2);
  SPI.transfer((address << 1) | 0x00); // write command 
  SPI.transfer((data >> 16) & 0xFF);   // top 8 bits
  SPI.transfer((data >> 8)  & 0xFF);   // middle 8 bits
  SPI.transfer(data         & 0xFF);   // low 8 bits
  delay(2);
  digitalWrite(_csPin, HIGH);
  SPI.endTransaction();
}

uint32_t MAX30001G::readRegister(uint8_t address) {
  /*
  Serrvice routine to read data through SPI
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
  LOGD("SPI read  %3u, %3u", address, data);
  return data;
}

uint8_t MAX30001G::readRegister(uint8_t address) {
  /*
  Service routine to read one byte through SPI
  */
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(_csPin, LOW);
  SPI.transfer(address << 1 | 0x01);
  uint8_t data =  (uint8_t)SPI.transfer(0xff);        // 8 bits
  digitalWrite(_csPin, HIGH);
  SPI.endTransaction();
  LOGD("SPI read  %3u, %3u", address, data);
  return data;
}

/******************************************************************************************************/
// Accessing readings
/******************************************************************************************************/

void MAX30001G::readECG_FIFO(void) {
  /*
  Burst read the ECG FIFO
  Call this routine if ECG FIFO interrupt occured
  */

  max30001_mngr_int_t mngr_int;
  mngr_int.all = readRegister(MNGR_INT);
  uint8_t  num_samples = (mngr_int.e_fit +1)
  
  max3001_ecg_burst_t ecg_burst;

  uint32_t data;
  int32_t sdata;

  // Burst read FIFO register
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(_csPin, LOW);
  SPI.transfer((ECG_FIFO_BURST << 1) | 0x01);
  for (int i = 0; i < num_samples; i++) {
    data  = ((uint32_t)SPI.transfer(0x00) << 16);  // top 8 bits
    data |= ((uint32_t)SPI.transfer(0x00) << 8);  // middle 8 bits
    data |=  (uint32_t)SPI.transfer(0x00);        // low 8 bits
    _bufferECG[i] = data;
  }
  digitalWrite(_csPin, HIGH);
  SPI.endTransaction();

  // Copy to the ECG data buffer
  ecg_counter = 0;
  for (int i = 0; i < num_samples; i ++) {
    ecg_burst.all = _bufferECG[i]; 
    uint8_t ecg_etag = ecg_burst.bit.etag;
    LOGD("ECG Tag: %3u", ecg_etag);
    if (ecg_etag == 0x00) { 
      // Valid sample
      // data is signed 18bit signed at position 6..23 in uint32
      sdata = (int32_t)(ecg_burst.data << (32 - 18)) >> (32 - 18); // need to shift left to get the sign bit
      // Fill the buffer with calibrated value
      // V_ECG (mV) = ADC x VREF / (2^17 x ECG_GAIN)
      ECG_data[ecg_counter++] = float(sdata * V_ref) / float(131072 * ECG_gain); // in [mV]
      LOGD("ECG Sample: %.2f [mV] %d", ECG_data[ecg_counter-1], sdata)
    }
    else if (ecg_etag == 0x07) { 
      // FIFO Overflow
      FIFOReset();
    }
  }

  LOGD("Read %3u samples from ECG FIFO ", ecg_counter);
  ecgSamplesAvailable = ecg_counter;
}

void MAX30001G::readBIOZ_FIFO(void) {
  /*
  Burst read the BIOZ FIFO
  Call this routine if BIOZ FIFO interrupt occured
  */

  max30001_mngr_int_t mngr_int;
  mngr_int.all = readRegister(MNGR_INT);
  uint8_t  num_samples = (mngr_int.e_fit +1)

  max3001_bioz_burst_t bioz_burst;

  uint32_t data;
  int32_t sdata;

  // Burst read FIFO register
  SPI.beginTransaction(SPI_SETTINGS);
  digitalWrite(_csPin, LOW);
  SPI.transfer((BIOZ_FIFO_BURST << 1) | 0x01);
  for (int i = 0; i < num_samples; i++) {
    data  = ((uint32_t)SPI.transfer(0x00) << 16); // top 8 bits
    data |= ((uint32_t)SPI.transfer(0x00) << 8);  // middle 8 bits
    data |=  (uint32_t)SPI.transfer(0x00);        // low 8 bits
    _bufferBIOZ[i] = data;
  }
  digitalWrite(_csPin, HIGH);
  SPI.endTransaction();

  // Copy to the BIOZ data buffer
  bioz_counter = 0;
  for (int i = 0; i < num_bytes; i += 3) {
    uint8_t bioz_etag = ((((uint8_t)_bufferBIOZ[i + 2]) & 0x38) >> 3);
    LOGD("BIOZ Tag: %3u", bioz_etag);
    if (bioz_etag == 0x00) { 
      // Valid sample
      data | = ((uint32_t)_bufferBIOZ[i]   << 16); 
      data | = ((uint32_t)_bufferBIOZ[i+1] << 8); 
      data | = ((uint32_t)_bufferBIOZ[i+2] & 0xC0);
      data = (data << 8);
      // convert the singed data
      sdata = (int32_t)data;
      sdata = (int32_t)sdata >> 8;
      // Fill the buffer
      BIOZ_data[bioz_counter++] = sdata;
    }
    else if (bioz_etag == 0x07) { 
      // FIFO Overflow
      FIFOReset();
    }
  }

  // Copy to the ECG data buffer
  bioz_counter = 0;
  for (int i = 0; i < num_samples; i ++) {
    bioz_burst.all = _bufferBIOZ[i]; 
    uint8_t bioz_etag = bioz_burst.bit.etag;
    LOGD("BIOZ Tag: %3u", bioz_etag);
    if (ecg_etag == 0x00) { 
      // Valid sample
      // data is signed 20bit signed at position 4..23 in uint32
      sdata = (int32_t)(bioz_burst.data << (32 - 20)) >> (32 - 20); // need to shift left to get the sign bit
      // Fill the buffer with calibrated value
      // BioZ (Ω) = ADC x VREF / (2^19 x BIOZ_CGMAG x BIOZ_GAIN)
      // BIOZ_CGMAG is 50 x 10-9 to 96 x 10-6 A, set by CNFG_BIOZ and CNFG_BIOZ_LC 
      // BIOZ_GAIN = 10V/V, 20V/V, 40V/V, or 80V/V. BIOZ_GAIN are set in CNFG_BIOZ (0x18).

      BIOZ_data[bioz_counter++] = float(sdata * V_ref) / float(524288* BIOZ_cgmag * BIOZ_gain); // in [Ohm]
      LOGD("BIOZ Sample: %.2f [Ohm] %d", BIOZ_data[bioz_counter-1], sdata)
    }
    else if (ecg_etag == 0x07) { 
      // FIFO Overflow
      FIFOReset();
    }
  }

  LOGD("Read %3u samples from BIOZ FIFO ", bioz_counter);
  biozSamplesAvailable = bioz_counter;
}

void MAX30001G::readHRandRR(void) {
  /*
  Read the RTOR register to calculate the heart rate and RR interval
  Call this routine if RTOR interrupt occured
  */
  uint32_t rtor = readRegister(RTOR);
  rr_interval = (float)(rtor >> 10) * RtoR_resolution; // in [ms]
  heart_rate = 60000./rr_interval; // in beats per minute 
}

/******************************************************************************************************/
// Interrupt service routine
/******************************************************************************************************/

void MAX30001G::serviceAllInterrupts() {
  /*
  Service routine to read the interrupt status register
  */
  max30001_status_t status; 

  status.all = readRegister(STATUS);
  
  LOGD("Interrupt Status: %s", intToBinaryString(status));

  if (status.bit.eint == 1) {
    // ECG FIFO interrupt
    LOGD("ECG FIFO interrupt");
    ecg_available = true;
  }

  if (status.bit.bint == 1) {
    // BIOZ FIFO interrupt
    LOGD("BIOZ FIFO interrupt");
    bioz_available = true;
  }

  if (status.bit.print == 1) {
    // R to R interrupt
    LOGD("R to R interrupt");
    rtor_available = true;
  }

  if (status.dcloffint == 1) {
    // ECG lead off interrupt
    LOGD("ECG lead off interrupt");
  }

  if (status.bit.eovf == 1) {
    // ECG overflow detection interrupt
    LOGD("ECG overflow detection interrupt");
  }

  if (status.bit.bcgmon == 1) {
    // BIOZ current generator monitor interrupt
    LOGD("BIOZ current generator monitor interrupt");
  }

  if (status.bit.bundr == 1) {
    // BIOZ undervoltage interrupt
    LOGD("BIOZ under voltage interrupt");
  }

  if (status.bit.bover == 1) {
    // BIOZ overcurrent interrupt
    LOGD("BIOZ over voltage interrupt");
  }

}

/******************************************************************************************************/
// Initialize
/******************************************************************************************************/

// ECG Input Stage 
// ---------------
// EMI and ESD protection is built in
//

// DC Lead Off
//  uses programmable current sources to detect lead off conditions. 
//    0,5,10,20,50,100nA are selectable
//  if voltage on lead is above or below threshold, lead off is detected 
//    VMID +/- 300 (default),400,450, 500 mV options

// Lead On check
//  ECGN is pulled high and ECGP is pulled low with pull up/down resistor, comparator checks if both electrodes are attached

// Polarity
//  ECGN and ECGP polarity can be switched internally
//  ECGN and ECGP can be connected to subject

// Lead Bias to VMID
//  internal or external lead bias can be enabled 500, 100, 200 MOhm to meet common mode range requirements

// Calibration Generator
//  +/- 0.25, 0.5 mV uni or bi polar, 1/64 to 256 Hz 

// Amplifier
//  ECG external filter for differential DC rejection is 10microF resoluting in corner frequency of 0.05Hz allowing best ECG quality but has motion artifacts
//  20,40,80,160V/V over all gain selectable

// Filters
// ...

// We have external RC on ECG N and ECG P 47nF and 51k.
// We have external RC on VCM 47nF and 51k
// We might want to exchange external resistor on VCM to 200k
// We have external capacitor on CAPN/P of 10uF, therfore our high pass filter is 0.05Hz


// BIOZ Driver and Impedance Stage
// READ DOCUMENTATION AND WRITE UP HERE
//
//
// We have external RBIAS of 324k and therfore should enable EXT_RBIAS in CNFG_BIOZ

void MAX30001G::beginECG(){
  /* 
  Enables ECG
  */

  max30001_cnfg_gen_t   cnfg_gen;
  max30001_cnfg_cal_t   cnfg_cal;
  max30001_cnfg_emux_t  cnfg_emux;
  max30001_cnfg_ecg_t   cnfg_ecg;
  max30001_en_int_t     en_int;
    
  cnfg_gen.bit.rbiasn =        1; // ECGN connected to VMID 
  cnfg_gen.bit.rbiasp =        1; // ECGP connected to VMID
  cnfg_gen.bit.rbiasv =     0b01; // 100 MOhm
  cnfg_gen.bit.en_rbias =   0b00; // resistive bias disabled
  cnfg_gen.bit.vth =        0b00; // VMID +/-300mV
  cnfg_gen.bit.imag =      0b000; // 0nA, disconnect current sources
  cnfg_gen.bit.ipol =          0; // ECGP pullup, ECGN pull down (not inverted)
  cnfg_gen.bit.en_dcloff =  0b01; // DC lead off detection enabled on ECG N and P
  cnfg_gen.bit.en_bloff =   0b00; // BIOZ lead off detection disabled
  cnfg_gen.bit.en_pace =       0; // pace detection disabled
  cnfg_gen.bit.en_bioz =       0; // BIOZ disabled
  cnfg_gen.bit.en_ecg =        1; // ECG enabled
  cnfg_gen.bit.fmstr =      0b00; // 32768 Hz
  cnfg_gen.bit.en_ulp_lon = 0b00; // ULP lead detection disabled


  cnfg_cal.bit.thigh = 0b00000000000; // thig x CAL_RES
  cnfg_cal.bit.fifty = 0b0;           // Use thigh to select time high for VCALP and VCALN
  cnfg_cal.bit.fcal =  0b000;         // FMSTR/128
  cnfg_cal.bit.vmag =  0b1;           // 0.5mV
  cnfg_cal.bit.vmode = 0b1;           // bipolar
  cnfg_cal.bit.vcal =  0b1;           // calibration sources and modes enabled

  cnfg_emux.bit.caln_sel = 0b11; // ECGN connected to CALN
  cnfg_emux.bit.calp_sel = 0b10; // ECGP connected to CALP
  cnfg_emux.bit.openn =     0b0; // ECGN connected to ECGN AFE Channel
  cnfg_emux.bit.openp =     0b0; // ECGP connected to ECGP AFE Channel
  cnfg_emux.bit.pol =       0b0; // Non inverted polarity

  cnfg_ecg.bit.dlpf = 0b01; // 40z
  cnfg_ecg.bit.dhpf = 0b1;  // 0.5 Hz
  cnfg_ecg.bit.gain = 0b10; // 80 V/V
  cnfg_ecg.bit.rate = 0b10; // 128 sps

  en_int.bit.intb_type = 0b01; // open drain
  en_int.bit.en_pllint =  0b0; // PLL interrupt disabled
  en_int.bit.en_samp   =  0b0; // sample interrupt disabled
  en_int.bit.en_rrint  =  0b0; // R to R interrupt disabled
  en_int.bit.en_lonint =  0b0; // lead off interrupt disabled
  en_int.bit.en_bcgmon =  0b0; // BIOZ current generator monitor interrupt disabled
  en_int.bit.en_bunder =  0b0; // BIOZ undervoltage interrupt disabled
  en_int.bit.en_bover  =  0b0; // BIOZ overcurrent interrupt disabled
  en_int.bit.en_bovf   =  0b0; // BIOZ overvoltage interrupt disabled
  en_int.bit.en_bint   =  0b0; // BIOZ FIFO interrupt 
  en_int.bit.en_dcloff =  0b0; // ECG lead off detection interrupt disabled
  en_int.bit.en_eovf   =  0b0; // ECG overflow detection interrupt disabled
  en_int.bit.en_eint   =  0b1; // ECG FIFO interrupt enabled

  swReset(); // reset the AFE
  delay(100); // wait for reset to complete
  writeRegister(CNFG_GEN, cnfg_gen.all);
  delay(100);
  writeRegister(CNFG_CAL, cnfg_cal.all);  
  delay(100);
  writeRegister(CNFG_EMUX, cnfg_emux.all);
  delay(100);
  writeRegister(CNFG_ECG, cnfg_ecg.all);
  delay(100);
  writeRegister(CNFG_RTOR1, cnfg_rtor1.all);
  delay(100); 
  writeRegister(CNFG_RTOR2, cnfg_rtor2.all);
  delay(100); 
  writeRegister(EN_INT1,    en_int.all);
  delay(100); 
  sync(); // synchronize the AFE
  delay(100); 
} // initialize driver for ECG

void MAX30001G::beginECGcalibration(){
  /* 
  Enables ECG, calibrates it and enables R to R detection
  */

  max30001_cnfg_gen_t   cnfg_gen;
  max30001_cnfg_cal_t   cnfg_cal;
  max30001_cnfg_emux_t  cnfg_emux;
  max30001_cnfg_ecg_t   cnfg_ecg;
  max30001_cnfg_rtor1_t cnfg_rtor1;
  max30001_cnfg_rtor2_t cnfg_rtor2;
  max30001_en_int_t     en_int;
    
  cnfg_gen.bit.rbiasn    =     1; // ECGN connected to VMID 
  cnfg_gen.bit.rbiasp    =     1; // ECGP connected to VMID
  cnfg_gen.bit.rbiasv    =  0b01; // 100 MOhm
  cnfg_gen.bit.en_rbias  =  0b00; // resistive bias disabled
  cnfg_gen.bit.vth       =  0b00; // VMID +/-300mV
  cnfg_gen.bit.imag      = 0b000; // 0nA, disconnect current sources
  cnfg_gen.bit.ipol      =     0; // ECGP pullup, ECGN pull down (not inverted)
  cnfg_gen.bit.en_dcloff =  0b01; // DC lead off detection enabled on ECG N and P
  cnfg_gen.bit.en_bloff  =  0b00; // BIOZ lead off detection disabled
  cnfg_gen.bit.en_pace   =     0; // pace detection disabled
  cnfg_gen.bit.en_bioz   =     0; // BIOZ disabled
  cnfg_gen.bit.en_ecg    =     1; // ECG enabled
  cnfg_gen.bit.fmstr     =  0b00; // 32768 Hz
  cnfg_gen.bit.en_ulp_lon = 0b00; // ULP lead detection disabled


  cnfg_cal.bit.thigh     = 0b00000000000; // thig x CAL_RES
  cnfg_cal.bit.fifty     =    0b0; // Use thigh to select time high for VCALP and VCALN
  cnfg_cal.bit.fcal      =  0b000; // FMSTR/128
  cnfg_cal.bit.vmag      =    0b1; // 0.5mV
  cnfg_cal.bit.vmode     =    0b1; // bipolar
  cnfg_cal.bit.vcal      =    0b1; // calibration sources and modes enabled

  cnfg_emux.bit.caln_sel =   0b11; // ECGN connected to CALN
  cnfg_emux.bit.calp_sel =   0b10; // ECGP connected to CALP
  cnfg_emux.bit.openn    =    0b0; // ECGN connected to ECGN AFE Channel
  cnfg_emux.bit.openp    =    0b0; // ECGP connected to ECGP AFE Channel
  cnfg_emux.bit.pol      =    0b0; // Non inverted polarity

  cnfg_ecg.bit.dlpf      =   0b01; // 40z
  cnfg_ecg.bit.dhpf      =    0b1; // 0.5 Hz
  cnfg_ecg.bit.gain      =   0b10; // 80 V/V
  cnfg_ecg.bit.rate      =   0b10; // 128 sps

  cnfg_rtor1.bit.ptsf    = 0b0110; // (0b0110 + 1)/16
  cnfg_rtor1.bit.pavg    =   0b00; // 2
  cnfg_rtor1.bit.en_rtor =    0b1; // R to R detection enabled
  cnfg_rtor1.bit.gain    = 0b1111; // autoscale
  cnfg_rtor1.bit.wndw    = 0b0011; // 12 x RTOR resolution 

  cnfg_rtor2.bit.rhsf    =  0b100; // 4/8 
  cnfg_rtor2.bit.ravg    =   0b10; // 4 
  cnfg_rtor2.bit.hoff    = 0b100000; // 32*T_RTOR

  en_int.bit.intb_type   =   0b01; // open drain
  en_int.bit.en_pllint   =    0b0; // PLL interrupt disabled
  en_int.bit.en_samp     =    0b0; // sample interrupt disabled
  en_int.bit.en_rrint    =    0b0; // R to R interrupt disabled
  en_int.bit.en_lonint   =    0b0; // lead off interrupt disabled
  en_int.bit.en_bcgmon   =    0b0; // BIOZ current generator monitor interrupt disabled
  en_int.bit.en_bunder   =    0b0; // BIOZ undervoltage interrupt disabled
  en_int.bit.en_bover    =    0b0; // BIOZ overcurrent interrupt disabled
  en_int.bit.en_bovf     =    0b0; // BIOZ overvoltage interrupt disabled
  en_int.bit.en_bint     =    0b0; // BIOZ FIFO interrupt 
  en_int.bit.en_dcloff   =    0b0; // ECG lead off detection interrupt disabled
  en_int.bit.en_eovf     =    0b0; // ECG overflow detection interrupt disabled
  en_int.bit.en_eint     =    0b1; // ECG FIFO interrupt enabled

  swReset(); // reset the AFE
  delay(100); // wait for reset to complete
  writeRegister(CNFG_GEN, cnfg_gen.all);
  delay(100);
  writeRegister(CNFG_CAL, cnfg_cal.all);  
  delay(100);
  writeRegister(CNFG_EMUX, cnfg_emux.all);
  delay(100);
  writeRegister(CNFG_ECG, cnfg_ecg.all);
  delay(100);
  writeRegister(CNFG_RTOR1, cnfg_rtor1.all);
  delay(100);
  writeRegister(CNFG_RTOR2, cnfg_rtor2.all);
  delay(100);
  writeRegister(EN_INT1,    en_int.all);
  delay(100);
  sync(); // synchronize the AFE
  delay(100); 
} // initialize driver for ECG

void MAX30001G::beginRtoR(){
  /* 
  Enables ECG, and enables R to R detection
  */

  max30001_cnfg_gen_t   cnfg_gen;    // general
  // max30001_cnfg_cal_t   cnfg_cal;
  max30001_cnfg_emux_t  cnfg_emux;   // ECG multiplexer
  max30001_cnfg_ecg_t   cnfg_ecg;    // ECG
  max30001_cnfg_rtor1_t cnfg_rtor1;  // R to R detection
  max30001_cnfg_rtor2_t cnfg_rtor2;  // R to R detection
  max30001_en_int_t     en_int;      // Interrupts

  need to set how RTOR interrupt is cleared
    
  cnfg_gen.bit.rbiasn =        0; // ECGN is not connected to VMID 
  cnfg_gen.bit.rbiasp =        0; // ECGP is not connected to VMID
  cnfg_gen.bit.rbiasv =     0b01; // 100 MOhm
  cnfg_gen.bit.en_rbias =   0b00; // resistive bias disabled
  cnfg_gen.bit.vth =        0b00; // VMID +/-300mV
  cnfg_gen.bit.imag =      0b000; // 0nA, disconnect current sources
  cnfg_gen.bit.ipol =          0; // ECGP pullup, ECGN pull down (not inverted)
  cnfg_gen.bit.en_dcloff =  0b00; // DC lead off detection disabled on ECG N and P
  cnfg_gen.bit.en_bloff =   0b00; // BIOZ lead off detection disabled
  cnfg_gen.bit.en_pace =       0; // pace detection disabled
  cnfg_gen.bit.en_bioz =       0; // BIOZ disabled
  cnfg_gen.bit.en_ecg =        1; // ECG enabled
  cnfg_gen.bit.fmstr =      0b00; // 32768 Hz
  cnfg_gen.bit.en_ulp_lon = 0b00; // ULP lead detection disabled

  cnfg_cal.bit.thigh = 0b00000000000; // thig x CAL_RES
  cnfg_cal.bit.fifty =       0b0; // Use thigh to select time high for VCALP and VCALN
  cnfg_cal.bit.fcal =      0b000; // FMSTR/128
  cnfg_cal.bit.vmag =        0b1; // 0.5mV
  cnfg_cal.bit.vmode =       0b1; // bipolar
  cnfg_cal.bit.vcal =        0b0; // calibration sources and modes disabled

  cnfg_emux.bit.caln_sel = 0b11; // ECGN connected to CALN
  cnfg_emux.bit.calp_sel = 0b10; // ECGP connected to CALP
  cnfg_emux.bit.openn =     0b0; // ECGN connected to ECGN AFE Channel
  cnfg_emux.bit.openp =     0b0; // ECGP connected to ECGP AFE Channel
  cnfg_emux.bit.pol =       0b0; // Non inverted polarity

  cnfg_ecg.bit.dlpf =      0b01; // 40z
  cnfg_ecg.bit.dhpf =       0b1; // 0.5 Hz
  cnfg_ecg.bit.gain =      0b00; // 20 V/V
  cnfg_ecg.bit.rate =      0b10; // 128 sps

  cnfg_rtor1.bit.ptsf =  0b0110; // (0b0110 + 1)/16
  cnfg_rtor1.bit.pavg =    0b00; // 2
  cnfg_rtor1.bit.en_rtor =  0b1; // R to R detection enabled
  cnfg_rtor1.bit.gain =  0b1111; // autoscale
  cnfg_rtor1.bit.wndw =  0b0011; // 12 x RTOR resolution 

  cnfg_rtor2.bit.rhsf =    0b100; // 4/8 
  cnfg_rtor2.bit.ravg =     0b10; // 4 
  cnfg_rtor2.bit.hoff = 0b100000; // 32*T_RTOR

  en_int.bit.intb_type =    0b01; // open drain
  en_int.bit.en_pllint =     0b0; // PLL interrupt disabled
  en_int.bit.en_samp =       0b0; // sample interrupt disabled
  en_int.bit.en_rrint =      0b1; // R to R interrupt enabled
  en_int.bit.en_lonint =     0b0; // lead off interrupt disabled
  en_int.bit.en_bcgmon =     0b0; // BIOZ current generator monitor interrupt disabled
  en_int.bit.en_bunder =     0b0; // BIOZ undervoltage interrupt disabled
  en_int.bit.en_bover =      0b0; // BIOZ overcurrent interrupt disabled
  en_int.bit.en_bovf =       0b0; // BIOZ overvoltage interrupt disabled
  en_int.bit.en_bint =       0b0; // BIOZ FIFO interrupt 
  en_int.bit.en_dcloff =     0b0; // ECG lead off detection interrupt disabled
  en_int.bit.en_eovf =       0b0; // ECG overflow detection interrupt disabled
  en_int.bit.en_eint =       0b0; // ECG FIFO interrupt enabled

  swReset(); // reset the AFE
  delay(100); // wait for reset to complete
  writeRegister(CNFG_GEN,   cnfg_gen.all);
  delay(100);
  writeRegister(CNFG_CAL,   cnfg_cal.all);  
  delay(100);
  writeRegister(CNFG_EMUX,  cnfg_emux.all);
  delay(100);
  writeRegister(CNFG_ECG,   cnfg_ecg.all);
  delay(100);
  writeRegister(CNFG_RTOR1, cnfg_rtor1.all);
  delay(100);
  writeRegister(CNFG_RTOR2, cnfg_rtor2.all);
  delay(100);
  writeRegister(EN_INT1,    en_int.all);
  delay(100);
  sync(); // synchronize the AFE
  delay(100); 

} // initialize driver ECG RTOR

void MAX30001G::beginBIOZ(){

  max30001_cnfg_gen_t     cnfg_gen;
  max30001_cnfg_emux_t    cnfg_emux;
  max30001_cnfg_ecg_t     cnfg_ecg;
  max30001_cnfg_bmux_t    cnfg_bmux;
  max30001_cnfg_bioz_t    cnfg_bioz;
  max30001_cnfg_cal_t     cnfg_cal;
  max30001_cnfg_bioz_lc_t cnfg_bioz_lc;
  max30001_en_int_t       en_int;

  // CONFIG GEN Register Settings
  cnfg_gen.bit.en_ulp_lon   =      0; // ULP Lead-ON Disabled
  cnfg_gen.bit.fmstr        =   0b00; // 32768 Hz
  cnfg_gen.bit.en_ecg       =      0; // ECG Enabled
  cnfg_gen.bit.en_bioz      =      1; // BioZ Enabled
  cnfg_gen.bit.en_bloff     =      0; // BioZ digital lead off detection disabled
  cnfg_gen.bit.en_dcloff    =      0; // DC Lead-Off Detection Disabled
  cnfg_gen.bit.en_rbias     =   0b00; // RBias disabled
  cnfg_gen.bit.rbiasv       =   0b01; // RBias = 100 Mohm
  cnfg_gen.bit.rbiasp       =   0b00; // RBias Positive Input not connected
  cnfg_gen.bit.rbiasn       =   0b00; // RBias Negative Input not connected
  
  // BioZ Config Settings, 64SPS, current generator 48microA
  cnfg_bioz.bit.rate        =      0; // 64 SPS
  cnfg_bioz.bit.ahpf        =  0b010; // 500 Hz
  cnfg_bioz.bit.ext_rbias   =      0; // internal bias generator used
  cnfg_bioz.bit.ln_bioz     =      1; // low noise mode
  cnfg_bioz.bit.gain        =   0b01; // 20 V/V
  cnfg_bioz.bit.dhpf        =   0b10; // 0.5Hz
  cnfg_bioz.bit.dlpf        =   0b01; // 4Hz
  cnfg_bioz.bit.fcgen       = 0b0100; // FMSTR/4 approx 8000z
  cnfg_bioz.bit.cgmon       =      0; // current generator monitor disabled 
  cnfg_bioz.bit.cgmag       =  0b100; // 48 microA
  cnfg_bioz.bit.phoff       = 0x0011; // 3*11.25degrees shift 0..168.75 degrees

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

  writeRegister(CNFG_GEN, cnfg_gen.all);
  delay(100);
  wirteRegister(CNFG_CAL, cnfg_cal.all);
  delay(100);
  writeRegister(CNFG_BIOZ, cnfg_bioz.all);
  delay(100);
  writeRegister(CNFG_BIOZ_LC, cnfg_bioz_lc.all);
  delay(100);
  writeRegister(CNFG_BMUX, cnfg_bmux.all);
  delay(100);
  writeRegister(EN_INT1,    en_int.all);
  delay(100);
  synch();
  delay(100);

} // initialize driver for BIOZ

void MAX30001G::beginBIOZcalibrationInternal(){

}

void MAX30001G::beginBIOZcalibrationExternal(){

}

void MAX30001G::beginECGandBIOZ() {
  max30001_cnfg_gen_t     cnfg_gen;
  max30001_cnfg_emux_t    cnfg_emux;
  max30001_cnfg_ecg_t     cnfg_ecg;
  max30001_cnfg_bmux_t    cnfg_bmux;
  max30001_cnfg_bioz_t    cnfg_bioz;
  max30001_cnfg_cal_t     cnfg_cal;
  max30001_cnfg_bioz_lc_t cnfg_bioz_lc;
  max30001_en_int_t       en_int;
  
  // CONFIG GEN Register Settings
  cnfg_gen.bit.en_ulp_lon   =      0; // ULP Lead-ON Disabled
  cnfg_gen.bit.fmstr        =   0b00; // 32768 Hz
  cnfg_gen.bit.en_ecg       =      1; // ECG Enabled
  cnfg_gen.bit.en_bioz      =      1; // BioZ Enabled
  cnfg_gen.bit.en_bloff     =      0; // BioZ digital lead off detection disabled
  cnfg_gen.bit.en_dcloff    =      0; // DC Lead-Off Detection Disabled
  cnfg_gen.bit.en_rbias     =   0b00; // RBias disabled
  cnfg_gen.bit.rbiasv       =   0b01; // RBias = 100 Mohm
  cnfg_gen.bit.rbiasp       =   0b00; // RBias Positive Input not connected
  cnfg_gen.bit.rbiasn       =   0b00; // RBias Negative Input not connected

  // ECG Config Settings
  cnfg_ecg.bit.rate         =   0b10; // Default, 128SPS
  cnfg_ecg.bit.gain         =   0b10; // 80 V/V
  cnfg_ecg.bit.dhpf         =      1; // 0.5Hz
  cnfg_ecg.bit.dlpf         =   0b01; // 40Hz
  
  // ECG MUX Settings
  cnfg_emux.bit.openp       =      0; // ECGP connected to ECGP AFE Channel
  cnfg_emux.bit.openn       =      0; // ECGN connected to ECGN AFE Channel
  cnfg_emux.bit.pol         =      0; // Non inverted polarity
  cnfg_emux.bit.calp_sel    =      0; // No calibration signal on ECG_P
  cnfg_emux.bit.caln_sel    =      0; // No calibration signal on ECG_N

  // BioZ Config Settings, 64SPS, current generator 48microA
  cnfg_bioz.bit.rate        =      0; // 64 SPS
  cnfg_bioz.bit.ahpf        =  0b010; // 500 Hz
  cnfg_bioz.bit.ext_rbias   =      0; // internal bias generator used
  cnfg_bioz.bit.ln_bioz     =      1; // low noise mode
  cnfg_bioz.bit.gain        =   0b01; // 20 V/V
  cnfg_bioz.bit.dhpf        =   0b10; // 0.5Hz
  cnfg_bioz.bit.dlpf        =   0b01; // 4Hz
  cnfg_bioz.bit.fcgen       = 0b0100; // FMSTR/4 approx 8000z
  cnfg_bioz.bit.cgmon       =      0; // current generator monitor disabled 
  cnfg_bioz.bit.cgmag       =  0b100; // 48 microA
  cnfg_bioz.bit.phoff       = 0x0011; // 3*11.25degrees shift 0..168.75 degrees

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

  // Interrupt Bit Settings
  en_int.bit.intb_type      =   0b01; // open drain
  en_int.bit.en_pllint      =    0b0; // PLL interrupt disabled
  en_int.bit.en_samp        =    0b0; // sample interrupt disabled
  en_int.bit.en_rrint       =    0b0; // R to R interrupt enabled
  en_int.bit.en_lonint      =    0b0; // lead off interrupt disabled
  en_int.bit.en_bcgmon      =    0b0; // BIOZ current generator monitor interrupt disabled
  en_int.bit.en_bunder      =    0b0; // BIOZ undervoltage interrupt disabled
  en_int.bit.en_bover       =    0b0; // BIOZ overcurrent interrupt disabled
  en_int.bit.en_bovf        =    0b0; // BIOZ overvoltage interrupt disabled
  en_int.bit.en_bint        =    0b1; // BIOZ FIFO interrupt 
  en_int.bit.en_dcloff      =    0b0; // ECG lead off detection interrupt disabled
  en_int.bit.en_eovf        =    0b0; // ECG overflow detection interrupt disabled
  en_int.bit.en_eint        =    0b1; // ECG FIFO interrupt enabled

  swReset();
  delay(100);
  writeRegister(CNFG_GEN, cnfg_gen.all);
  delay(100);
  wirteRegister(CNFG_CAL, cnfg_cal.all);
  delay(100);
  writeRegister(CNFG_ECG, cnfg_ecg.all);
  delay(100);
  writeRegister(CNFG_EMUX, cnfg_emux.all);
  delay(100);
  writeRegister(CNFG_BIOZ, cnfg_bioz.all);
  delay(100);
  writeRegister(CNFG_BIOZ_LC, cnfg_bioz_lc.all);
  delay(100);
  writeRegister(CNFG_BMUX, cnfg_bmux.all);
  delay(100);
  writeRegister(EN_INT1,    en_int.all);
  delay(100);
  synch();
  delay(100);

} // initialize driver for ECG and BIOZ

/******************************************************************************************************/
// Interrupt Service
/******************************************************************************************************/

//void MAX30001G::enableInterrupt(uint8_t interrupt) {
//  max30001_en_int_t en_int = readRegister(EN_INT1);
//}

/******************************************************************************************************/
// Change Settings
/******************************************************************************************************/

void MAX30001G::setSamplingRate(uint8_t  ECG, uint8_t BIOZ) {
  /*
  Set the ECG  sampling rate for the AFE, 0=low, 1=medium, 2=high
  Set the BIOZ sampling rate for the AFE, 0=low, 1=high
  */

  max30001_cnfg_ecg_t  cnfg_ecg;
  max30001_cnfg_bioz_t cnfg_bioz;
  max30001_cnfg_gen_t  cnfg_gen;

  cnfg_ecg.all  = readRegister(CNFG_ECG);
  cnfg_bioz.all = readRegister(CNFG_BIOZ);
  cnfg_gen.all  = readRegister(CNFG_GEN);

  switch (ECG) {
    case 0:
      cnfg_ecg.bit.rate = 0b10; // low
      break;
    case 1:
      cnfg_ecg.bit.rate = 0b01; // medium
      break;
    case 2:
      cnfg_ecg.bit.rate = 0b00; // high
      break;
    default:
      cnfg_ecg.bit.rate = 0b10; // low
      break;
  }

  writeRegister(CNFG_ECG, cnfg_ecg.all);

  switch (BIOZ) {
    case 0:
      cnfg_bioz.bit.rate = 0b1; // low
      break;
    case 1:
      cnfg_bioz.bit.rate = 0b0; // high
      break;
    default:
      cnfg_bioz.bit.rate = 0b1; // low
      break;
  }

  writeRegister(CNFG_bioz, cnfg_bioz.all);

  switch (cnfg_gen.fmstr) {
    case 0b00: // FMSTR = 32768Hz, TRES = 15.26µs, 512Hz
      fmstr = 32768.;
      tres = 15.26;
      ECG_progression = 512.;
      RtoR_resolution = 7.8125;
      CAL_resolution  = 30.52;
      
      switch (cnfg_ecg.bit.rate) {
        case 0b00:
          ECG_samplingRate = 512.;
          break;
        case 0b01:
          ECG_samplingRate = 256.;
          break;
        case 0b10:
          ECG_samplingRate = 128.;
          break;
        default:
          ECG_samplingRate = 0.;
          break;
      }
      switch (cnfg_bioz.bit.rate) {
        case 0:
          BIOZ_samplingRate = 64.;
          break;
        case 1:
          BIOZ_samplingRate = 32.;
          break;
        default:
          BIOZ_samplingRate = 0.;
          break;
      }
      break;
    case 0b01: // FMSTR = 32000Hz, TRES = 15.63µs, 500Hz
      fmstr = 32000.;
      tres = 15.63;
      ECG_progression = 500.;
      RtoR_resolution = 8.0;
      CAL_resolution  = 31.25;
      switch (cnfg_ecg.bit.rate) {
        case 0b00:
          ECG_samplingRate = 500.;
          break;
        case 0b01:
          ECG_samplingRate = 250.;
          break;
        case 0b10:
          ECG_samplingRate = 125.;
          break;
        default:
          ECG_samplingRate = 0.;
          break;
      }    
      switch (cnfg_bioz.bit.rate) {
        case 0:
          BIOZ_samplingRate = 62.5;
          break;
        case 1:
          BIOZ_samplingRate = 31.25;
          break;
        default:
          BIOZ_samplingRate = 0.;
          break;
      }
      break;
    case 0b10: // FMSTR = 32000Hz, TRES = 15.63µs, 200Hz
      fmstr = 32000.;
      tres = 15.63;
      ECG_progression = 200.;
      RtoR_resolution = 8.0;
      CAL_resolution  = 31.25;
      switch (cnfg_ecg.bit.rate) {
        case 0b10:
          ECG_samplingRate = 200.;
          break;
        default:
          ECG_samplingRate = 0.;
          break;
      }
      switch (cnfg_bioz.bit.rate) {
        case 0:
          BIOZ_samplingRate = 50.;
          break;
        case 1:
          BIOZ_samplingRate = 25.;
          break;
        default:
          BIOZ_samplingRate = 0.;
          break;
      }
      break;

    case 0b11: // FMSTR = 31968.78Hz, TRES = 15.64µs, 199.8049Hz
      fmstr = 31968.78;
      tres = 15.64;
      ECG_progression = 199.8049;
      RtoR_resolution = 8.008;
      CAL_resolution  = 31.28;
      switch (cnfg_ecg.bit.rate) {
        case 0b10:
          ECG_samplingRate = 199.8049;
          break;
        default:
          ECG_samplingRate = 0.;
          break;
      }
      switch (cnfg_bioz.bit.rate) {
        case 0:
          BIOZ_samplingRate = 49.95;
          break;
        case 1:
          BIOZ_samplingRate = 24.98;
          break;
        default:
          BIOZ_samplingRate = 0.;
          break;
      }
      break;
    
    default:
      ECG_samplingRate  = 0.;
      BIOZ_samplingRate = 0.;
      fmstr             = 0.;
      tres              = 0.;
      ECG_progression   = 0.;
      RtoR_resolution   = 0.;
      CAL_resolution    = 0.;
      break;

  }

}
// After chaning the sampling rate, we should also reset the ECG lowpass filter! 
// When re running setECGfilter, the allowable values will be applied.

void MAX30001G::setECGfilter(uint8_t lpf, uint8_t hpf) {
  /*
  Set the ECG lpf for the AFE
  
  0 bypass
  1 low
  2 medium
  3 high

  Set the ECG hpf for the AFE
  
  0 bypass
  1 low 0.5Hz
  
  */

  max30001_cnfg_ecg_t cnfg_ecg;
  max30001_cnfg_gen_t  cnfg_gen;

  cnfg_ecg.all = readRegister(CNFG_ECG);
  cnfg_gen.all  = readRegister(CNFG_GEN);

  if hpf == 0:
    ECG_hpf = 0.;
    cnfg_ecg.dhpf = 0b00; // bypass
  else:
    ECG_hpf = 0.5;
    cnfg_ecg.dhpf = 0b01; // 0.5Hz

  switch (cnfg_gen.fmstr) {
    case 0b00: // FMSTR = 32768Hz, TRES = 15.26µs, 512Hz
      fmstr = 32768.;
      tres = 15.26;
      ECG_progression = 512.;
      switch (cnfg_ecg.bit.rate) {
        case 0b00:
          ECG_samplingRate = 512.;
          switch (lpf) {
            case 0:
              ECG_lpf = 0.;  
              cnfg_ecg.bit.dlpf = 0b00; // bypass
              break;
            case 1:
              ECG_lpf = 40.96;  
              cnfg_ecg.bit.dlpf = 0b01; // low
              break;
            case 2:
              ECG_lpf = 102.4;  
              cnfg_ecg.bit.dlpf = 0b10; // medium
              break;
            case 3:
              ECG_lpf = 153.6;  
              cnfg_ecg.bit.dlpf = 0b11; // high
              break;
            default:
              ECG_lpf = 40.96;  
              cnfg_ecg.bit.dlpf = 0b01; // low
              break;
          }
          break;

        case 0b01:
          ECG_samplingRate = 256.;
          switch (lpf) {
            case 0:
              ECG_lpf = 0.;  
              cnfg_ecg.bit.dlpf = 0b00; // bypass
              break;
            case 1:
              ECG_lpf = 40.96;  
              cnfg_ecg.bit.dlpf = 0b01; // low
              break;
            case 2:
              ECG_lpf = 102.4;  
              cnfg_ecg.bit.dlpf = 0b10; // medium
              break;
            default:
              ECG_lpf = 40.96;  
              cnfg_ecg.bit.dlpf = 0b01; // low
              break;
          }
          break;

        case 0b10:
          ECG_samplingRate = 128.;
          if (lpf == 0) {
            ECG_lpf = 0.;  
            cnfg_ecg.bit.dlpf = 0b00; // bypass
          } else {
            ECG_lpf = 28.35;  
            cnfg_ecg.bit.dlpf = 0b01; // low
            break;
          }
          break;

        default:
          ECG_samplingRate = 0.;
          ECG_lpf = 0.;  
          cnfg_ecg.bit.dlpf = 0b00; // bypass
          break;
      }

    case 0b01: // FMSTR = 32000Hz, TRES = 15.63µs, 500Hz
      fmstr = 32000.;
      tres = 15.63;
      ECG_progression = 500.;
      switch (cnfg_ecg.bit.rate) {
        case 0b00:
          ECG_samplingRate = 500.;
          switch (lpf) {
            case 0:
              ECG_lpf = 0.;  
              cnfg_ecg.bit.dlpf = 0b00; // bypass
              break;
            case 1:
              ECG_lpf = 40.;  
              cnfg_ecg.bit.dlpf = 0b01; // low
              break;
            case 2:
              ECG_lpf = 100.;  
              cnfg_ecg.bit.dlpf = 0b10; // medium
              break;
            case 3:
              ECG_lpf = 150.;  
              cnfg_ecg.bit.dlpf = 0b11; // high
              break;
            default:
              ECG_lpf = 40.;  
              cnfg_ecg.bit.dlpf = 0b01; // low
              break;
          }
          break;

        case 0b01:
          ECG_samplingRate = 250.;
          switch (lpf) {
            case 0:
              ECG_lpf = 0.;  
              cnfg_ecg.bit.dlpf = 0b00; // bypass
              break;
            case 1:
              ECG_lpf = 40.;  
              cnfg_ecg.bit.dlpf = 0b01; // low
              break;
            case 2:
              ECG_lpf = 100.;  
              cnfg_ecg.bit.dlpf = 0b10; // medium
              break;
            default:
              ECG_lpf = 40.;  
              cnfg_ecg.bit.dlpf = 0b01; // low
              break;
          }
          break;

        case 0b10:
          ECG_samplingRate = 125.;
          if (lpf==0) {
            ECG_lpf = 0.;  
            cnfg_ecg.bit.dlpf = 0b00; // bypass
          } else {
            ECG_lpf = 27.68;  
            cnfg_ecg.bit.dlpf = 0b01; // low
          }
          break;

        default:
          ECG_samplingRate = 0.;
          ECG_lpf = 0.;  
          cnfg_ecg.bit.dlpf = 0b00; // bypass
          break;
      }

    case 0b10: // FMSTR = 32000Hz, TRES = 15.63µs, 200Hz
      fmstr = 32000.;
      tres = 15.63;
      ECG_samplingRate = 200.;
      ECG_progression = 200.;
      if (lpf==0) {
          ECG_lpf = 0.;  
          cnfg_ecg.bit.dlpf = 0b00; // bypass
      } else {
          ECG_lpf = 40.;  
          cnfg_ecg.bit.dlpf = 0b01; // low
      }

    case 0b11: // FMSTR = 31968.78Hz, TRES = 15.64µs, 199.8049Hz
      fmstr = 31968.78;
      tres = 15.64;
      ECG_samplingRate = 199.8;
      ECG_progression = 199.8;
      if (lpf==0) {
          ECG_lpf = 0.;  
          cnfg_ecg.bit.dlpf = 0b00; // bypass
      } else {
          ECG_lpf = 39.96;  
          cnfg_ecg.bit.dlpf = 0b01; // low
      }
  }
  writeRegister(CNFG_ECG, cnfg_ecg.all);
}

void MAX30001G::setECGgain(uint8_t gain) {
  /*
  Set the ECG gain for the AFE
  0 20V/V
  1 40V/V
  2 80V/V
  3 160V/V
  */

  max30001_cnfg_ecg_t cnfg_ecg;
  if (gain <=3) {
    cnfg_ecg.all = readRegister(CNFG_ECG);
    cnfg_ecg.bit.gain = gain;
    writeRegister(CNFG_ECG, cnfg_ecg.all);
  }

  switch (gain){
    case 0:
      ECG_gain = 20;
    case 1:
      ECG_gain = 40;
    case 2:
      ECG_gain = 80;
    case 3:
      ECG_gain = 160;
    default:
      ECG_gain = 0;
  }
}

void MAX30001G::setECGLeadDetection() {
}

void MAX30001G::setECGLeadBias() {}

void MAX30001G::setBIOZgain(uint8_t gain) {
  /*
  Set the BIOZ gain for the AFE
  0 20V/V
  1 40V/V
  2 80V/V
  3 160V/V
  */

  max30001_cnfg_bioz_t cnfg_bioz;
  if (gain <=3) {
    cnfg_bioz.all = readRegister(CNFG_BIOZ);
    cnfg_bioz.bit.gain = gain;
    writeRegister(CNFG_BIOZ, cnfg_bioz.all);
  }

  switch (gain){
    case 0:
      BIOZ_gain = 10;
    case 1:
      BIOZ_gain = 20;
    case 2:
      BIOZ_gain = 40;
    case 3:
      BIOZ_gain = 80;
    default:
      BIOZ_gain = 0;
  }
}

void MAX30001G::setBIOZmag(uint32_t current){
  /*
  * Setting current magnitude
  * 50 to 96000 nA
  * This adjust current to be in the range allwed by frequency
  * This programs the BIOZ common mode resistor
  */
  
  max30001_cnfg_bioz_t cnfg_bioz;
  cnfg_bioz.all = readRegister(CNFG_BIOZ);

  max30001_cnfg_bioz_lc_t cnfg_bioz_lc;
  cnfg_bioz_lc.all = readRegister(CNFG_BIOZ_LC);

  // Make sure requested current is achievable with current frequency 
  // ----------------------------------------------------------------
  // BIOZ CURRENT GENERATOR MODULATION FREQUENCY (Hz)
  // BIOZ_FCGEN[3:0]  FMSTR[1:0] = 00 FMSTR[1:0] = 01  FMSTR[1:0] = 10  FMSTR[1:0] = 11   CMAG allowed
  //                  fMSTR= 32,768Hz fMSTR = 32,000Hz fMSTR = 32,000Hz fMSTR = 31,968Hz  
  // 0000             131,072         128,000          128,0001         27,872            all
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

  // high current 
  // --------------------------------------------------------------------
  //   cmag >= 001
  //   and cnfg_bioz_lc.bit.hi_lob == 1 
  //
  //BioZ Current Generator Magnitude: cnfg_bioz.cmag
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
  // BIOZ Low Current Generator Magnitude: cnfg_bioz_lc.bit.cmag_lc
  //         LC2X = 0 LC2X = 1
  // 0000    0        0
  // 0001    55nA     110nA
  // 0010    110nA    220nA
  // 0011    220nA    440nA
  // 0100    330nA    660nA
  // 0101    440nA    880nA
  // 0110    550nA    1100nA

  if (cnfg_bioz.bit.fcgen >= 0b0111) {
    if (current > 8000) {
      current = 8000
      LOGD("Current can not exceed 8000 nA")
    }
  } else if (cnfg_bioz.bit.fcgen == 0b0110) {
    if (current > 16000) {
      current = 16000
      LOGD("Current can not exceed 16000 nA")
    }
  } else if (cnfg_bioz.bit.fcgen == 0b0101) {
    if (current > 32000) {
      current = 32000
      LOGD("Current can not exceed 32000 nA")
    }
  } else if (cnfg_bioz.bit.fcgen == 0b0100) {
    if (current > 96000) {
      current = 96000
      LOGD("Current can not exceed 96000 nA")
    }
  }

  if (current == 0) {
    // Disabled ---------------------------------
    cnfg_bioz.cmag = 0b000;
    BIOZ_cgmag = 0;
  }

  else if (current >= 8000) {
    // High current -----------------------------
    cnfg_bioz_lc.bit.hi_lob = 1;
    if        (current < 12000){
      cnfg_bioz.cmag = 0b001;
      BIOZ_cgmag = 8000;
    } else if (current < 24000) {
      cnfg_bioz.cmag = 0b010;
      BIOZ_cgmag = 16000;
    } else if (current < 40000) {
      cnfg_bioz.cmag = 0b011;
      BIOZ_cgmag = 32000;
    } else if (current < 56000) {
      cnfg_bioz.cmag = 0b100;
      BIOZ_cgmag = 48000;
    } else if (current < 72000) {
      cnfg_bioz.cmag = 0b101;
      BIOZ_cgmag = 64000;
    } else if (current < 88000) {
      cnfg_bioz.cmag = 0b110;
      BIOZ_cgmag = 80000;
    } else if (current == 96000) {
      cnfg_bioz.cmag = 0b111;
      BIOZ_cgmag = 96000;
    }
  
  } else {
    // Low current ------------------------------
    cnfg_bioz.cmag = 0b001;
    cnfg_bioz_lc.bit.hi_lob = 0;
    if          (current < 80){
      cnfg_bioz_lc.bit.lc2x = 0;
      cnfg_bioz_lc.bit.cmag_lc = 0b001;
      BIOZ_cgmag = 55;
    } else {
      cnfg_bioz_lc.bit.lc2x = 1;
      if        (current < 165){
        cnfg_bioz_lc.bit.cmag_lc = 0b001;
        BIOZ_cgmag = 110;
      } else if (current < 330){
        cnfg_bioz_lc.bit.cmag_lc = 0b010;
        BIOZ_cgmag = 220;
      } else if (current < 550){
        cnfg_bioz_lc.bit.cmag_lc = 0b011;
        BIOZ_cgmag = 440;
      } else if (current < 770){
        cnfg_bioz_lc.bit.cmag_lc = 0b100;
        BIOZ_cgmag = 660;
      } else if (current < 990){
        cnfg_bioz_lc.bit.cmag_lc = 0b101;
        BIOZ_cgmag = 880;
      } else {
        cnfg_bioz_lc.bit.cmag_lc = 0b110;
        BIOZ_cgmag = 1100;
      }
    } 
  } // low current
  
  // Adjust CMRES 
  // ------------
  // Should be approximately  5000 / (drive current [µA]) [kΩ]
  // BIOZ_cmag is in [nA]
  uint32_t cmres = 5000 * 1000 / BIOZ_cgmag;
  if        (cmres <= 5250) {
    cnfg_bioz_lc.cmres = 0b1111; 
    BIOZ_cmres = 5000;
  } else if (cmres <= 6000) {
    cnfg_bioz_lc.cmres = 0b1110; 
    BIOZ_cmres = 5500;
  } else if (cmres <= 7000) {
    cnfg_bioz_lc.cmres = 0b1101; 
    BIOZ_cmres = 6500;
  } else if (cmres <= 8750) {
    cnfg_bioz_lc.cmres = 0b1100; 
    BIOZ_cmres = 7500;
  } else if (cmres <= 11250) {
    cnfg_bioz_lc.cmres = 0b1011; 
    BIOZ_cmres = 10000;
  } else if (cmres <= 16250) {
    cnfg_bioz_lc.cmres = 0b1010; 
    BIOZ_cmres = 12500;
  } else if (cmres <= 30000) {
    cnfg_bioz_lc.cmres = 0b1001; 
    BIOZ_cmres = 20000;
  } else if (cmres <= 60000) {
    cnfg_bioz_lc.cmres = 0b1000; 
    BIOZ_cmres = 40000;
  } else if (cmres <= 90000) {
    cnfg_bioz_lc.cmres = 0b0111; 
    BIOZ_cmres = 80000;
  } else if (cmres <= 13000) {
    cnfg_bioz_lc.cmres = 0b0101; 
    BIOZ_cmres = 100000;
  } else if (cmres <= 240000) {
    cnfg_bioz_lc.cmres = 0b0011; 
    BIOZ_cmres = 160000;
  } else if (cmres >240000) {
    cnfg_bioz_lc.cmres = 0b0001; 
    BIOZ_cmres = 320000;
  }

  writeRegister(CNFG_BIOZ, cnfg_bioz.all);
  writeRegister(CNFG_BIOZ_LC, cnfg_bioz_lc.all);

  LOGI("BioZ current set to %5u [nA]", BIOZ_cgmag);
  LOGI("BioZ common mode resistance set to %5u [nA]", BIOZ_cres);

}

void MAX30001G::setBIOZfrequency(uint16_t frequency){
  /*
  * Set the BIOZ frequency modulation
  * After setting the the frequency, the current magnitude will need to be set.
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

  max30001_cnfg_bioz_t cnfg_bioz;
  cnfg_bioz.all = readRegister(CNFG_BIOZ);
  
  max30001_cnfg_gen_t cnfg_gen;
  cnfg_gen.all = readRegister(CNFG_GEN);

  if (frequency >= 104000) {
    cnfg_bioz.bit.fcgen = 0b0000;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 131072;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 128000;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 128000;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 127872;
    }
  } else if (frequency > 60000) {
    cnfg_bioz.bit.fcgen = 0b0001;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 81920;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 80000;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 80000;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 81920;
    }
  } else if (frequency > 29000) {
    cnfg_bioz.bit.fcgen = 0b0010;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 40960;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 40000;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 40000;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 40960;
    }
  } else if (frequency > 13000) {
    cnfg_bioz.bit.fcgen = 0b0011;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 18204;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 17780;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 17780;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 18204;
    }
  } else if (frequency > 6000) {
    cnfg_bioz.bit.fcgen = 0b0100;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 8192;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 8000;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 8000;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 7992;
    }
  } else if (frequency > 3000) {
    cnfg_bioz.bit.fcgen = 0b0101;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 4096;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 4000;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 4000;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 3996;
    }
  } else if (frequency > 1500) {
    cnfg_bioz.bit.fcgen = 0b0110;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 2048;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 2000;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 2000;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 1998;
    }
  } else if (frequency > 750) {
    cnfg_bioz.bit.fcgen = 0b0111;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 1024;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 1000;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 1000;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 999;
    }
  } else if (frequency > 375) {
    cnfg_bioz.bit.fcgen = 0b1000;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 512;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 500;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 500;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 500;
    }
  } else if (frequency > 187) {
    cnfg_bioz.bit.fcgen = 0b1001;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 256;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 250;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 250;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 250;
    }
  } else if (frequency > 0) {
    cnfg_bioz.bit.fcgen = 0b1010;
    if        (cnfg_gen.fmstr == 0b00) {
      BIOZ_frequency = 128;
    } else if (cnfg_gen.fmstr == 0b01) {
      BIOZ_frequency = 125;
    } else if (cnfg_gen.fmstr == 0b10) {
      BIOZ_frequency = 125;
    } else if (cnfg_gen.fmstr == 0b11) {
      BIOZ_frequency = 125;
    }
  }

  writeRegister(CNFG_BIOZ, cnfg_bioz.all);
  LOGI("BIOZ frequency set to %5u [Hz]", BIOZ_frequency);

}
// After setting the the frequency, the current magnitude will need to be set.

void MAX30001G::setBIOZLeadDetection() {
}

void MAX30001G::synch(void){
  writeRegister(SYNCH, 0x000000);  
}

void MAX30001G::swReset(void){
  writeRegister(SW_RST, 0x000000);
  delay(100);
}

void MAX30001G::fifoReset(void){
  writeRegister(FIFO_RST, 0x000000);
}

/******************************************************************************************************/
// Registers
/******************************************************************************************************/
void MAX30001G::readAllRegisters() {
  status.all       = readRegister(STATUS);
  en_int.all       = readRegister(EN_INT);
  en_int2.all      = readRegister(EN_INT2);
  mngr_int.all     = readRegister(MNGR_INT);
  mngr_dyn.all     = readRegister(MNGR_DYN);
  info.all         = readRegister(INFO,);
  cnfg_gen.all     = readRegister(CNFG_GEN);
  cnfg_cal.all     = readRegister(CNFG_CAL);
  cnfg_emux.all    = readRegister(CNFG_EMUX);
  cnfg_ecg.all     = readRegister(CNFG_ECG);
  cnfg_bmux.all    = readRegister(CNFG_BMUX);
  cnfg_bioz.all    = readRegister(CNFG_BIOZ);
  cnfg_bioz_lc.all = readRegister(CNFG_BIOZ_LC);
  cnfg_rtor1.all   = readRegister(CNFG_RTOR1);
  cnfg_rtor2.all   = readRegister(CNFG_RTOR2);
}

void MAX30001G::dumpRegisters() {
  /*
  Read and report all known register
  */
  printStatus();
  printEN_INT(en_int);
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

max30001_info_t MAX30001::readInfo(void)
{
  /*
  Read content of information register
  This should contain the revision number of the chip
  It should also include part ID but its not clear where in the register it is.
  */
    max30001_info_t info;
    info.all = readRegister(INFO, info);
    return info;
}

void MAX30001::printStatus() {
  LOGI("MAX30001 Status Register:");
  LOGI("----------------------------");
 
  LOGI("ECG ------------------------");
  
  if (status.bit.dcdoffint ==1) {
    LOGI("Lead ECG lead are off:");
    if (status.bit.ldoff_nl == 1) {
      LOGI("Lead ECGN below VTHL") ;
    }else if (status.bit.ldoff_nh == 1) {
      LOGI("Lead ECGN above VTHH") ;
    } 
    if (status.bit.ldoff_pl == 1) {
      LOGI("Lead ECGP below VHTL") ;
    } else if (status.bit.ldoff_ph == 1) {
      LOGI("Lead ECGP above VHTL") ;
    } 
  } else {
    LOGI("Lead ECG lead are on.");
  }

  LOGI("ECG FIFO interrupt is %s", status.bit.eint ? "on" : "off");
  LOGI("ECG FIFO overflow is %s", status.bit.eovf ? "on" : "off");
  LOGI("ECG Sample interupt %s", status.bit.eint ? "occured", "not present");
  LOGI("ECG R to R interrupt %s", status.bit.print ? "occured", "not present");
  LOGI("ECG Fast Recovery interrupt is %s", status.bit.fstint ? "on" : "off");

  LOGI("BIOZ -----------------------");

  if (status.bit.bcgmon == 1) {
    LOGI("BIOZ leads are off.");
    if (status.bit.bcgmn == 1) { LOGI("BIOZ N lead off"); }
    if (status.bit.bcgmP == 1) { LOGI("BIOZ p lead off"); }
    if (status.bit.bunder == 1) { LOGI("BIOZ ouput magnitude under BIOZ_LT (4 leads)"); }
    if (status.bit.bover == 1) { LOGI("BIOZ ouput magnitude under BIOZ_HT (4 leads)"); }
  } else {
    LOGI("BIOZ leads are on.");
  }
  LOGI("BIOZ ultra low power leads interrupt is %s", status.bit.lonint ? "on" : "off");
  LOGI("PLL %s", status.bit.pllint ? "PLL" "is working",  "has lost signal")
}

void MAX30001::printEN_INT(max30001_en_int_t en_int) {
  LOGI("MAX30001 Interrupts:");
  LOGI("----------------------------");
  if (en_int.bit.int_type == 0) {
    LOGI("Interrupts are disabled");
  } else if (en_int.bit.int_type == 1) {
    LOGI("Interrupt is CMOS driver");
  } else if (en_int.bit.int_type == 2) {
    LOGI("Interrupt is Open Drain driver");
  } else if (en_int.bit.int_type == 3) {
    LOGI("Interrupt is Open Drain with 125k pullup driver");
  }
  LOGI("PLL interrupt is                                %s", en_int.bit.en_pllint ? "enabled" : "disabled");
  LOGI("Sample synch pulse is                           %s", en_int.bit.en_samp ? "enabled" : "disabled");
  LOGI("R to R detection interrupt is                   %s", en_int.bit.en_rrint ? "enabled" : "disabled");
  LOGI("Ultra low power leads on detection interrupt is %s", en_int.bit.en_samp ? "enabled" : "disabled");
  LOGI("BIOZ current monitor interrupt is               %s", en_int.bit.en_samp ? "enabled" : "disabled");
  LOGI("BIOZ under range interrupt is                   %s", en_int.bit.en_bunder ? "enabled" : "disabled");
  LOGI("BIOZ over range interrupt is                    %s", en_int.bit.en_bover ? "enabled" : "disabled");
  LOGI("BIOZ FIFO overflow interrupt is                 %s", en_int.bit.en_bovf ? "enabled" : "disabled");
  LOGI("BIOZ FIFO interrupt is                          %s", en_int.bit.en_bint ? "enabled" : "disabled");
  LOGI("ECG dc lead off interrupt is                    %s", en_int.bit.en_dcloffint ? "enabled" : "disabled");
  LOGI("ECG fast recovery interrupt is                  %s", en_int.bit.en_fstint ? "enabled" : "disabled");
  LOGI("ECG FIFO overflow interrupt is                  %s", en_int.bit.en_eovf ? "enabled" : "disabled");
  LOGI("ECG FIFO interrupt is                           %s", en_int.bit.en_eint ? "enabled" : "disabled");

}

void MAX30001::printMNGR_INT() {
  LOGI("MAX30001 Interrupt Management:");
  LOGI("----------------------------");

  if (mngr_int.bit.samp_it == 0) {
    LOGI("Sample interrupt on every sample")
  } else if (mngr_int.bit.samp_it == 1) {
    LOGI("Sample interrupt on every 2nd sample")
  } else if (mngr_int.bit.samp_it == 2) {
    LOGI("Sample interrupt on every 4th sample")
  } else if (mngr_int.bit.samp_it == 3) {
    LOGI("Sample interrupt on every 16th sample")
  }

  LOGI("Sample synchronization pulse is cleared ", mngr_int.bit.clr_samp ? "automatically" : "on status read")

  if (mngr_int.bit.clr_rrint == 0) {
    LOGI("RtoR interrupt is cleared on status read");
  } else if (mngr_int.bit.clr_rrint == 1) {
    LOGI("RtoR interrupt is cleared RTOR read");
  } else if (mngr_int.bit.clr_rrint == 2) {
    LOGI("RtoR interrupt is cleared automatically");
  } else {
    LOGI("RtoR interrupt clearance is not defined");
  }

  LOGI("FAST_MODE interrupt %s", mngr_int.bit.clr_fast ? "remains on until FAST_MODE is disengaged" : "is cleared on status read")
  LOGI("BIOZ FIFO interrupt after %u samples", mngr_int.bit.b_fit+1);
  LOGI("ECG FIFO interrupt after %u samples", mngr_int.bit.e_fit+1);
}

void MAX30001::printMNGR_DYN(){
  LOGI("MAX30001 Dynamic Modes:");
  LOGI("----------------------------");
  if (cnfg_gen.bit.en_bloff >= 2) {
    LOGI("BIOZ lead off hi threshold +/-%u * ", 32*mngr_dyn.bit.bloff_hi_it);
    LOGI("BIOZ lead off low threshold +/-%u * ", 32*mngr_dyn.bit.bloff_lo_it);
  } else {
    LOGI("BIOZ lead off thresholds not applicable");
  }

  if (mngr_gyn.bit.fast == 0) {
    LOGI("ECG fast recovery is disabled");
  } else if (mngr_gyn.bit.fast == 1) {
    LOGI("ECG manual fast recovery is enabled");
  } else if (mngr_gyn.bit.fast == 2) {
    LOGI("ECG Automatic Fast recovery is enabled");
    LOGI("ECG Fast recovery threshold is %u", 2048*mngr_dyn.bit.fast_th);
  } else {
    LOGI("ECG Fast recovery is not defined");
  }

}

void MAX30001::printInfo()
{
  /*
  Print the information register
  */
  LOGI("MAX30001 Information Register:");
  LOGI("----------------------------");
  LOGI("Nibble 1:       %u", info.bit.n1);
  LOGI("Nibble 2:       %u", info.bit.n2);
  LOGI("Nibble 3:       %u", info.bit.n3);
  LOGI("Constant 1: (1) %u", info.bit.c1);
  LOGI("2 Bit:          %u", info.bit.n4);
  LOGI("Revision:       %u", info.revision);
  LOGI("Constant 2 (5): %u", info.c2);
}

void MAX30001::printCNFG_GEN()
{
  LOGI("MAX30001 General Config:");
  LOGI("----------------------------");
  LOGI("ECG  is %s", cnfg_gen.bit.en_ecg ? "enabled" : "disabled");
  LOGI("BIOZ is %s", cnfg_gen.bit.en_bioz ? "enabled" : "disabled");
  if (confg_gen.bit.fmstr == 0) {
    LOGI("FMSTR is 32768Hz, global var: %f", fmstr);
    LOGI("TRES is 15.26us, global var: %f", tres);
    LOGI("ECG progression is 512Hz, global var: %f", ECG_progression);    
  } else if (confg_genbit.fmstr == 1) {
    LOGI("FMSTR is 32000Hz, global var: %f", fmstr);
    LOGI("TRES is 15.63us, global var: %f", tres);
    LOGI("ECG progression is 500Hz, global var: %f", ECG_progression);    
  } else if (confg_genbit.fmstr == 2) {
    LOGI("FMSTR is 32000Hz, global var: %f", fmstr);
    LOGI("TRES is 15.63us, global var: %f", tres);
    LOGI("ECG progression is 200Hz, global var: %f", ECG_progression);    
  } else if (confg_genbit.fmstr == 3) {
    LOGI("FMSTR is 31968.78Hz, global var: %f", fmstr);
    LOGI("TRES is 15.64us, global var: %f", tres);
    LOGI("ECG progression is 199.8049Hz, global var: %f", ECG_progression);    
  } else {
    LOGI("FMSTR is undefined");
  }

  LOGI("          --------------------------");
  if (cnfg_gen.bit.en_rbias > 0) {}
    if        ((cnfg_gen.bit.en_rbias == 1) && (cnfg_gen.bit.en_ecg == 1)) {
      LOGI("ECG bias resistor is enabled");
    } else if ((cnfg_gen.bit.en_rbias == 2) && (cnfg_gen.bit.en_bioz == 1)) {
      LOGI("BIOZ bias resistor is enabled");
    } else {
      LOGI("ECG and BIOZ bias resistors are undefined");
    }
    LOGI("N bias resistance is %s", cnfg_gen.bit.rbiasn ? "enabled" : "disabled");
    LOGI("P bias resistance is %s", onfg_gen.bit.rbiasp ? "enabled" : "disabled");
    if (cnfg_gen.bit.rbiasv == 0) {
      LOGI("bias resistor is 50MOhm");
    } else if (cnfg_gen.bit.rbiasv == 1) {
      LOGI("bias resistor is 100MOhm");
    } else if (cnfg_gen.bit.rbiasv == 2) {
      LOGI("bias resistor is 200MOhm");
    } else {
      LOGI("bias resistor is not defined");
    }
  else {
    LOGI("ECG and BIOZ bias resistors are disabled");
  }

  LOGI("          --------------------------");
  if (cnfg_gen.bit.en_dcloff == 1) {
    LOGI("ECG lead off detection is enabled");
    if (cnfg_gen.bit.vth == 0) {
      LOGI("ECG lead off/on threshold is VMID +/-300mV")
    } else if (cnfg_gen.bit.vth == 1) {
      LOGI("ECG lead off/on threshold is VMID +/-400mV")
    } else if (cnfg_gen.bit.vth == 2) {
      LOGI("ECG lead off/on threshold is VMID +/-450mV")
    } else if (cnfg_gen.bit.vth == 3) {
      LOGI("ECG lead off/on threshold is VMID +/-5000mV")
    } else {
      LOGI("ECG lead off/on threshold is not defined")
    }

    if (cnfg_gen.bit.imag == 0) {
      LOGI("ECG lead off/on current source is 0nA")
    } else if (cnfg_gen.bit.imag == 1) {
      LOGI("ECG lead off/on current source is 5nA")
    } else if (cnfg_gen.bit.imag == 2) {
      LOGI("ECG lead off/on current source is 10nA")
    } else if (cnfg_gen.bit.imag == 3) {
      LOGI("ECG lead off/on current source is 20nA")
    } else if (cnfg_gen.bit.imag == 4) {
      LOGI("ECG lead off/on current source is 50nA")
    } else if (cnfg_gen.bit.imag == 5) {
      LOGI("ECG lead off/on current source is 100nA")
    } else {
      LOGI("ECG lead off/on threshold is not defined")
    }

    LOGI("ECG lead off/on polarity is %s", cnfg_gen.bit.dcloff_ipol ? "P is pull down" : "P is pull up");

  } else {
    LOGI("ECG lead off detection is disabled");
  }

  LOGI("          --------------------------");
  if (cnfg_gen.bit.en_ulp_lon == 0) {
    LOGI("ECG Ultra low power leads on detection is disabled");
  } else if (cnfg_gen.bit.en_ulp_lon == 1) {
    LOGI("ECG Ultra low power leads on detection is enabled");
  } else{
    LOGI("ECG Ultra low power leads on detection is not defined");
  }

  LOGI("          --------------------------");
  if (cnfg_gen.bit.en_bloff > 0) {
    LOGI("BIOZ lead off detection is enabled");
    if (cnfg_gen.bit.en_bloff == 1) {
      LOGI("BIOZ lead off/on is under range detection for 4 electrodes operation")
    } else if (cnfg_gen.bit.en_bloff == 2) {
      LOGI("BIOZ lead off/on is over range detection for 2 and 4 electrodes operation")
    } else if (cnfg_gen.bit.en_bloff == 3) {
      LOGI("BIOZ lead off/on is under and over range detection for 4 electrodes operation")
    } else {
      LOGI("BIOZ lead off/on detection is not defined")
    }

  } else {
    LOGI("BIOZ lead off detection is disabled");
  }

}

void MAX30001::printCNFG_CAL(){
  LOGI("MAX30001 Internal Voltage Calibration Source:");
  LOGI("---------------------------------------------");
  if (cnfg_cal.bit.vcal == 1){
    LOGI("Internal voltage calibration source is enabled");
    LOGI ("Voltage calibration source is %s", cnfg_cal.bit.vmode ? "bi polar" : "unipolar");
    LOGI ("Magnitude is %s", cnfg_cal.bit.vmode ? "0.25mV" : "0.5mV");
    if (cnfg_cal.bit.fcal == 0) {
      LOGI ("Frequency is %u Hz", fmstr/128);
    } else if(cnfg_cal.bit.fcal == 1) {
      LOGI ("Frequency is %u Hz", fmstr/512);
    } else if(cnfg_cal.bit.fcal == 2) {
      LOGI ("Frequency is %u Hz", fmstr/2048);
    } else if(cnfg_cal.bit.fcal == 3) {
      LOGI ("Frequency is %u Hz", fmstr/8192);
    } else if(cnfg_cal.bit.fcal == 4) {
      LOGI ("Frequency is %u Hz", fmstr/(2^15));
    } else if(cnfg_cal.bit.fcal == 5) {
      LOGI ("Frequency is %u Hz", fmstr/(2^17));
    } else if(cnfg_cal.bit.fcal == 6) {
      LOGI ("Frequency is %u Hz", fmstr/(2^19));
    } else if(cnfg_cal.bit.fcal == 6) {
      LOGI ("Frequency is %u Hz", fmstr/(2^21));
    } else {
      LOGI ("Frequency is not defined");
    }
  } else { 
    LOGI("Internal voltage calibration source is disabled");
  }
  if (cnfg_cal.bit.fifty == 1) {
    LOGI("50%% duty cycle");
  } else {
    LOGI("Pule length %u [us]", cnfg_cal.bit.thigh*CAL_resolution);
  }
}

void MAX30001::printCNFG_EMUX(){
  LOGI("MAX30001 ECG multiplexer:");
  LOGI("-------------------------");
  
  if (cnfg_emux.bit.caln_sel = 0) {
    LOGI("ECG N is not connected to calibration signal");
  } else if (cnfg_emux.bit.caln_sel == 1) {
    LOGI("ECG N is connected to V_MID");
  } else if (cnfg_emux.bit.caln_sel == 2) {
    LOGI("ECG N is connected to V_CALP");
  } else if (cnfg_emux.bit.caln_sel == 3) {
    LOGI("ECG N is connected to V_CALN");
  }
  if (cnfg_emux.bit.calp_sel = 0) {
    LOGI("ECG P is not connected to calibration signal");
  } else if (cnfg_emux.bit.calp_sel == 1) {
    LOGI("ECG P is connected to V_MID");
  } else if (cnfg_emux.bit.calp_sel == 2) {
    LOGI("ECG P is connected to V_CALP");
  } else if (cnfg_emux.bit.calp_sel == 3) {
    LOGI("ECG P is connected to V_CALN");
  }
  LOGI("ECG N is %s from AFE", cnfg_emux.bit.openn ? "disconnected" : "connected");
  LOGI("ECG P is %s from AFE", cnfg_emux.bit.openp ? "disconnected" : "connected");
  LOGI("ECG input polarity is %s", cnfg_emux.bit.openp ? "inverted" : "not inverted");
}

void MAX30001::printCNFG_ECG(){
  LOGI("MAX30001 ECG settings:");
  LOGI("----------------------");
  
  NEED TO CHECK HERE SEEMS INCOMPLETE

  if (cnfg_ecg.bit.fmstr == 0) {
    if        (cfg_ecg.bit.rate = 0) {
      LOGI("ECG digital low pass filter is bypassed, global: %,2f", ECG_lpf);
    } else if (cfg_ecg.bit.rate = 1) {
      if        (cnfg_ecg.bit.dlpf = 0) {
        LOGI("ECG digital low pass filter is bypassed, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 1) {
        LOGI("ECG digital low pass filter is 40.96Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 2) {
        LOGI("ECG digital low pass filter is 102.4Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 3) {
        LOGI("ECG digital low pass filter is 153.6Hz, global: %,2f", ECG_lpf);
      }
    } else if (cfg_ecg.bit.rate = 2) {
      if        (cnfg_ecg.bit.dlpf = 0) {
        LOGI("ECG digital low pass filter is bypassed, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 1) {
        LOGI("ECG digital low pass filter is 40.96Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 2) {
        LOGI("ECG digital low pass filter is 102.4Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 3) {
        LOGI("ECG digital low pass filter is 40.96Hz, global: %,2f", ECG_lpf);
      }  
    }

  } else if (cnfg_ecg.bit.fmstr == 1) {
    if        (cfg_ecg.bit.rate = 0) {
          LOGI("ECG digital low pass filter is bypassed, global: %,2f", ECG_lpf);
    } else if (cfg_ecg.bit.rate = 1) {
      if        (cnfg_ecg.bit.dlpf = 0) {
        LOGI("ECG digital low pass filter is bypassed, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 1) {
        LOGI("ECG digital low pass filter is 40.00Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 2) {
        LOGI("ECG digital low pass filter is 100.0Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 3) {
        LOGI("ECG digital low pass filter is 150.0Hz, global: %,2f", ECG_lpf);
      }
    } else if (cfg_ecg.bit.rate = 2) {
      if        (cnfg_ecg.bit.dlpf = 0) {
        LOGI("ECG digital low pass filter is bypassed, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 1) {
        LOGI("ECG digital low pass filter is 27.68Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 2) {
        LOGI("ECG digital low pass filter is 27.68Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 3) {
        LOGI("ECG digital low pass filter is 27.68Hz, global: %,2f", ECG_lpf);
      }  
    }

  } else if (cnfg_ecg.bit.fmstr == 2) {
    if (cfg_ecg.bit.rate = 2) {
      if        (cnfg_ecg.bit.dlpf = 0) {
        LOGI("ECG digital low pass filter is bypassed, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 1) {
        LOGI("ECG digital low pass filter is 40.00Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 2) {
        LOGI("ECG digital low pass filter is 40.00Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 3) {
        LOGI("ECG digital low pass filter is 40.00Hz, global: %,2f", ECG_lpf);
      }  
    }  else {
      LOGI("ECG digital low pass filter is not set, global: %,2f", ECG_lpf);
    }

  } else if (cnfg_ecg.bit.fmstr == 3) {
    if       (cfg_ecg.bit.rate = 2) {
      if        (cnfg_ecg.bit.dlpf = 0) {
        LOGI("ECG digital low pass filter is bypassed, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 1) {
        LOGI("ECG digital low pass filter is 39.96Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 2) {
        LOGI("ECG digital low pass filter is 39.96Hz, global: %,2f", ECG_lpf);
      } else if (cnfg_ecg.bit.dlpf = 3) {
        LOGI("ECG digital low pass filter is 39.93Hz, global: %,2f", ECG_lpf);
      }  
    } else {
      LOGI("ECG digital low pass filter is not set, global: %,2f", ECG_lpf);
    }
  }

  LOGI("ECG digital high pass filter is %s Hz, global: %,2f", cnfg_ecg.bit.dhpf ? "bypassed" : "0.5", cnfg ECG_hpf);

  LOGIS("ECG gain is ");
  if (cnfg_ecg.bit.gain = 0) {
    LOGIE("20V/V, global %d", ECG_gain );
  } else if (cnfg_ecg.bit.gain == 1) {
    LOGIE("40V/V, global %d", ECG_gain);
  } else if (cnfg_ecg.bit.gain == 2) {
    LOGIE("80V/V, global %d", ECG_gain);
  } else if (cnfg_ecg.bit.gain == 3) {
    LOGIE("160V/V, global %d", ECG_gain);
  }

  LOGI("ECG data rate is ");
  if (cnfg_ecg.bit.rate == 0) {
    if        (cnfg_gen.fmstr == 0b00) {
      LOGI("512SPS, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b01) {
      LOGI("500SPS, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b10) {
      LOGI("not defined, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b11) {
      LOGI("not defined, global %d", ECG_samplingRate);
    }
  } else if (cnfg_ecg.bit.rate == 1) {
    if        (cnfg_gen.fmstr == 0b00) {
      LOGI("256SPS, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b01) {
      LOGI("250SPS, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b10) {
      LOGI("not defined, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b11) {
      LOGI("not defined, global %d", ECG_samplingRate);
    }
  } else if (cnfg_ecg.bit.rate == 2) {
    if        (cnfg_gen.fmstr == 0b00) {
      LOGI("128SPS, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b01) {
      LOGI("125SPS, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b10) {
      LOGI("200SPS, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b11) {
      LOGI("199.8SPS, global %d", ECG_samplingRate);
    }
  } else if (cnfg_ecg.bit.rate == 3) {
    if        (cnfg_gen.fmstr == 0b00) {
      LOGI("not defined, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b01) {
      LOGI("not defined, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b10) {
      LOGI("not defined, global %d", ECG_samplingRate);
    } else if (cnfg_gen.fmstr == 0b11) {
      LOGI("not defined, global %d", ECG_samplingRate);
    }
  }

}

void MAX30001::printCNFG_BMUX(){

  LOGI("BIOZ MUX Configuration");
  LOGI("----------------------");
  LOGI("Calibration");
  LOGIS("BIOZ Calibration source frequency is ");
  if (confg_bmux.bit.fbist == 0) {
    LOGIE("%5u", fmst/(2^13));
  } else if (confg_bmux.bit.fbist == 1) {
    LOGIE("%5u", fmst/(2^15));
  } else if (confg_bmux.bit.fbist == 2) {
    LOGIE("%5u", fmst/(2^17));
  } else if (confg_bmux.bit.fbist == 3) {
    LOGIE("%5u", fmst/(2^19));
  }

  if (confg_bmux.bit.rnom == 0) {
    LOGIS("BIOZ nominal resistance is 5000 Ohm with modulated resistamce of ");
    if        (confg_bmux.bit.rmod == 0) {
      LOGIE("2960.7 mOhm");
    } else if (confg_bmux.bit.rmod == 1) {
      LOGIE("980.6 mOhm");
    } else if (confg_bmux.bit.rmod == 2) {
      LOGIE("247.5.7 mOhm");
    } else if (confg_bmux.bit.rmod >=4) {
      LOGIE("unmodulated");
    }
  } else if (confg_bmux.bit.rnom == 1) {
    LOGIS("BIOZ nominal resistance is 2500 Ohm with modulated resistance of ");
    if        (confg_bmux.bit.rmod == 0) {
      LOGIE("740.4 mOhm");
    } else if (confg_bmux.bit.rmod == 1) {
      LOGIE("245.2 mOhm");
    } else if (confg_bmux.bit.rmod == 2) {
      LOGIE("61.9 mOhm");
    } else if (confg_bmux.bit.rmod >= 4) {
      LOGIE("unmodulated");
    }
  } else if (confg_bmux.bit.rnom == 2) {
    LOGIS("BIOZ nominal resistance is 1667 Ohm with modulated resistance of ");
    if        (confg_bmux.bit.rmod == 0) {
      LOGIE("329.1 mOhm");
    } else if (confg_bmux.bit.rmod == 1) {
      LOGIE("109.0 mOhm");
    } else if (confg_bmux.bit.rmod == 2) {
      LOGIE("27.5 mOhm");
    } else if (confg_bmux.bit.rmod >= 4) {
      LOGIE("unmodulated");
    }
  } else if (confg_bmux.bit.rnom == 3) {
    LOGIS("BIOZ nominal resistance is 1250 Ohm with modulated resitance of ");
    if        (confg_bmux.bit.rmod == 0) {
      LOGIE("185.1 mOhm");
    } else if (confg_bmux.bit.rmod == 1) {
      LOGIE("61.3 mOhm");
    } else if (confg_bmux.bit.rmod >=4) {
      LOGIE("unmodulated");
    }
  } else if (confg_bmux.bit.rnom == 4) {
    LOGIS("BIOZ nominal resistance is 1000 Ohm with modulated resitance of ");
    if        (confg_bmux.bit.rmod == 0) {
      LOGIE("118.5 mOhm");
    } else if (confg_bmux.bit.rmod == 1) {
      LOGIE("39.2 mOhm");
    } else if (confg_bmux.bit.rmod >=4) {
      LOGIE("unmodulated");
    }
  } else if (confg_bmux.bit.rnom == 5) {
    LOGIS("BIOZ nominal resistance is 833 Ohm with modulated resistance of ");
    if        (confg_bmux.bit.rmod == 0) {
      LOGIE("82.3 mOhm");
    } else if (confg_bmux.bit.rmod == 1) {
      LOGIE("27.2 mOhm");
    } else if (confg_bmux.bit.rmod >=4) {
      LOGIE("unmodulated");
    }
  } else if (confg_bmux.bit.rnom == 6) {
    LOGIS("BIOZ nominal resistance is 714 Ohm with modulated resitance of ");
    if        (confg_bmux.bit.rmod == 0) {
      LOGIE("60.5 mOhm");
    } else if (confg_bmux.bit.rmod == 1) {
      LOGIE("20 mOhm");
    } else if (confg_bmux.bit.rmod >=4) {
      LOGIE("unmodulated");
    }
  } else if (confg_bmux.bit.rnom == 7) {
    LOGIS("BIOZ nominal resistance is 625 Ohm with modulated resistance of ");
    if        (confg_bmux.bit.rmod == 0) {
      LOGI("46.3 mOhm");
    } else if (confg_bmux.bit.rmod == 1) {
      LOGI("15.3 mOhm");
    } else if (confg_bmux.bit.rmod >=4) {
      LOGI("unmodulated");
    }
  }

  LOGI("Built in selftest %s", cnfg_bmux.bit.en_bist ? "enabled, BIP/N should be open." : "disabled");

  LOGIS("BIOZ current generator for ");
  if (cnfg_bmux.bit.cg_mode == 0) {
    LOGIE("unchopped sources (ECG&BIOZ application), or low current range mode");
  } else if (cnfg_bmux.bit.cg_mode == 1) {
    LOGIE("chopped sources, BIOZ without low pass filter");
  } else if (cnfg_bmux.bit.cg_mode == 2) {
    LOGIE("chopped sources, BIOZ with low pass filter");
  } else if (cnfg_bmux.bit.cg_mode == 3) {
    LOGIE("chopped soources, with resistive CM setting, low impedance");
  }

  LOGI("Connections");
  LOGIS("BIOZ N is connected to ")
  if (cnfg_bmux.bit.caln_sel == 0) {
    LOGIE("no calibration");
  } else if (cnfg_bmux.bit.caln_sel == 1) {
    LOGIE("V_MID");
  } else if (cnfg_bmux.bit.caln_sel == 2) {
    LOGIE("V_CALP");
  } else if (cnfg_bmux.bit.caln_sel == 3) {
    LOGIE("V_CALN");
  }
  LOGIS("BIOZ P is connected to ")
  if (cnfg_mux.bit.calp_sel == 0) {
    LOGIE("no calibration");
  } else if (cnfg_mux.bit.calp_sel == 1) {
    LOGIE("V_MID");
  } else if (cnfg_mux.bit.calp_sel == 2) {
    LOGIE("V_CALP");
  } else if (cnfg_mux.bit.calp_sel == 3) {
    LOGIE("V_CALN");
  }

  LOGI("BIOZ N is %s from AFE", cnfg_bmux.bit.openn ? "disconnected" : "connected");
  LOGI("BIOZ P is %s from AFE", cnfg_bmux.bit.openp ? "disconnected" : "connected");
}

void MAX30001::printCNFG_BIOZ(){

  LOGI("BIOZ Configuration");
  LOGI("----------------------");

  LOGIS("BIOZ phaseoffset is ");
  if (cnfg_bioz.bit.fcgen == 0) {
    LOGIE("%f", cnfg_bioz.bit.phoff*45);
  } else if (cnfg_bioz.bit.fcgen == 1) {
    LOGIE("%f", cnfg_bioz.bit.phoff*22.5);
  } else if (cnfg_bioz.bit.fcgn >= 2) {
    LOGIE("%f", cnfg_bioz.bit.phoff*11.25);
  }

  LOGIS("BIOZ current generator ");
  if (cnfg_bioz.bit.cgmag == 0) {
    LOGIE("is off");
  } else if (cnfg_bioz.bit.cgmag == 1) {
    LOGIE("magnitude is 8uA or low current mode");
  } else if (cnfg_bioz.bit.cgmag == 2) {
    LOGIE("magnitude is 16uA");
  } else if (cnfg_bioz.bit.cgmag == 3) {
    LOGIE("magnitude is 32uA");
  } else if (cnfg_bioz.bit.cgmag == 4) {
    LOGIE("magnitude is 48uA");
  } else if (cnfg_bioz.bit.cgmag == 5) {
    LOGIE("magnitude is 64uA");
  } else if (cnfg_bioz.bit.cgmag == 6) {
    LOGIE("magnitude is 80uA");
  } else if (cnfg_bioz.bit.cgmag == 7) {
    LOGIE("magnitude is 96uA");
  }

  LOGI("BIOZ current generator monitor(lead off) is %s". cnfg_bioz.bit.cgmon ? "enabled" : "disabled");

  LOGI("Modulation frequency is ...");
  if (cnfg_bioz.bit.fcgen == 0) {
    if (cnfg_gen.bit.fmstr == 0) {
      LOGI("131072 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("128000 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("128000 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("127872 Hz");
    }
  } else if (cnfg_bioz.bit.fcgen == 1) {
    if (cnfg_gen.bit.fmstr == 0) {
      LOGI("81920 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("80000 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("80000 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("81920 Hz");
    }
  } else if (cnfg_bioz.bit.fcgen == 2) {
    if (cnfg_gen.bit.fmstr == 0) {0
      LOGI("40960 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("40000 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("40000 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("40960 Hz");
    }
  } else if (cnfg_bioz.bit.fcgen == 3) {
    if (cnfg_gen.bit.fmstr == 0) {
      LOGI("18204 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("17780 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("17780 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("18204 Hz");
    }
  } else if (cnfg_bioz.bit.fcgen == 4) {
    if (cnfg_gen.bit.fmstr == 0) {
      LOGI("8192 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("8000 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("8000 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("7992 Hz");
    }
  } else if (cnfg_bioz.bit.fcgen == 5) {
    if (cnfg_gen.bit.fmstr == 0) {
      LOGI("4096 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("4000 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("4000 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("3996 Hz");
    }
  } else if (cnfg_bioz.bit.fcgen == 6) {
    if (cnfg_gen.bit.fmstr == 0) {
      LOGI("2048 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("2000 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("2000 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("1998 Hz");
    }
  } else if (cnfg_bioz.bit.fcgen == 7) {
    if (cnfg_gen.bit.fmstr == 0) {
      LOGI("1024 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("1000 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("1000 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("999 Hz");
    }
  } else if (cnfg_bioz.bit.fcgen == 8) {
    if (cnfg_gen.bit.fmstr == 0) {
      LOGI("512 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("500 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("500 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("500 Hz");
    }
  } else if (cnfg_bioz.bit.fcgen == 9) {
    if (cnfg_gen.bit.fmstr == 0) {
      LOGI("256 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("250 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("250 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("250 Hz");
    }
  } else if (cnfg_bioz.bit.fcgen > 9) {
    if (cnfg_gen.bit.fmstr == 0) {
      LOGI("128 Hz");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("125 Hz");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("125 Hz");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("125 Hz");
    }
  }

  LOGI("Low pass cut off frequency is ");
  if (cnfg_gen.bit.fmstr == 0) {
    if (cnfg_bioz.bit.dlpf == 0) {
      LOGI("bypassed");
    } else if (cnfg_bioz.bit.dlpf == 1) {
      LOGI("4.096 Hz");
    } else if (cnfg_bioz.bit.dlpf == 2) {
      LOGI("8.192 Hz");
    } else if (cnfg_bioz.bit.dlpf == 3) {
      if (cnfg_bioz.bit.rate == 0) {
        LOGI("16.384 Hz");
      } else {
        LOGI("4.096 Hz");
      }
    }
  } else if (cnfg_gen.bit.fmstr == 1) {
    if (cnfg_bioz.bit.dlpf == 0) {
      LOGI("bypassed");
    } else if (cnfg_bioz.bit.dlpf == 1) {
      LOGI("4.0 Hz");
    } else if (cnfg_bioz.bit.dlpf == 2) {
      LOGI("8.0 Hz");
    } else if (cnfg_bioz.bit.dlpf == 3) {
      if (cnfg_bioz.bit.rate == 0) {
        LOGI("16.0 Hz");
      } else {
        LOGI("4.0 Hz");
      }
    }
  } else if (cnfg_gen.bit.fmstr == 2) {
    if (cnfg_bioz.bit.dlpf == 0) {
      LOGI("bypassed");
    } else if (cnfg_bioz.bit.dlpf == 1) {
      LOGI("4.0 Hz");
    } else if (cnfg_bioz.bit.dlpf == 2) {
      LOGI("8.0 Hz");
    } else if (cnfg_bioz.bit.dlpf == 3) {
      if (cnfg_bioz.bit.rate == 0) {
        LOGI("16.0 Hz");
      } else {
        LOGI("4.0 Hz");
      }
    }
  } else if (cnfg_gen.bit.fmstr == 3) {
    if (cnfg_bioz.bit.dlpf == 0) {
      LOGI("bypassed");
    } else if (cnfg_bioz.bit.dlpf == 1) {
      LOGI("3.996 Hz");
    } else if (cnfg_bioz.bit.dlpf == 2) {
      LOGI("7.992 Hz");
    } else if (cnfg_bioz.bit.dlpf == 3) {
      if (cnfg_bioz.bit.rate == 0) {
        LOGI("15.984 Hz");
      } else {
        LOGI("3.996 Hz");
      }
    }
  }

  LOGI("High pass cut off frequency is ...");
  if (cnfg_bio.bit.dhpf ==0) {
    LOGI("bypassed");
  } else if (cnfg_bio.bit.dhpf ==1) {
    LOGI("0.05 Hz");
  } else if (cnfg_bio.bit.dhpf >=2) {
    LOGI("0.5 Hz");
  } 

  LOGI("BIOZ gain is ...");
  if        (cnfg_bioz.bit.gain == 0) {
    LOGI("10V/V");
  } else if (cnfg_bioz.bit.gain == 1) {
    LOGI("20V/V");
  } else if (cnfg_bioz.bit.gain == 2) {
    LOGI("40V/V");
  } else if (cnfg_bioz.bit.gain == 3) {
    LOGI("80V/V");
  } 

  LOGI("BIOZ INA is in %s mode", cnfg_bioz.bit.ln_bioz ? "low noise" : "low power");

  LOGI("BIOZ external bias resistor is %s", cnfg_bioz.bit.ext_rbias ? "enabled" : "disabled");

  LOGI("BIOZ analog high pass cutoff frequency is ...");
  if (cnfg_bioz.bit.ahpf == 0) {
    LOGI("60.0 Hz");
  } else if (cnfg_bioz.bit.ahpf == 1) {
    LOGI("150 Hz");
  } else if (cnfg_bioz.bit.ahpf == 2) {
    LOGI("500 Hz");
  } else if (cnfg_bioz.bit.ahpf == 3) {
    LOGI("1000 Hz");
  } else if (cnfg_bioz.bit.ahpf == 4) {
    LOGI("2000 Hz");
  } else if (cnfg_bioz.bit.ahpf == 5) {
    LOGI("4000 Hz");
  } else if (cnfg_bioz.bit.ahpf > 5) {
    LOGI("bypassed");
  }

  LOGI("BIOZ data rate is ...");
  if (cnfg_bioz.bit.rate == 0){
    if        (cnfg_gen.bit.fmstr == 0) {
      LOGI("64 SPS");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("62.5 SPS");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("50 SPS");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("49.95 SPS");
    }
  } else if (cnfg_bioz.bit.rate == 1){
    if        (cnfg_gen.bit.fmstr == 0) {
      LOGI("32 SPS");
    } else if (cnfg_gen.bit.fmstr == 1) {
      LOGI("31.25 SPS");
    } else if (cnfg_gen.bit.fmstr == 2) {
      LOGI("25 SPS");
    } else if (cnfg_gen.bit.fmstr == 3) {
      LOGI("24.98 SPS");
    }
  }
}

void MAX30001::printCNFG_BIOZ_LC(){
  LOGI("BIOZ Low Current Configuration");
  LOGI("------------------------------");
  LOGIS("BIOZ low current magnitude is ")
  if (cnfg_bioz_lc.bit.cmag_lc == 0) {
    LOGIE("0nA")
  } else if (cnfg_bioz_lc.bit.cmag_lc == 1) {
    if (cnfg_bioz_lc.bit.lc2x == 0) {
      LOGIE("55nA")
    } else {
      LOGIE("110nA")
    }
  } else if (cnfg_bioz_lc.bit.cmag_lc == 2) {
    if (cnfg_bioz_lc.bit.lc2x == 0) {
      LOGIE("110nA")
    } else {
      LOGIE("220nA")
    }
  } else if (cnfg_bioz_lc.bit.cmag_lc == 3) {
    if (cnfg_bioz_lc.bit.lc2x == 0) {
      LOGIE("220nA")
    } else {
      LOGIE("440nA")
    }
  } else if (cnfg_bioz_lc.bit.cmag_lc == 4) {
    if (cnfg_bioz_lc.bit.lc2x == 0) {
      LOGIE("330nA")
    } else {
      LOGIE("660nA")
    }
  } else if (cnfg_bioz_lc.bit.cmag_lc == 5) {
    if (cnfg_bioz_lc.bit.lc2x == 0) {
      LOGIE("440nA")
    } else {
      LOGIE("880nA")
    }
  } else if (cnfg_bioz_lc.bit.cmag_lc == 6) {
    if (cnfg_bioz_lc.bit.lc2x == 0) {
      LOGIE("550nA")
    } else {
      LOGIE("1100nA")
    }
  } 

  LOGIS("BIOZ common mode feedback resestance for current generator is ");
  if        (cnfg_bioz_lc.bit.cmres == 0) {
    LOGIE("off");
  } else if (cnfg_bioz_lc.bit.cmres == 1) {
    LOGIE("320MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 3) {
    LOGIE("160MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 5) {
    LOGIE("100MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 7) {
    LOGIE("80MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 8) {
    LOGIE("40MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 9) {
    LOGIE("20MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 10) {
    LOGIE("12.5MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 11) {
    LOGIE("10MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 12) {
    LOGIE("7.5MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 13) {
    LOGIE("6.5MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 14) {
    LOGIE("5.5MOhm");
  } else if (cnfg_bioz_lc.bit.cmres == 15) {
    LOGIE("5.0MOhm");
  }

  LOGI("BIOZ high resisstance is %s", cnfg_bioz_lc.bit.en_bistr ? "enabled" : "disabled");
  LOGIS("BIOZ high resistance load is ");
  if        (cnfg_bioz.bit.bistr == 0) {
    LOGIE("27kOhm");
  } else if (cnfg_bioz.bit.bistr == 1) {
    LOGIE("108kOhm");
  } else if (cnfg_bioz.bit.bistr == 2) {
    LOGIE("487kOhm");
  } else if (cnfg_bioz.bit.bistr == 3) {
    LOGIE("1029kOhm");
  }

  LOGI("BIOZ low current mode is %s", cnfg_bioz_lc.bit.lc2x ? "2x" : "1");
  LOGI("BIOZ drive current range is %s", cnfg_bioz.bit.hi_lob ? "high [uA]" : "low [nA]");

}

void MAX30001::printCNFG_RTOR1(){
  LOGI("R to R configuration");
  LOGI("--------------------");

  LOGI("R to R peak detection is %s", cnfg_rtor1.bit.en_rtor ? "enabled" : "disabled");
  LOGI("R to R threshold scaling factor is %d/16", cnfg_rtor1.bit.ptsf+1);

  LOGIS("R to R peak averaging weight factor is ...");
  if (cnfg_rtor1.bit.pavg == 0) {
    LOGIE("2")
  } else if (cnfg_rtor1.bit.pavg == 1) {
    LOGIE("4")
  } else if (cnfg_rtor1.bit.pavg == 2) {
    LOGIE("8")
  } else if (cnfg_rtor1.bit.pavg == 3) {
    LOGIE("16")
  }

  if (cnfg_rtor1.bit.gain < 0b1111) {
    LOGI("R to R gain %d", 2^cnfg_rtor1.bit.gain);
  } else {
    LOGI("R to R gain is auto scale");
  }

  LOGIS("R to R window legnth is ...");
  if (cnfg_rtor1.bit.wndw == 0) {
    LOGIE("%f", 6*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 1) {
    LOGIE("%f", 8*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 2) {
    LOGIE("%f", 10*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 3) {
    LOGIE("%f", 12*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 4) {
    LOGIE("%f", 14*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 5) {
    LOGIE("%f", 16*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 6) {
    LOGIE("%f", 18*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 7) {
    LOGIE("%f", 20*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 8) {
    LOGIE("%f", 22*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 9) {
    LOGIE("%f", 24*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 10) {
    LOGIE("%f", 26*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw == 11) {
    LOGIE("%f", 28*RtoR_resolution)
  } else if (cnfg_rtor1.bit.wndw >= 12) {
    LOGIE("-1")
  }
}

void MAX30001::printCNFG_RTOR2(){
  LOGI("R to R configuration");
  LOGI("--------------------");

  if (cnfg_rtor2.bit.rhsf > 0) {
    LOGI("R to R hold off scaling is %d/8", cnfg_rtor2.bit.rhsf);
  } else {
    LOGI("R to R hold off interval is determined by minimum hold off only");
  }

  LOGIS("R to R interval averaging weight factor is ...");
  if (cnfg_rtor2.bit.ravg==0){
    LOGIE("2");
  } else if (cnfg_rtor2.bit.ravg==1){
    LOGIE("4");
  } else if (cnfg_rtor2.bit.ravg==2){
    LOGIE("8");
  } else if (cnfg_rtor2.bit.ravg==3){
    LOGIE("16");
  }

  LOGI("R to R minimum hold off is %f", cnfg_rtor2.bit.hoff*RtoR_resolution);
}

/******************************************************************************************************/
// Run Diagnostics
/******************************************************************************************************/

bool MAX30001G::selfCheck(){
  /*
    Perform a self check to verify connections and chip clock integrity
    -------------------------------------------------------------------

    Returns ture if the self-check passes
    ! The timers need to be enabled and the sensor running when executing the self test !
  */

  max30001_info_t original = readInfo();
  for (int i = 0; i < 5; i++) {
    if ( readInfo().all != original.all) { return false; }
  }
  if ( original.all == 0 ) { return false; }
  return true;
}
