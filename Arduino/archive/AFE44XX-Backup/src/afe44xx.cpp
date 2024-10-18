/******************************************************************************************************/
// Driver for the AFE44XX Pulse Oximeter Analog Front End from Texas Instruments.
//
// This is a comprehensive driver allowing to change most configuration settings.
//
// It is expected that the 
//   - SPI interface is wired correctly.
//   - Chip select line is wired
//   - Dataready signal is wired
//   - Power Down signal is wired
// Other signals are optional.
//
// Functions are privided to:
//  - Set the Photo Diode Amplifier settings
//  - Set the Transmitter LED current and voltage
//  - Set the digital IO
//  - Set the measurement timing
//
// Urs Utzinger, May/June 2024
// ChatGPT, May/June 2024
/******************************************************************************************************/
#include "afe44xx.h"
#include "logger.h"

// SPI Settings
SPISettings SPI_SETTINGS(AFE44XX_SPI_SPEED, MSBFIRST, SPI_MODE0); 

volatile boolean AFE44XX::afe44xx_data_ready = false;

// The AFE device
AFE44XX::AFE44XX(int csPin, int pwdnPin, int drdyPin)
    : _csPin(csPin), _pwdnPin(pwdnPin), _drdyPin(drdyPin) { // assing pins
    }

/******************************************************************************************************/
// Driver startup
/******************************************************************************************************/

void AFE44XX::begin() {
/*
  Configure the necessary input/output digital pins.
  Toggles power to device.
  Initialized SPI and turns on SPI read bit in the AFE.
*/

  // Define I/O
  LOGD("AFE: Pins DRDY(input): %u, CS,PWDN(ouptut): %u, %u", _drdyPin, _csPin, _pwdnPin);

  pinMode(_drdyPin,INPUT_PULLUP); // data ready, activate pull up resistor
  pinMode(_csPin,  OUTPUT);       // chip select
  pinMode(_pwdnPin,OUTPUT);       // power down

  LOGD("AFE: Power Cycle");
  digitalWrite(_pwdnPin, LOW);    // power down
  delay(100);                     // give time
  digitalWrite(_pwdnPin, HIGH);   // power up
  delay(500);                     // give time

  LOGD("AFE: CS pin HIGH/Float");
  digitalWrite(_csPin,HIGH);      // CS float

  // SPI
  LOGD("AFE: SPI start");
  SPI.begin();

  // Reset CONTROL0 register
  LOGD("AFE: Reset Control0 register");
  writeRegister(CONTROL0, CLEAR);

}

/******************************************************************************************************/
// SPI service functions
/******************************************************************************************************/

void AFE44XX::enableSPIread() {
  spiWrite(CONTROL0, SPI_READ_ENABLE);
}

void AFE44XX::disableSPIread() {
  spiWrite(CONTROL0, CLEAR);
}

void AFE44XX::writeRegister(uint8_t address, uint32_t data) {
  spiWrite(address, data);
}

uint32_t AFE44XX::readRegister(uint8_t address) {
  spiWrite(CONTROL0, SPI_READ_ENABLE);
  uint32_t data = spiRead(address);
  spiWrite(CONTROL0, CLEAR);
  return data;
}

void AFE44XX::spiWrite(uint8_t address, uint32_t data) {
/*
  Service routine to write FE register with SPI
*/
    LOGD("SPI write %3u, %3u", address, data);
    SPI.beginTransaction(SPI_SETTINGS);
    digitalWrite(_csPin, LOW);
    SPI.transfer(address); 
    SPI.transfer((data >> 16) & 0xFF);   // top 8 bits
    SPI.transfer((data >> 8)  & 0xFF);   // middle 8 bits
    SPI.transfer(data         & 0xFF);   // low 8 bits
    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
}

uint32_t AFE44XX::spiRead(uint8_t address) {
/*
  Servcice routine to read 24 bit AFE register with SPI
*/
    uint32_t data = 0;
    SPI.beginTransaction(SPI_SETTINGS);
    digitalWrite(_csPin, LOW);
    SPI.transfer(address);
    data |= ((uint32_t)SPI.transfer(0x00) << 16); // top 8 bits
    data |= ((uint32_t)SPI.transfer(0x00) << 8);  // middle 8 bits
    data |=  (uint32_t)SPI.transfer(0x00);        // low 8 bits
    digitalWrite(_csPin, HIGH);
    SPI.endTransaction();
    LOGD("SPI read  %3u, %3u", address, data);
    return data;
}

/******************************************************************************************************/
// Interrupt service routine
/******************************************************************************************************/

void AFE44XX::dataReadyISR() {
  /* short indicator */
  afe44xx_data_ready = true;
}

/******************************************************************************************************/
// Accessing readings
/******************************************************************************************************/

void AFE44XX::getData(int32_t &irRawValue,     int32_t &redRawValue, 
                      int32_t &irAmbientValue, int32_t &redAmbientValue,
                      int32_t &irValue,        int32_t &redValue) {
  afe44xx_data_ready = false;
  enableSPIread();
  irRawValue      = convert22Int(spiRead(LED1VAL));
  redRawValue     = convert22Int(spiRead(LED2VAL));
  irAmbientValue  = convert22Int(spiRead(ALED1VAL));
  redAmbientValue = convert22Int(spiRead(ALED2VAL));
  irValue         = convert22Int(spiRead(LED1ABSVAL));
  redValue        = convert22Int(spiRead(LED2ABSVAL));
  disableSPIread();
}

int32_t AFE44XX::convert22Int(uint32_t number) {
  // AFE ADC produces 22bit twos complement, convert this to singed integer
  number = number << 10;            // Extract the lower 22 bits, keep sign bit and place it to MSB
  return (((int32_t)number) >> 10); // Convert to integer and shift number back to lower 22 bit
}

/******************************************************************************************************/
// Registers
/******************************************************************************************************/
/*

  TIAGAIN 
  -------
  [23:16] must be 0
  [15] separate gain mode
    0 RF, CF, and stage 2 gain settings are the same for both LED2 and LED1
      Values are set in RF_LED2, CF_LED2, STAGE2EN2 and STG2GAIN2 in TIA_AMB_GAIN
    1 RF, CF, and stage 2 gain can be independtly set for LED1 and LED2
      LED1: RF_LED1, CF_LED1, STAGE2EN1 in TIAGAIN
      LED2: RF_LED2, CF_LED2, STAGE2EN1 in TIA_AMB_GAIN
  [14] Stage 2 for LED1, 
    0 disabled by default
    1 with gain set in STG2GAIN1 below
  [13:11] must be 0
  [10:8] STG2GAIN1: 000 linear gain 1, 001/1.5, 010/2, 011/3, 100/4
  [7:3] CF_LED1 00000 5pF, 00001 5+5pF, 00010 15+5pF, 00100 25+5pF, 01000 50+5pF, 10000 150+5pF
  [2:0] RF_LED1 000 500k, 001/250k, 010 100k, 011 50k, 100 25k, 101 10k, 110 1M, 111 None

  TIA_AMB_GAIN
  ------------
  [23:20] must be 0
  [19:16] Ambient DAC cancellation current: 
    00000 0 mu Amp
    1,2,3,4,5,6,7,8,9,
    00101 10 mu Amp
  [15] Filter Corner Selection
    0 500Hz
    1 1000Hz
  [14] Stage 2 for LED2, 
    0 stage 2 bypassed
    1 with gain set in STG2GAIN2 below
  [13:11] must be 0
  [10:8] STG2GAIN2: 000 linear gain 1, 001/1.5, 010/2, 011/3, 100/4
  [7:3] CF_LED2 00000 5pF, 00001 5+5pF, 00010 15+5pF, 00100 25+5pF, 01000 50+5pF, 10000 150+5pF
  [2:0] RF_LED2 000 500k, 001/250k, 010 100k, 011 50k, 100 25k, 101 10k, 110 1M, 111 None

  LEDCNTRL
  --------
  [23:18] must be 0
  [17:16] LED_RANGE 
       [0.75, 0.5 1.0V]
    00 150/100/200mA  (default), 
    01 75/50/100, 
    10 150/100/200, 
    11 Tx Off
  [15:8] LED1/256*Full_Scale_Current from above
  [7:0]  LED2/256*Full_Scale_Current from above

  CONTROL0 
  --------
  [23:4] must be 0
  [3] Software Reset
  [2] Enable Diagnostics, 1 starts diagnostics  and results are shown in Diagnostics Flags Resigster DIAG
  [1] Timer Counter Reset 0 is normal operation, 1 counters are in reset state
  [0] 0 SPI read, 0 is disabled after reset, 1 read is enabled

  CONTROL1
  --------
  [23:12] must be 0
  [11:9] CLKALMPIN, clocks on ALM pins
        PD_ALM                    LED_ALM
    000 Sample LED2 pulse         Sample LED1 pulse
    001 LED2 LED pulse            LED1 LED pulse
    010 Sample LED2 ambient pulse Sample LED1 ambient pulse
    011 LED2 convert              LED1 convert
    100 LED2 ambient convert      LED1 ambient convert
    101 None                      None
    110 None                      None
    111 None                      None
  [8]   Timer Enable, after reset all internal clocks are disabled
  [7:0] Number of samples to be avaeraged -1, setting to 3 restults in 4 averaged samples

  CONTROL2
  --------
  [23:19] must be 0
  [18:17] TX_REF, reference voltage
    00 0.75V default
    01 0.5V
    10 1.0V
    11 0.75V
  [16] 0 no RST Clock on PD_ALM, normal
  [15] 0 internal ADC is active
  [14:12] must be 0 
  [11] 0 LED driver is H-Bridge, 1 is push pull
  [10] digital output mode 0 is normal 1 is tristate (needed for multiple device SPI wen SPI is inactive)
  [9] Oscillator disable, 0 enable oscillator based on external crystal
  [8] 0 fast diagnostic mode 8ms after reset
  [7:3] must be 0
  [2] 1 Tx power down 
  [1] 1 Rs power down
  [0] 1 entire AFE power down

  DIAG (read only)
  ----
  [23:13] must be 0
  [12] Photo Diode, Alarm, 0 no alarm
  [11] LED Alarm
  [10] LED1 open
  [9]  LED2 open
  [8] LED short
  [7] Output P to Ground
  [6] Output N to Ground
  [5] Photo Diode Open
  [4] Photo Diode Short
  [3] In N to Ground
  [2] In P to Ground
  [1] In N to LED
  [0] In P to LED

*/

/******************************************************************************************************/
// Clear
/******************************************************************************************************/

void AFE44XX::clearCNTRL0() {
  /* Reset the Control0 register
     ---------------------------
  */
  LOGD("AFE: clear control0 register");
  writeRegister(CONTROL0, CLEAR); // clear all bits
  LOGD("AFE: cleared control 0");
}

/******************************************************************************************************/
// Reset
/******************************************************************************************************/

void AFE44XX::reset() {
  /* Reset the AFE with a soft reset and enable SPI read
     ---------------------------------------------------
  */
  LOGD("AFE: soft reset");
  writeRegister(CONTROL0, CLEAR); // clear all bits
  // Set the software reset bit in the CONTROL0 register
  writeRegister(CONTROL0, SW_RST);
  // Wait for a short period to allow the reset to complete
  delay(10);  // 10 milliseconds should be sufficient
  LOGD("AFE: soft reset completed");
}

/******************************************************************************************************/
// Run Diagnostics
/******************************************************************************************************/

void AFE44XX::readDiag(uint32_t &status) {
/*
  Starts diagnostics process and reads the status register
  --------------------------------------------------------
*/

  LOGD("AFE: diagnostics in progres");
  // Start diagnostics by writing to the CONTROL0 register
  writeRegister(CONTROL0, DIAGNOSIS); // Set the DIAG_EN bit and SPI read bit to enable diagnostics
  // Wait for diagnostics to complete
  delay(16); // 16 milliseconds delay
  // Read the DIAG register
  status = readRegister(DIAG);
  LOGD("AFE: diagnosis completed");
}

void AFE44XX::printDiag(uint32_t &status)
{
/* 
  Print the diagnostic values in readable form
  --------------------------------------------

  Needs value of diagnostic status register.
*/

  LOGI("DIAG Register: %s", intToBinaryString(status));

  // Check each bit and print the corresponding status
  LOGI("AFE: Diagnostics Status:");
  LOGI("------------------------");
  bool ok = true;
  for (int i = 13; i <= 23; i++) { if (status & (1 << i)) {ok = false;} }
  LOGI("Bits [23:13] must be 0: %s", (ok)                 ? "0 (Ok)" : "1 (Error)");
  LOGI("LED:                    %s", (status & (1 << 11)) ? "Alarm" : "No Alarm");
  LOGI("LED1 Open:              %s", (status & (1 << 10)) ? "Open"  : "OK");
  LOGI("LED2 Open:              %s", (status & (1 <<  9)) ? "Open"  : "OK");
  LOGI("LED Short:              %s", (status & (1 <<  8)) ? "Short" : "OK");
  LOGI("Output P to Ground:     %s", (status & (1 <<  7)) ? "Short" : "OK");
  LOGI("Output N to Ground:     %s", (status & (1 <<  6)) ? "Short" : "OK");
  LOGI("Photo Diode:            %s", (status & (1 << 12)) ? "Alarm" : "No Alarm");
  LOGI("Photo Diode Open:       %s", (status & (1 <<  5)) ? "Open"  : "OK");
  LOGI("Photo Diode Short:      %s", (status & (1 <<  4)) ? "Short" : "OK");
  LOGI("In N to Ground:         %s", (status & (1 <<  3)) ? "Short" : "OK");
  LOGI("In P to Ground:         %s", (status & (1 <<  2)) ? "Short" : "OK");
  LOGI("In N to LED:            %s", (status & (1 <<  1)) ? "Connected" : "OK");
  LOGI("In P to LED:            %s", (status & 1)         ? "Connected" : "OK");
}

bool AFE44XX::selfCheck(){
  /*
    Perform a self check to verify connections and chip clock integrity
    -------------------------------------------------------------------

    Returns ture if the self-check passes
    ! The timers need to be enabled and the sensor running when executing the self test !
  */
  uint32_t original_value = readRegister(CONTROL1);
  for (int i = 0; i < 5; i++) {
    if ( readRegister(CONTROL1) != original_value) { return false; }
  }
  if ( readRegister(CONTROL1) != 0 ) { return false; }
  return true;
}

/******************************************************************************************************/
// Receiver Stage
/******************************************************************************************************/

uint32_t AFE44XX::findCapacitancePattern(int16_t desiredValue) {
  /* 
    Given a desired capacitance, this computes the closest available value for the AFE feedback amp.

    All values in pico Farrad.
  */
  
  struct Capacitor {
    uint8_t binaryPattern;
    uint8_t value;
  };

  const Capacitor capacitors[] = {
    {0b00001,   5},
    {0b00010,  15},
    {0b00100,  25},
    {0b01000,  50},
    {0b10000, 150}
  };
  const int numCapacitors = sizeof(capacitors) / sizeof(capacitors[0]);
  uint32_t closestPattern = 0;
  int closestValue = 0;
  int minDifference = 255; // Initial large difference

  // Loop through all possible combinations of capacitors
  for (uint8_t pattern = 0; pattern < (1 << numCapacitors); ++pattern) {
      int totalValue = 0;

      // Calculate total capacitor value for the current pattern
      for (int i = 0; i < numCapacitors; ++i) {
          if (pattern & (1 << i)) {
              totalValue += capacitors[i].value;
          }
      }
      totalValue += 5; // Add the 5pF minimum capacitance of the system

      // Check if this combination is closer to the desired value
      int difference = abs(totalValue - desiredValue);
      if (difference < minDifference) {
          minDifference = difference;
          closestValue = totalValue;
          closestPattern = pattern;
      }
  }

  LOGD("AFE:   Finding feedback capacitor: Traget: %d, Best: %d, Config %s", desiredValue, closestValue, intToBinaryString(closestPattern));
  return closestPattern;
}

int8_t AFE44XX::setCancellationCurrent(uint8_t current) {
  /* 
     Set the cancelation/background current of the photo diode
     ---------------------------------------------------------

     0,1,2,3,4,5,6,7,8,9 micro Amp 
  */
  LOGD("AFE: --Configureing cackground current");
  if (current > 0b1010) {
      return -1;
  } else {
      uint32_t mask = 0b1111 << 16;              // Create a mask to isolate bits 19..16
      uint32_t tia_settings = readRegister(TIA_AMB_GAIN);
      tia_settings &= ~mask;                     // Clear bits 19..16
      tia_settings |= (uint32_t(current) << 16); // Set bits 19..16 with the current value
      writeRegister(TIA_AMB_GAIN, tia_settings);
      LOGD("AFE:   Background current configured to %u, %s", current, intToBinaryString(tia_settings));
      return 0;
  }
}

int8_t AFE44XX::setSameGain(bool enable) {
  /* Enable Same Gain and Feedback Resistor and Capacitor settings for LED1 and LED2
     --------------------------- ---------------------------------------------------

     Settings for LED1 will be used for LED2
  */
  LOGD("AFE: --Setting same gain and feedback resistor, capacitor for LED1&2");
  uint32_t mask = 0b1 << 15; // Create a mask to isolate bit 15
  uint32_t tia_settings = readRegister(TIAGAIN);
  // Set gain for LED1&2 to be the same or different
  if (enable) {
    tia_settings &= ~mask; // Clear bit 15
    LOGD("AFE:   Same gain and feedback paramters set for LED1&2, %s", intToBinaryString(tia_settings));
  } else {
    tia_settings |= mask;  // Set bit 15
    LOGD("AFE:   Different gain and feedback paramters enabled for LED1&2, %s", intToBinaryString(tia_settings));
}
  writeRegister(TIAGAIN, tia_settings);
  return 0;
}

int8_t AFE44XX::setSecondStageGain(uint8_t gain, bool enabled, uint8_t LED) {
  /* 
    Set the gain of the second stage amplifier
    ------------------------------------------

    0 gain 1, 1 gain 1.5, 2 gain 2, 3 gain 3, 4 gain 4
    enabled: true or false
    0 both LEDs, 1 LED1, 2 LED2
   */
  LOGD("AFE: --Setting second stage gain");
  if (gain > 0b100) {
    return -1;
  } else {
    uint32_t mask = (0b1 << 14) | (0b111 << 8);  // Create a mask for bits 14 (enable) and 10-8 (gain)
    uint32_t tia_settings1 = readRegister(TIAGAIN);
    uint32_t tia_settings2 = readRegister(TIA_AMB_GAIN);
    tia_settings1 &= ~mask;  // Clear bits 14 and 10-8
    tia_settings1 |= (uint32_t(gain) << 8);  // Set bits 10-8 with the gain value
    tia_settings2 &= ~mask;  // Clear bits 14 and 10-8
    tia_settings2 |= (uint32_t(gain) << 8);  // Set bits 10-8 with the gain value
    if (enabled) { tia_settings1 |= (0b1 << 14); } // Set bit 14 if enabled}
    if (enabled) { tia_settings2 |= (0b1 << 14); } // Set bit 14 if enabled}
    switch (LED) {
      case 0: { 
        writeRegister(TIAGAIN, tia_settings1);
        writeRegister(TIA_AMB_GAIN, tia_settings2);
        LOGD("AFE:   Second stage gain set to %u, %s, %s", gain, intToBinaryString(tia_settings1), intToBinaryString(tia_settings2));
        break;
      }
      case 1: { 
        writeRegister(TIAGAIN, tia_settings1);
        LOGD("AFE:   Second stage gain set to %u, %s", gain, intToBinaryString(tia_settings1));
        break;
      }
      case 2: { 
        writeRegister(TIA_AMB_GAIN, tia_settings2);
        LOGD("AFE:   Second stage gain set to %u, %s", gain, intToBinaryString(tia_settings2));
        break;
      }
      default: {
        break;
      }
    }
    return 0;
  }
}

int8_t AFE44XX::setFirstStageFeedbackCapacitor(uint16_t cap, uint8_t LED) {
  /* 
    Set the feedback capcitor of photodiode amplifier
    -------------------------------------------------

    cap 0..275pF 5pF default 
    LED 0 both, 1 LED1, 2 LED2 
  */
  LOGD("AFE: --Setting feeback capacitor");
  if (cap > 275) {
      return -1;
  } else {
    uint32_t config = findCapacitancePattern(cap); // Removed type from function call
    uint32_t mask = 0b11111 << 3;
    uint32_t tia_settings1 = readRegister(TIAGAIN);
    uint32_t tia_settings2 = readRegister(TIA_AMB_GAIN);
    tia_settings1 &= ~mask; // Clear bits 7-3
    tia_settings1 |= config << 3;
    tia_settings2 &= ~mask; // Clear bits 7-3
    tia_settings2 |= config << 3;
    switch (LED) {
      case 0: { 
        writeRegister(TIAGAIN, tia_settings1);
        writeRegister(TIA_AMB_GAIN, tia_settings2);
        LOGD("AFE:   Feeback capacitor on LED %u set to %u, %s, %s", LED, cap, intToBinaryString(tia_settings1), intToBinaryString(tia_settings2));
        break;
      }
      case 1: { 
        writeRegister(TIAGAIN, tia_settings1);
        LOGD("AFE:   Feeback capacitor on LED %u set to %u, %s", LED, cap, intToBinaryString(tia_settings1));
        break;
      }
      case 2: { 
        writeRegister(TIA_AMB_GAIN, tia_settings2);
        LOGD("AFE:   Feeback capacitor on LED %u set to %u, %s", LED, cap, intToBinaryString(tia_settings2));
        break;
      }
      default: {
        return -1;
        break;
      }
    }
    return 0;
  }
}


int8_t AFE44XX::setFirstStageFeedbackResistor(uint16_t resistance, uint8_t LED) {
  /*
    Set feedback resistor of photodiode amplifier
    ---------------------------------------------

    -1..1000 kOhm 500kOhm default 
  */
  LOGD("AFE: --Setting feeback resistor");
  if (resistance > 1000 || resistance < -1)  {
      return -1;
  } else {
    uint32_t mask = 0b111; // No need to shift since we are working with bits 2-0
    uint32_t tia_settings1 = readRegister(TIAGAIN);
    uint32_t tia_settings2 = readRegister(TIA_AMB_GAIN);
    tia_settings1 &= ~mask; // Clear bits 2-0
    tia_settings2 &= ~mask; // Clear bits 2-0
    if (resistance <= 0) {
        tia_settings1 |= 0b111; // 0kOhm
        tia_settings2 |= 0b111; // 0kOhm
    } else if (resistance >= 750) {
        tia_settings1 |= 0b110; // 1000kOhm
        tia_settings2 |= 0b110; // 1000kOhm
    } else if (resistance >= 375) {
        tia_settings1 |= 0b000; // 500kOhm
        tia_settings2 |= 0b000; // 500kOhm
    } else if (resistance >= 175) {
        tia_settings1 |= 0b001; // 250kOhm
        tia_settings2 |= 0b001; // 250kOhm
    } else if (resistance >= 75) {
        tia_settings1 |= 0b010; // 100kOhm
        tia_settings2 |= 0b010; // 100kOhm
    } else if (resistance >= 38) {
        tia_settings1 |= 0b011; // 50kOhm
        tia_settings2 |= 0b011; // 50kOhm
    } else if (resistance >= 18) {
        tia_settings1 |= 0b100; // 25kOhm
        tia_settings2 |= 0b100; // 25kOhm
    } else {
        tia_settings1 |= 0b101; // 10kOhm
        tia_settings2 |= 0b101; // 10kOhm
    }
    switch (LED) {
      case 0: {
        writeRegister(TIAGAIN, tia_settings1);
        writeRegister(TIA_AMB_GAIN, tia_settings2);
        LOGD("AFE:   Feeback resistor on LED %u set to %u, %s, %s", LED, resistance, intToBinaryString(tia_settings1), intToBinaryString(tia_settings2));
        break;
      }
      case 1: {
        writeRegister(TIAGAIN, tia_settings1);
        LOGD("AFE:   Feeback resistor on LED %u set to %u,%s", LED, resistance, intToBinaryString(tia_settings1));
        break;
      }
      case 2: {
        writeRegister(TIA_AMB_GAIN, tia_settings2);
        LOGD("AFE:   Feeback resistor on LED %u set to %u,%s", LED, resistance, intToBinaryString(tia_settings2));
        break;
      }
      default: {
        return -1;
        break;
      }
    }
    return 0;
  }
}

int8_t AFE44XX::setFirstStageCutOffFrequency(uint8_t frequency) {
  /*
    Set the cut-off frequency of the ADC low pass filter
    ----------------------------------------------------

    0 500Hz, 1 1000Hz 
  */
  LOGD("AFE: --Setting low pass filter");
  if (frequency > 1) {
      return -1;
  } else {
    uint32_t mask = 0b1 << 15; // Create a mask to isolate bit 15
    uint32_t tia_amb_settings = readRegister(TIA_AMB_GAIN);
    tia_amb_settings &= ~mask; // Clear bit 15
    if (frequency) {
      tia_amb_settings |= (0b1 << 15); // Set bit 15
    }
    writeRegister(TIA_AMB_GAIN, tia_amb_settings);
    LOGD("AFE:   Low pass filter set to %u, %s", (frequency==0) ? "500Hz" : "1000Hz", intToBinaryString(tia_amb_settings));
    return 0;
  }
} 

/******************************************************************************************************/
// Transmitter Stage
/******************************************************************************************************/

uint8_t AFE44XX::getLEDpower(uint8_t LED) {
  /* 
    Obtain LED power
    ----------------

    LED 0 or 1 
    returns 0..255
  */
  switch (LED) {
    case 0: {
      return (readRegister(LEDCNTRL) & 0x00FF);
    }
    case 1: {
      return ((readRegister(LEDCNTRL) & 0xFF00) >> 8);
    }
    default: {
      LOGE("AFE: LED %u not recognized. Only 0 or 1 are valid.", LED);
      return 0;
    }
  }
}

int8_t AFE44XX::setLEDdriver(uint8_t txRefVoltage, uint8_t range) {
  /*
    Set the reference voltage and max current of the LED driver
    -----------------------------------------------------------

                txRefVoltage
                1   0/2   3 

  current range
              1  50   75  100 milliAmp
            0/2 100  150  200 milliAmp
              3   0    0    0
  power 0..255
  */

  uint32_t led_settings;
  uint32_t mask;

  LOGD("AFE: --Setting LED driver");
  if (txRefVoltage > 3) {
    return -1;
  } else {
    mask = 0b11 << 17; // bit 18-17
    led_settings = readRegister(CONTROL2);
    led_settings &= ~mask; // Clear bits 18-17
    switch (txRefVoltage) {
      case 0: // default 0.75V
        led_settings  |= (0x00 << 17);
        break;
      case 1: // 0.5V
        led_settings  |= (0x01 << 17);
        break;
      case 2: // 0.75V
        led_settings  |= (0x03 << 17);
        break;
      case 3: // 1.0V
        led_settings  |= (0x02 << 17);
        break;
    }
    writeRegister(CONTROL2, led_settings);
    LOGD("AFE:   LED ref voltage set to %u, %s", txRefVoltage, intToBinaryString(led_settings));
  }

  if (range > 3) {
    return -1;
  } else {
    mask = 0b11 << 16; // bit 17-16
    led_settings = readRegister(LEDCNTRL);
    led_settings &= ~mask; // Clear bits 17-16
    switch (range) {
      case 0:
        led_settings |= (0x00 << 16); // 00 for default range
        break;
      case 1:
        led_settings |= (0x01 << 16); // 01 for low range
        break;
      case 2:
        led_settings |= (0x02 << 16); // 10 for high range
        break;
      case 3:
        led_settings |= (0x03 << 16); // 11 for off
        break;
    }
    writeRegister(LEDCNTRL, led_settings);
    LOGD("AFE:   LED range set to %u, %s", range, intToBinaryString(led_settings));
  }
}

void AFE44XX::setLEDpower(uint8_t power, uint8_t LED) {
  /*
    Set the LED power level
    -----------------------

    power
    0 both leds, 1 led1, 2 led2
  */

  uint32_t led_settings;
  uint32_t mask;

  LOGD("AFE: --Setting LED power");

  led_settings = readRegister(LEDCNTRL);

  switch (LED) {
    case 0: {
      mask = 0xFFFF; // bit 15-0
      led_settings &= ~mask; // Clear bits 15-0
      led_settings |= (power << 8); // LED1 power level
      led_settings |= power; // LED2 power level
      break;
    }
    case 1: {
      mask = 0xFF00; // bit 15-8
      led_settings &= ~mask; // Clear bits 15-8
      led_settings |= (power << 8); // LED1 power level
      break;
    }
    case 2: {
      mask = 0x00FF; // bit 7-0
      led_settings &= ~mask; // Clear bits 7-0
      led_settings |= power; // LED2 power level
      break;
    }
    default: {
      LOGE("AFE: LED %u not recognized. Only 0, 1 or 2 are valid.", LED);
      break;}
  }

  writeRegister(LEDCNTRL, led_settings);
  LOGD("AFE:   LED %u power set to %u,%s", LED, power, intToBinaryString(led_settings));

}

/******************************************************************************************************/
// AFE Behaviour
/******************************************************************************************************/

void AFE44XX::bypassADC(bool enable) {
  /* 
    Enable interal ADC
    ------------------
  */
  uint32_t internalADC_settings = readRegister(CONTROL2);
  if (enable) { internalADC_settings |=  (1 << 15); } // Set bit 15
  else        { internalADC_settings &= ~(1 << 15); } // Clear bit 15
  writeRegister(CONTROL2, internalADC_settings);
  LOGD("AFE: --Internal ADC is %s, %s", enable ? "on" : "off", intToBinaryString(internalADC_settings));
}

void AFE44XX::setOscillator(bool enable){
  /* 
    Set Crystal Oscillator or Provide Clock
    ---------------------------------------
    1 enable crystal, 
    0 enable external oscillator
  */
  uint32_t mask = 0b1 << 9;       // Create a mask to isolate bit 9 
  uint32_t oscillator_settings = readRegister(CONTROL2);
  if (enable) { oscillator_settings &= ~mask; } // Clear bit 9
  else        { oscillator_settings |=  mask; } // Set bit 9
  writeRegister(CONTROL2, oscillator_settings);
  LOGD("AFE: --Oscillator is %s, %s", enable ? "on" : "off", intToBinaryString(oscillator_settings));
}

void AFE44XX::setShortDiagnostic(bool enable){
  /*
   Set length of diagnostic time
   -----------------------------

   0 8ms
`  1 16ms
  */
  uint32_t mask = 0b1 << 8;              // Create a mask to isolate bit 8
  uint32_t diagTime_settings = readRegister(CONTROL2);
  if (enable) { diagTime_settings &= ~mask; } // Clear bit 8 
  else        { diagTime_settings |=  mask; }  // Set bit 8
  writeRegister(CONTROL2, diagTime_settings);
  LOGD("AFE: --Diagnostic time length is %s, %s", enable ? "16ms" : "8ms", intToBinaryString(diagTime_settings));
}  

void AFE44XX::setTristate(bool enable) {
  /*
   Enable trisate mode for the digital pins
   ----------------------------------------

  needed for multiple devices on the same SPI bus
   0 normal, 1 tristate
   enable when SPI is inactive
  */
  uint32_t triState_settings = readRegister(CONTROL2);
  uint32_t mask = 0b1 << 10;                  // Create a mask to isolate bit 10
  // Tristate 0 normal, 1 tristate
  if (enable) { triState_settings |= mask; }  // Set bit 10 
  else        { triState_settings &= ~mask; } // Clear bit 10 
  writeRegister(CONTROL2, triState_settings);
  LOGD("AFE: --Tristate is %s, %s", enable ? "on" : "off", intToBinaryString(triState_settings));
}

void AFE44XX::setHbridge(bool enable) {
  /*
    Enable H-Bridge mode for the LED driver
    ---------------------------------------

    push pull otherwise
  */  
  uint32_t hBridge_settings = readRegister(CONTROL2);
  uint32_t mask = 0b1 << 11;                 // Create a mask to isolate bit 11
  // H Bridge 1 is push pull, 0 is H-Bridge
  if (enable) { hBridge_settings &= ~mask; } // Clear bit 11
  else        { hBridge_settings |= mask; }  // Set bit 11 
  writeRegister(CONTROL2, hBridge_settings);
  LOGD("AFE: --H-Bridge is %s, %s", enable ? "on" : "off", intToBinaryString(hBridge_settings));
}

int8_t AFE44XX::setAlarmpins(uint8_t alarmpins) {
  /*
    Setting the alarm pins output behaviour
    ---------------------------------------

        PD_ALM                    LED_ALM
    000 Sample LED2 pulse         Sample LED1 pulse
    001 LED2 LED pulse            LED1 LED pulse
    010 Sample LED2 ambient pulse Sample LED1 ambient pulse
    011 LED2 convert              LED1 convert
    100 LED2 ambient convert      LED1 ambient convert
    101 None                      None
    110 None                      None
    111 None                      None
  */

  if (alarmpins > 0b111) {
    return -1;
  } else {
    // Read control1 register
    uint32_t alarmpins_settings = readRegister(CONTROL1);
    uint32_t mask = 0b111 << 9;              // Create a mask to isolate bits 11 to 9
    // Alarm pins
    alarmpins_settings &= ~mask;             // Clear bit 11 to 9 
    alarmpins_settings |= (alarmpins << 9);
    writeRegister(CONTROL1, alarmpins_settings);
    LOGD("AFE: --Alarm pins set to %u, %s", alarmpins, intToBinaryString(alarmpins_settings));
    return 0;
  }
}

void AFE44XX::setPowerRX(bool enable) // if enable 1 power is on and bit is 0
{
  /*
    Power up the receiver
  */
  LOGD("AFE: --RXPOWER");
  uint32_t control2Value = readRegister(CONTROL2);
  uint32_t mask = 0b1 << 1;               // Create a mask to isolate bit 1
  if (enable) { control2Value &= ~mask; } // Clear bit 1 to power up the Receiver
  else        { control2Value |= mask; }  // Set bit 1 to power down the Receiver
  writeRegister(CONTROL2, control2Value); // Write the modified value back to the CONTROL2 register
  LOGD("AFE: --RXPOWER:   %s, %s", (enable) ? "On": "Off", intToBinaryString(control2Value));
}

void AFE44XX::setPowerAFE(bool enable) // 0 is on
{
  /* 
    Power up the device
  */
  LOGD("AFE: --AFEPOWER");
  uint32_t control2Value = readRegister(CONTROL2);
  uint32_t mask = 0b1;                    // Create a mask to isolate bit 0
  if (enable) { control2Value &= ~mask; } // Clear bit 0 to power up the Receiver
  else        { control2Value |= mask; }  // Set bit 0 to power down the Receiver
  writeRegister(CONTROL2, control2Value); // Write the modified value back to the CONTROL2 register
  LOGD("AFE: --AFEPOWER:  %s, %s", (enable) ? "On ": "Off", intToBinaryString(control2Value));
}

void AFE44XX::setPowerTX(bool enable) // 0 is on
{ 
  /*
    Power up the LED driver
  */
  LOGD("AFE: --TXPOWER");
  uint32_t control2Value = readRegister(CONTROL2);
  uint32_t mask = 0b1 << 2;               // Create a mask to isolate bit 2
  if (enable) { control2Value &= ~mask; } // Clear bit 2 to power up the Receiver
  else        { control2Value |= mask; }  // Set bit 2 to power down the Receiver
  writeRegister(CONTROL2, control2Value); // Write the modified value back to the CONTROL2 register
  LOGD("AFE: --TXPOWER:   %s, %s", (enable) ? "On ": "Off", intToBinaryString(control2Value));
}

bool AFE44XX::isTXPoweredDown() { // 1 is down
  /*
    Is the LED driver powered down?
  */
  LOGD("AFE: --TXPOWER");
  uint32_t control2Value = readRegister(CONTROL2);
  LOGD("AFE:   CONTROL2: %s",intToBinaryString(control2Value));
  return control2Value & (1 << 2);
}

bool AFE44XX::isRXPoweredDown() { // 1 is down
  /*
    Is the photodiode driver powered down?
  */
  LOGD("AFE: --RXPOWER");
  uint32_t control2Value = readRegister(CONTROL2);
  LOGD("AFE:   CONTROL2: %s",intToBinaryString(control2Value));
  return control2Value & (1 << 1);
}

bool AFE44XX::isAFEPoweredDown() { // 1 is down
  /*
    Is the AFE powered down?
  */
  LOGD("AFE: --AFEPOWER");
  uint32_t control2Value = readRegister(CONTROL2);
  LOGD("AFE:   CONTROL2: %s",intToBinaryString(control2Value));
  return control2Value & (1 << 0);
}

/******************************************************************************************************/
// Configure Timer
/******************************************************************************************************/

int8_t AFE44XX::setTimers(float factor) {
/*
  Configure timing sequence of LED power, sampling and conversion
  ---------------------------------------------------------------

  If factor is 1.0, the default values from Table 2 in the data sheet are used. This results in a 
  measurement sequence executing 500 times per second.

  Factor is multiplied with timing values in table 2, while reset pulse length and sampling offset is kept the same.

  PRPCOUNT (length of measurement sequence)
  --------
  [23:16] must be 0
  [15:0] 800 to 64000  

  4Mhz / PRPCount = Measurements per second

  Order of measurement operation is:
    Turn LED on
    Sample Signal 
    Turn LED off
    Reset ADC 
    ADC Conversion

  All measurements:
  1. Ambient LED2 (LED1&2 off), background measurement
  2. LED1
  3. Ambient LED1 (LED1&2 off)
  4. LED2
  5. Repeat

  Timing
  ------------------------------------------------------
  What occurs when?
                 On        Sample    Convert   ADC Reset
    LED2         6000-7999 6050-7998    4-1999    0-   3
    LED1         2000-3999 2050-3998 4004-5999 4000-4003
    Ambient LED2             50-1998 2004-3999 2000-2003
    Ambient LED1           4050-5998 6004-7999 6000-6003
*/

  if (factor < 0.1 || factor > 10.0) {
    return -1;
  }

  LOGD("AFE: --Configuring timing");

  disableSPIread();

  // 1) Reset CONTROL0 register
  spiWrite(CONTROL0,CLEAR);

  // 2) Set TIMINGS
 
  uint32_t _START = 0;
  uint32_t _ONEQUARTER    = uint32_t(factor * 2000.0);
  uint32_t _HALF          = uint32_t(factor * 4000.0);
  uint32_t _THREEQUARTERS = uint32_t(factor * 6000.0);
  uint32_t _END           = uint32_t(factor * 8000.0);

  LOGD("AFE:   PRPCOUNT: %u", (_END - 1));
  spiWrite(PRPCOUNT,     (_END -            1));  // Set timing reload counter, default is 8000 increments

  LOGD("AFE:   Start: %u", _START);
  spiWrite(ADCRSTSTCT0,  (_START             ));  // Start of first ADC conversion reset pulse
  spiWrite(ADCRSTENDCT0, (_START +          3));  // End of first ADC conversion reset pulse
  spiWrite(LED2CONVST,   (_START +          4));  // Start of convert LED2 pulse
  spiWrite(ALED2STC,     (_START +         50));  // Start of sample LED2 ambient pulse

  LOGD("AFE:   1/4: %u", _ONEQUARTER);
  spiWrite(ALED2ENDC,    (_ONEQUARTER -     2));  // End of sample LED2 ambient pulse
  spiWrite(LED2CONVEND,  (_ONEQUARTER -     1));  // End of convert LED2 pulse
  spiWrite(LED1LEDSTC,   (_ONEQUARTER        ));  // Start of LED1 pulse
  spiWrite(ADCRSTSTCT1,  (_ONEQUARTER        ));  // Start of second ADC conversion reset pulse
  spiWrite(ADCRSTENDCT1, (_ONEQUARTER +     3));  // End of second ADC conversion reset pulse
  spiWrite(ALED2CONVST,  (_ONEQUARTER +     4));  // Start of convert LED2 ambient pulse
  spiWrite(LED1STC,      (_ONEQUARTER +    50));  // Start of sample LED1 pulse

  LOGD("AFE:   1/2: %u", _HALF);
  spiWrite(LED1ENDC,     (_HALF -           2));  // End of sample LED1 pulse
  spiWrite(LED1LEDENDC,  (_HALF -           1));  // End of LED1 pulse
  spiWrite(ALED2CONVEND, (_HALF -           1));  // End of convert LED2 ambient pulse
  spiWrite(ADCRSTSTCT2,  (_HALF              ));  // Start of third ADC conversion reset pulse
  spiWrite(ADCRSTENDCT2, (_HALF +           3));  // End of third ADC conversion reset pulse
  spiWrite(LED1CONVST,   (_HALF +           4));  // Start of convert LED1 pulse
  spiWrite(ALED1STC,     (_HALF +          50));  // Start of sample LED1 ambient pulse

  LOGD("AFE:   3/4: %u", _THREEQUARTERS);
  spiWrite(ALED1ENDC,    (_THREEQUARTERS -  2));  // End of sample LED1 ambient pulse
  spiWrite(LED1CONVEND,  (_THREEQUARTERS -  1));  // End of convert LED1 pulse
  spiWrite(LED2LEDSTC,   (_THREEQUARTERS     ));  // Start of LED2 pulse
  spiWrite(ADCRSTSTCT3,  (_THREEQUARTERS     ));  // Start of fourth ADC conversion reset pulse
  spiWrite(ADCRSTENDCT3, (_THREEQUARTERS +  3));  // End of fourth ADC conversion reset pulse
  spiWrite(ALED1CONVST,  (_THREEQUARTERS +  4));  // Start of convert LED1 ambient pulse
  spiWrite(LED2STC,      (_THREEQUARTERS + 50));  // Start of sample LED2 pulse

  LOGD("AFE:   End: %u", _END);
  spiWrite(LED2ENDC,     (_END -            2));  // End of sample LED2 pulse
  spiWrite(LED2LEDENDC,  (_END -            1));  // End of LED2 pulse
  spiWrite(ALED1CONVEND, (_END -            1));  // End of convert LED1 ambient pulse

  LOGD("AFE: --Timing configured");
  return 0;
}

void AFE44XX::readTimers() {
/*
  Debug Timers
*/
  enableSPIread();
  LOGI("AFE:   ADCRSTSTCT0:  %4u", spiRead(ADCRSTSTCT0));  // t21 Start of first ADC conversion reset pulse
  LOGI("AFE:   ADCRSTENDCT0: %4u", spiRead(ADCRSTENDCT0)); // t22 End of first ADC conversion reset pulse
  LOGI("AFE:   LED2CONVST:   %4u", spiRead(LED2CONVST));   // t13 Start of convert LED2 pulse
  LOGI("AFE:   ALED2STC:     %4u", spiRead(ALED2STC));     // t5 Start of sample LED2 ambient pulse
  LOGI("AFE:   ALED2ENDC:    %4u", spiRead(ALED2ENDC));    // t6 End of sample LED2 ambient pulse
  LOGI("AFE:   LED2CONVEND:  %4u", spiRead(LED2CONVEND));  // t14 End of convert LED2 pulse
  LOGI("AFE:   LED1LEDSTC:   %4u", spiRead(LED1LEDSTC));   // t9 Start of LED1 pulse
  LOGI("AFE:   ADCRSTSTCT1:  %4u", spiRead(ADCRSTSTCT1));  // t23 Start of second ADC conversion reset pulse
  LOGI("AFE:   ADCRSTENDCT1: %4u", spiRead(ADCRSTENDCT1)); // t24 End of second ADC conversion reset pulse
  LOGI("AFE:   ALED2CONVST:  %4u", spiRead(ALED2CONVST));  // t15 Start of convert LED2 ambient pulse
  LOGI("AFE:   LED1STC:      %4u", spiRead(LED1STC));      // t7 Start of sample LED1 pulse
  LOGI("AFE:   LED1ENDC:     %4u", spiRead(LED1ENDC));     // t8 End of sample LED1 pulse
  LOGI("AFE:   LED1LEDENDC:  %4u", spiRead(LED1LEDENDC));  // t10 End of LED1 pulse
  LOGI("AFE:   ALED2CONVEND: %4u", spiRead(ALED2CONVEND)); // t16 End of convert LED2 ambient pulse
  LOGI("AFE:   ADCRSTSTCT2:  %4u", spiRead(ADCRSTSTCT2));  // t25 Start of third ADC conversion reset pulse
  LOGI("AFE:   ADCRSTENDCT2: %4u", spiRead(ADCRSTENDCT2)); // t26 End of third ADC conversion reset pulse
  LOGI("AFE:   LED1CONVST:   %4u", spiRead(LED1CONVST));   // t17 Start of convert LED1 pulse
  LOGI("AFE:   ALED1STC:     %4u", spiRead(ALED1STC));     // t11 Start of sample LED1 ambient pulse
  LOGI("AFE:   ALED1ENDC:    %4u", spiRead(ALED1ENDC));    // t12 End of sample LED1 ambient pulse
  LOGI("AFE:   LED1CONVEND:  %4u", spiRead(LED1CONVEND));  // t18 End of convert LED1 pulse
  LOGI("AFE:   LED2LEDSTC:   %4u", spiRead(LED2LEDSTC));   // t3 Start of LED2 pulse
  LOGI("AFE:   ADCRSTSTCT3:  %4u", spiRead(ADCRSTSTCT3));  // t27 Start of fourth ADC conversion reset pulse
  LOGI("AFE:   ADCRSTENDCT3: %4u", spiRead(ADCRSTENDCT3)); // t28 End of fourth ADC conversion reset pulse
  LOGI("AFE:   ALED1CONVST:  %4u", spiRead(ALED1CONVST));  // t19 Start of convert LED1 ambient pulse
  LOGI("AFE:   LED2STC:      %4u", spiRead(LED2STC));      // t1 Start of sample LED2 pulse
  LOGI("AFE:   LED2ENDC:     %4u", spiRead(LED2ENDC));     // t2 End of sample LED2 pulse
  LOGI("AFE:   LED2LEDENDC:  %4u", spiRead(LED2LEDENDC));  // t4 End of LED2 pulse
  LOGI("AFE:   ALED1CONVEND: %4u", spiRead(ALED1CONVEND)); // t20 End of convert LED1 ambient pulse
  disableSPIread();
}

void AFE44XX::dumpRegisters() {
  /*
    Debug Registers incl Timers
  */

  enableSPIread();
  LOGI("AFE:Registers");
  LOGI("Start--------------------------------------------");
  LOGI("CONTROL0:     N.A");
  LOGI("LED2STC:      %4u", spiRead(LED2STC));
  LOGI("LED2ENDC:     %4u", spiRead(LED2ENDC));
  LOGI("LED2LEDSTC:   %4u", spiRead(LED2LEDSTC));
  LOGI("LED2LEDENDC:  %4u", spiRead(LED2LEDENDC));
  LOGI("ALED2STC:     %4u", spiRead(ALED2STC));
  LOGI("ALED2ENDC:    %4u", spiRead(ALED2ENDC));
  LOGI("LED1STC:      %4u", spiRead(LED1STC));
  LOGI("LED1ENDC:     %4u", spiRead(LED1ENDC));
  LOGI("LED1LEDSTC:   %4u", spiRead(LED1LEDSTC));
  LOGI("LED1LEDENDC:  %4u", spiRead(LED1LEDENDC));
  LOGI("ALED1STC:     %4u", spiRead(ALED1STC));
  LOGI("ALED1ENDC:    %4u", spiRead(ALED1ENDC));
  LOGI("LED2CONVST:   %4u", spiRead(LED2CONVST));
  LOGI("LED2CONVEND:  %4u", spiRead(LED2CONVEND));
  LOGI("ALED2CONVST:  %4u", spiRead(ALED2CONVST));
  LOGI("ALED2CONVEND: %4u", spiRead(ALED2CONVEND));
  LOGI("LED1CONVST:   %4u", spiRead(LED1CONVST));
  LOGI("LED1CONVEND:  %4u", spiRead(LED1CONVEND));
  LOGI("ALED1CONVST:  %4u", spiRead(ALED1CONVST));
  LOGI("ALED1CONVEND: %4u", spiRead(ALED1CONVEND));
  LOGI("ADCRSTSTCT0:  %4u", spiRead(ADCRSTSTCT0));
  LOGI("ADCRSTENDCT0: %4u", spiRead(ADCRSTENDCT0));
  LOGI("ADCRSTSTCT1:  %4u", spiRead(ADCRSTSTCT1));
  LOGI("ADCRSTENDCT1: %4u", spiRead(ADCRSTENDCT1));
  LOGI("ADCRSTSTCT2:  %4u", spiRead(ADCRSTSTCT2));
  LOGI("ADCRSTENDCT2: %4u", spiRead(ADCRSTENDCT2));
  LOGI("ADCRSTSTCT3:  %4u", spiRead(ADCRSTSTCT3));
  LOGI("ADCRSTENDCT3: %4u", spiRead(ADCRSTENDCT3));
  LOGI("PRPCOUNT:     %4u", spiRead(PRPCOUNT));
  LOGI("CONTROL1:     %s", intToBinaryString(spiRead(CONTROL1)));
  LOGI("SPARE1:       N.A");
  LOGI("TIAGAIN:      %s", intToBinaryString(spiRead(TIAGAIN)));
  LOGI("TIA_AMB_GAIN: %s", intToBinaryString(spiRead(TIA_AMB_GAIN)));
  LOGI("LEDCNTRL:     %s", intToBinaryString(spiRead(LEDCNTRL)));
  LOGI("CONTROL2:     %s", intToBinaryString(spiRead(CONTROL2)));
  LOGI("SPARE2:       N.A.");
  LOGI("SPARE3:       N.A.");
  LOGI("SPARE4:       N.A.");
  LOGI("RESERVED1:    N.A.");
  LOGI("RESERVED2:    N.A.");
  LOGI("ALARM:        %s", intToBinaryString(spiRead(ALARM)));
  LOGI("DIAG:         %s", intToBinaryString(spiRead(DIAG)));
  LOGI("Values----------------------------------------------");
  LOGI("LED2VAL:      %s", intToBinaryString(spiRead(LED2VAL)));
  LOGI("ALED2VAL:     %s", intToBinaryString(spiRead(ALED2VAL)));
  LOGI("LED1VAL:      %s", intToBinaryString(spiRead(LED1VAL)));
  LOGI("ALED1VAL:     %s", intToBinaryString(spiRead(ALED1VAL)));
  LOGI("LED2ABSVAL:   %s", intToBinaryString(spiRead(LED2ABSVAL)));
  LOGI("LED1ABSVAL:   %s", intToBinaryString(spiRead(LED1ABSVAL)));
  LOGI("End----------------------------------------------");
  disableSPIread();
}

/******************************************************************************************************/
// AFE Start and Stop
/******************************************************************************************************/

bool AFE44XX::start(uint8_t numAverages) {
/*
  Enable Timers
  Attach Interrupt
*/

  LOGD("AFE: --starting");

  // 0) Set CONTROL2 register
  // uint32_t control2Value = readRegister(CONTROL2);
  // control2Value &= ~(0b11 << 17);          // Clear bits 18, 17 ref Voltage
  // control2Value &= ~(0b1 << 16);           // Clear bit 16, NO RST clock on PD ALM (default)
  // control2Value &= ~(0b1 << 15);           // Clear bit 15, use internal ADC
  // control2Value &= ~(0b1 << 10);           // Set bit 10, tristate off
  // control2Value &= ~(0b1 << 8);            // Clear bit 8, set slow diagnosis
  // control2Value &= ~(0b111);               // Clear bits 0, 1, 2 to power up the AFE, Receiver, and Transmitter
  // writeRegister(CONTROL2, control2Value);  // Write the modified value back to the CONTROL2 register

  // 0) Make sure system is powered up
  if (isAFEPoweredDown()) { setPowerAFE(true); }
  if (isRXPoweredDown())  { setPowerRX(true); }
  if (isTXPoweredDown())  { setPowerTX(true); }

  // 1) Attach ISR
  attachInterrupt(digitalPinToInterrupt(_drdyPin), AFE44XX::dataReadyISR, FALLING);
  LOGD("AFE:   attached ISR");

  // 2a) Control 1: Enable timers
  uint32_t control1_settings = readRegister(CONTROL1);
  control1_settings |= (1 << 8); // Set bit 8 to enable timers 
  LOGD("AFE:   Timers are enabled, %s", intToBinaryString(control1_settings));
  // 2b) Control 2: Set Averaging
  if ((numAverages == 0) || (numAverages > 16)) {
      return -1;
  } else {
      control1_settings &= ~(0xFF);            // Clear bits 7 - 0
      control1_settings |= (numAverages << 0); // Set bits 7 to 0 with the averaging value
      LOGD("AFE:   Averaging is %u, %s", numAverages, intToBinaryString(control1_settings));
  }  
  // 2) Set CONTROL1 register
  writeRegister(CONTROL1, control1_settings);
  
  LOGD("AFE: --started");

  return true; // now running
}

bool AFE44XX::stop() {
  /*
    Disable Timers
    Detach Interrupt
  */
  LOGD("AFE: --stopping");

  // 1) Disable Timers
  uint32_t control1_settings = readRegister(CONTROL1);
  control1_settings &= ~(1 << 8);  // Clear bit 8 to disable
  writeRegister(CONTROL1, control1_settings);  // Write the modified value back to the CONTROL2 register
  LOGD("AFE:   Timers are disabled, %s", intToBinaryString(control1_settings));
  
  // Power off TX, RX, AFE
  // uint32_t control2_settings = readRegister(CONTROL2);
  // control2_settings |= 0b111;  // Set bits 0, 1, 2 to power down the AFE, Receiver, and Transmitter
  // writeRegister(CONTROL2, control2_settings);  // Write the modified value back to the CONTROL2 register
  // LOGD("AFE:   TX, RX, and AFE are powered %s, %s", enable ? "on" : "down", intToBinaryString(control2_settings));

  // 2) Detach ISR
  detachInterrupt(digitalPinToInterrupt(_drdyPin));

  LOGD("AFE: --stopped");
  return false; // no longer running
}

/******************************************************************************************************/
// AFE Initialization
/******************************************************************************************************/

void AFE44XX::initalize() {
/*
  Default Initialization Sequence
*/
  // 0) Software Reset
  reset(); // software reset

  // 1) Configure Timers
  setTimers(1.0);

  writeRegister(CONTROL0, CLEAR);       // Clear CONTROL0 register

  // 2) Configure AFE
  bypassADC(false);
  setOscillator(true);
  setShortDiagnostic(true);
  setTristate(true);
  setHbridge(true);
  setAlarmpins(0b101); // None  
  setPowerAFE(true);
  setPowerRX(true);
  setPowerTX(true);

  // 3) Configure Receiver
  setSameGain(true);                    // same gain for LED1 and LED2
  setFirstStageFeedbackCapacitor(5,0);  // 5pF, both LEDs
  setFirstStageFeedbackResistor(5,0);   // 500kOhm both LEDs
  setFirstStageCutOffFrequency(0);      // 500Hz
  setSecondStageGain(1, true, 0);       // 1.5x gain, enabled, both LEDs
  setCancellationCurrent(0);             // 0 microAmp

  // 4) Configure Transmitter
  setLEDdriver(0,0);                    // 0.75V, 150mA
  setLEDpower(24, 0);                   // 24 of 255 on both LEDs

}

////////////////////////////////////////////////////////////////////////////
// Texas Instruments Firmware
////////////////////////////////////////////////////////////////////////////
// setgpio
// =========================================================================

//port set..
// ===========================================================================
// P1.1 - AFE_RESETZ Out, High
// P1.2 - AFE_PDNZ   Out, High
// P2.3 - ADC_RDY    In
// P2.4 - PD_ALM     In
// P2.5 - LED_ALM    In
// P5.7 - DIAG_END   In

// setSPI
// ===========================================================================
// Set SPI peripheral bits
// STE, SCLK, and DOUT as output
// Din as input
// Set STE high
// Enable SW reset
// [b0]   1 -  Synchronous mode
// [b2-1] 00-  3-pin SPI
// [b3]   1 -  Master mode
// [b4]   0 - 8-bit data
// [b5]   1 - MSB first
// [b6]   0 - Clock polarity high.
// [b7]   1 - Clock phase - Data is captured on the first UCLK edge and changed on the following edge.
// SMCLK
// 16 MHz
// Clear SW reset, resume operation

// dataready interrupt
// ===========================================================================
// Enable P2.3 internal resistance
// Set P2.3 as pull-Up resistance
// P2.3 Hi/Lo edge
// P2.3 IFG cleared
// P2.3 interrupt disabled

// set Registers
// =============================================================================
// disableSPIread
// setTimers
// Write CONTROL0 CLEAR
// Write CONTROL2           LED driver
// Write TIAGAIN            PD driver
// Write TIA_AMB_GAIN       PD driver
// Write LEDCNTRL           LED current and power
// Write CONTROL1           
// enableSPIread
