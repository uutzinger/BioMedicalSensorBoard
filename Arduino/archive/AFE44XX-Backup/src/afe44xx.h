/******************************************************************************************************/
// Include file for the AFE44XX Pulse Oximeter Driver
//
// Urs Utzinger, 2024
/******************************************************************************************************/
#ifndef AFE44XX_H
#define AFE44XX_H

#include "Arduino.h"
#include <SPI.h>
#include "logger.h"

// Define SPI speed
#define AFE44XX_SPI_SPEED 2000000

// AFE44XX Register definitions
///////////////////////////////
#define CONTROL0      0x00
// timing
#define LED2STC       0x01
#define LED2ENDC      0x02
#define LED2LEDSTC    0x03
#define LED2LEDENDC   0x04
#define ALED2STC      0x05
#define ALED2ENDC     0x06
#define LED1STC       0x07
#define LED1ENDC      0x08
#define LED1LEDSTC    0x09
#define LED1LEDENDC   0x0a
#define ALED1STC      0x0b
#define ALED1ENDC     0x0c
#define LED2CONVST    0x0d
#define LED2CONVEND   0x0e
#define ALED2CONVST   0x0f
#define ALED2CONVEND  0x10
#define LED1CONVST    0x11
#define LED1CONVEND   0x12
#define ALED1CONVST   0x13
#define ALED1CONVEND  0x14
#define ADCRSTSTCT0   0x15
#define ADCRSTENDCT0  0x16
#define ADCRSTSTCT1   0x17
#define ADCRSTENDCT1  0x18
#define ADCRSTSTCT2   0x19
#define ADCRSTENDCT2  0x1a
#define ADCRSTSTCT3   0x1b
#define ADCRSTENDCT3  0x1c
#define PRPCOUNT      0x1d
//
#define CONTROL1      0x1e
#define SPARE1        0x1f
#define TIAGAIN       0x20
#define TIA_AMB_GAIN  0x21
#define LEDCNTRL      0x22
#define CONTROL2      0x23
#define SPARE2        0x24
#define SPARE3        0x25
#define SPARE4        0x26
#define RESERVED1     0x27
#define RESERVED2     0x28
#define ALARM         0x29
#define LED2VAL       0x2a
#define ALED2VAL      0x2b
#define LED1VAL       0x2c
#define ALED1VAL      0x2d
#define LED2ABSVAL    0x2e
#define LED1ABSVAL    0x2f
#define DIAG          0x30

// AFE44XX Commands for CONTROL0 register
#define SPI_READ_ENABLE  (0b00000000000000000000000000000001)
#define SW_RST           (0b00000000000000000000000000001000)
#define CLEAR            (0b00000000000000000000000000000000)
#define DIAGNOSIS        (0b00000000000000000000000000000100)

class AFE44XX {

    public:
        AFE44XX(int csPin, int pwdnPin, int drdyPin);

        // Global Variables
        static volatile boolean afe44xx_data_ready;
        uint32_t                afe44xx_diag_status;

        // Functions.
        void begin(); // initialize driver
        void reset(); // soft reset
        void clearCNTRL0(); // clear CONTROL0 register
        bool start(uint8_t numAverages); // start data acquisition, must initilize first
        bool stop(); // stop data acquisition
        void initalize(); // initialize the AFE44XX

        // Accessing readings
        void getData(int32_t &irRawValue, int32_t &redRawValue,
                    int32_t &irAmbientValue, int32_t &redAmbientValue,
                    int32_t &irValue, int32_t &redValue);

        // Receiver Stage
        int8_t setCancellationCurrent(uint8_t current);
        int8_t setSameGain(bool enable);
        int8_t setSecondStageGain(uint8_t gain, bool enabled, uint8_t LED);
        int8_t setFirstStageFeedbackCapacitor(uint16_t cap, uint8_t LED);
        int8_t setFirstStageFeedbackResistor(uint16_t resistance, uint8_t LED);
        int8_t setFirstStageCutOffFrequency(uint8_t frequency);

        // Transmitter Stage
        uint8_t getLEDpower(uint8_t LED);
        int8_t  setLEDdriver(uint8_t txRefVoltage, uint8_t range);
        void    setLEDpower(uint8_t power, uint8_t LED);

        // AFE Behavior
        void bypassADC(bool enable);
        void setOscillator(bool enable);
        void setShortDiagnostic(bool enable);
        void setTristate(bool enable);
        void setHbridge(bool enable);
        int8_t setAlarmpins(uint8_t alarmpins);
        void setPowerRX(bool enable);
        void setPowerAFE(bool enable);
        void setPowerTX(bool enable);
        bool isTXPoweredDown();
        bool isRXPoweredDown();
        bool isAFEPoweredDown();
        
        // Configure Timer
        int8_t setTimers(float factor);

        // Diagnostics
        void readDiag(uint32_t &status);
        void printDiag(uint32_t &status);
        bool selfCheck();
        void readTimers();
        void dumpRegisters();

    private:
        int _csPin;    // chip select for SPI
        int _pwdnPin;  // power down pin
        int _drdyPin;  // data ready interrupt pin

        // SPI service functions
        void     writeRegister(uint8_t address, uint32_t data);
        uint32_t readRegister(uint8_t address);
        void     enableSPIread();
        void     disableSPIread();
        void     spiWrite(uint8_t address, uint32_t data);
        uint32_t spiRead(uint8_t address);

        // Interrupt service routine / Accessing readings
        static void dataReadyISR();

        // Utility functions
        uint32_t findCapacitancePattern(int16_t desiredValue);
        int32_t  convert22Int(uint32_t number); // 22bit twos complement conversion to integer
};

#endif // AFE44XX_H

