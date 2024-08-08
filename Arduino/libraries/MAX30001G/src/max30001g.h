/******************************************************************************************************/
// Include file for the MAX30001 ECG and Bioimpedance module
//
// Urs Utzinger, July 2024
/******************************************************************************************************/
#ifndef MAX30001_H
#define MAX30001_H

#include "Arduino.h"
#include <SPI.h>
#include "logger.h"

// Define SPI speed
#define MAX30001_SPI_SPEED 1000000

// MAX30001 Register definitions
// Table 12
///////////////////////////////
#define NO_OP           0x00 // No operation R/W
#define STATUS          0x01 // Status register R   
#define EN_INT          0x02 // INT B output R/W
#define EN_INT2         0x03 // INT2B output R/W
#define MNGR_INT        0x04 // Interrupt management R/W
#define MNGR_DYN        0x05 // Dynamic modes managememt R/W
#define SW_RST          0x08 // Software reset W
#define SYNCH           0x09 // Synchronize, begins new operations W
#define FIFO_RST        0x0A // FIFO reset W
#define INFO            0x0F // Information on MAX30001 R
#define CNFG_GEN        0x10 // General settings R/W
#define CNFG_CAL        0x12 // Internal calibration settings R/W
#define CNFG_EMUX       0x14 // Input multiplexer for ECG R/W 
#define CNFG_ECG        0x15 // ECG channle R/W
#define CNFG_BMUX       0x17 // Input multiplext for BIOZ R/W
#define CNFG_BIOZ       0x18 // BIOZ channel R/W
#define CNFG_BIOZ_LC    0x1A // BIOZ low current ranges R/W
#define CNFG_RTOR1      0x1D // R to R heart rate dection R/W
#define CNFG_RTOR2      0x1E // second part of Register R/W
#define NO_OP           0x7F // Same as 0x00

// FIFO is circular memory of 32*24 bits
// Interrupt when threshold for number of samples is reached
// Overflow if write pointer reaches read pointer
#define ECG_FIFO_BURST  0x20 // 
#define ECG_FIFO        0x21 //  
#define BIOZ_FIFO_BURST 0x22 // 
#define BIOZ_FIFO       0x23 // 
#define RTOR            0x25 // 

// FIFO Data
// ECG[23:6] two s complement, 
// ECG[5:3] Tag: Valid, Fast, Valid EOF, Fast EOF, Empty, Overflow
// BIOZ[23:4] two s complement
// BIOZ[2:0] Tag,Valid, Over/Under range, Valid EOF, Over/Under EOF, Empty, Overflow
// RTOR[23:10]

// MAX30001 Commands
#define WRITEREG        0x00
#define READREG         0x01

#define RTOR_INTR_MASK  0x04

typedef enum
{
  SAMPLINGRATE_128 = 128,
  SAMPLINGRATE_256 = 256,
  SAMPLINGRATE_512 = 512
} sampRate;

enum EN_INT_bits
{
  EN_EINT             = 0b100000000000000000000000
  EN_EOVF             = 0b010000000000000000000000
  EN_FSTINT           = 0b001000000000000000000000
  EN_DCLOFFINT        = 0b000100000000000000000000
  EN_BINT             = 0b000010000000000000000000
  EN_BOVF             = 0b000001000000000000000000
  EN_BOVER            = 0b000000100000000000000000
  EN_BUNDER           = 0b000000010000000000000000
  EN_BCGMON           = 0b000000001000000000000000
  //
  EN_LONINT           = 0b000000000000100000000000
  EN_RRINT            = 0b000000000000010000000000
  EN_SAMP             = 0b000000000000001000000000
  EN_PLLINT           = 0b000000000000000100000000
  //
  EN_BCGMP            = 0b000000000000000000100000
  EN_BCGMN            = 0b000000000000000000010000
  EN_LDOFF_PH         = 0b000000000000000000001000
  EN_LDOFF_PL         = 0b000000000000000000000100
  EN_LDOFF_NH         = 0b000000000000000000000010
  EN_LDOFF_NL         = 0b000000000000000000000001
};

/**
 * @brief STATUS (0x01)
 * page 42
 */
typedef union max30001_status_reg
{
  uint32_t all;

  struct
  {
    uint32_t ldoff_nl  : 1; //  0   lead off detection status 
    uint32_t ldoff_nh  : 1; //  1
    uint32_t ldoff_pl  : 1; //  2
    uint32_t ldoff_ph  : 1; //  3

    uint32_t bcgmn     : 1; //  4   BIOZ channel gain monitor, negative output
    uint32_t bcgmp     : 1; //  5   BIOZ channel gain monitor, positive output
    uint32_t reserved1 : 1; //  6  
    uint32_t reserved2 : 1; //  7

    uint32_t pllint    : 1; //  8   PLL unlock interrupt
    uint32_t samp      : 1; //  9   sample synchronization pulse
    uint32_t print     : 1; // 10   ECG R2R detection interrupt
    uint32_t lonint    : 1; // 11   Ultra low power leads on detection interrupt

    uint32_t pedge     : 1; // 12 * 
    uint32_t povf      : 1; // 13 *
    uint32_t print     : 1; // 14 *
    uint32_t bcgmon    : 1; // 15   BIOZ current monitor

    uint32_t bundr     : 1; // 16   BIOZ under range
    uint32_t bover     : 1; // 17   BIOZ over range
    uint32_t bovf      : 1; // 18   BIOZ FIFO  flow
    uint32_t bint      : 1; // 19   BIOZ FIFO interrupt

    uint32_t dcloffint : 1; // 20   DC lead off detection interrupt
    uint32_t fstint    : 1; // 21   ECG fast recovery mode is engaged
    uint32_t eovf      : 1; // 22   ECG FIFO overflow
    uint32_t eint      : 1; // 23   ECG FIFO interrupt

    uint32_t reserved  : 8; //24-31

  } bit;

} max30001_status_t;

/**
 * @brief EN_INT (0x02) and (0x03)
 * page 43
 * we can attach two interrupt lines to functions of the MAX30001
 * multiple interrupts can be enabled at the same time as the bits are OR-ed
 */
typedef union max30001_en_int_reg
{
  uint32_t all;
  struct
  {
    uint32_t int_type     : 2; // 0,1   interrupt type, 00 disabled, CMOS, OpenDrain, OpenDrain with internal pull up
    uint32_t reserved1    : 6; // 2-7
    uint32_t en_pllint    : 1; // 8     PLL unlock interrupt enable
    uint32_t en_samp      : 1; // 9     sample synchronization pulse enable
    uint32_t en_rrint     : 1; // 10    ECG R to R detection interrupt enable
    uint32_t en_lonint    : 1; // 11    Ultra low power leads on detection interrupt enable
    uint32_t reserved2    : 3; // 12-14
    uint32_t en_bcgmon    : 1; // 15    BIOZ current monitor interrupt enable
    uint32_t en_bunder    : 1; // 16    BIOZ under range interrupt enable
    uint32_t en_bover     : 1; // 17    BIOZ over range interrupt enable
    uint32_t en_bovf      : 1; // 18    BIOZ FIFO overflow interrupt enable
    uint32_t en_bint      : 1; // 19    BIOZ FIFO interrupt enable
    uint32_t en_dcloffint : 1; // 20  DC lead off detection interrupt enable
    uint32_t en_fstint    : 1; // 21    ECG fast recovery interrupt enable
    uint32_t en_eovf      : 1; // 22    ECG FIFO overflow interrupt enable
    uint32_t en_eint      : 1; // 23    ECG FIFO interrupt enable
    uint32_t reserved3    : 8  // 24-31
  } bit;
} max30001_en_int_t;

/**
 * @brief MNGR_INT (0x04)
 * page 44
 */
typedef union max30001_mngr_int_reg
{
  uint32_t all;

  struct
  {
    uint32_t samp_it   : 2; //  0,1  sample synchronization pulse frequency
    uint32_t clr_samp  : 1; //  2    sample synchronization pulse clear behavior
    uint32_t clr_pedge : 1; //  3 *
    uint32_t clr_rrint : 2; //  4,5  RTOR R peak detection clear behavior
    uint32_t clr_fast  : 1; //  6,   fast mode interrupt clear behavior
    uint32_t reserved1 : 1; //  7
    uint32_t reserved2 : 4; //  8-11
    uint32_t reserved3 : 4; // 12-15
    uint32_t b_fit     : 3; // 16-18 BIOZ interrupt treshold
    uint32_t e_fit     : 5; // 19-23 ECG interrupt treshold
    uint32_t reserved  : 8; // 24-31
  } bit;

} max30001_mngr_int_t;

/**
 * @brief MNGR_DYN (0x05)
 * page 45
 */
typedef union max30001_mngr_dyn_reg
{
  uint32_t all;

  struct
  {
    uint32_t bloff_lo_it : 8; //  0-7  BIOZ lead off under range threshold
    uint32_t bloff_hi_it : 8; //  8-15  BIOZ lead off over range threshold
    uint32_t fast_th     : 6; // 16-21  ECG fast recovery threshold, 0x3F, if 2048*FAST_TH for more than 125ms go to recovery mode
    uint32_t fast        : 2; // 22,23  ECG fast recovery mode, 00 normal, 01 manual, 11 do not use
    uint32_t reserved    : 8; // 24-31
  } bit;

} max30001_mngr_dyn_t;

/**
 * @brief INFO (0x0F)
 * page 42
 */
typedef union max30001_info_reg
{
  uint32_t all;

  struct
  {
    uint32_t n1        : 4; //  0- 3 might randomly vary
    uint32_t n2        : 4; //  4- 7 might randomly vary
    uint32_t n3        : 4; //  8-11 might randomly vary
    uint32_t c1        : 2; // 12-13 should be 0b01
    uint32_t n4        : 2; // 14-15 might vary randomly
    uint32_t revision  : 4; // 16-19 should be b0101
    uint32_t c2        : 4; // 20-23
    uint32_t reserved  : 8; // 24-31

  } bit;

} max30001_info_t;

/**
 * @brief CNFG_GEN (0x10)
 * page 47
 */
typedef union max30001_cnfg_gen_reg
{
  uint32_t all;
  struct
  {
    uint32_t rbiasn     : 1; //  0,   enable resistive bias on N
    uint32_t rbiasp     : 1; //  1,   enable resistive bias on P
    uint32_t rbiasv     : 2; //  2,3  bias mode: 50, 100, 200MOhm, do notuse
    uint32_t en_rbias   : 2; //  4,5  disabled, ECG, BIOZ, reserved
    uint32_t vth        : 2; //  6,7  lead off threshold, 300, 400, 450, 500mV
    uint32_t imag       : 3; //  8-10 lead off magnitude selection, 0, 5, 10, 20, 50, 100 nA
    uint32_t ipol       : 1; // 11    lead off curren polarity, ECGN pullup, ECGN pull down
    uint32_t en_dcloff  : 2; // 12,13 lead off enable
    uint32_t en_bloff   : 2; // 14,15 BIOZ lead off enable
    uint32_t reserved1  : 1; // 16
    uint32_t en_pace    : 1; // 17 *  ?
    uint32_t en_bioz    : 1; // 18    enable BIOZ channel
    uint32_t en_ecg     : 1; // 19    enable ECG channel
    uint32_t fmstr      : 2; // 20,21 master clock frequency
    uint32_t en_ulp_lon : 2; // 22,23 ultra low power lead on detection
    uint32_t reserved   : 8; // 24-31
  } bit;

} max30001_cnfg_gen_t;

/**
 * @brief CNFG_CAL (0x12)
 * page 49
 */
typedef union max30001_cnfg_cal_reg
{
  uint32_t all;
  struct
  {
    uint32_t thigh      : 11; //  0-10, calibration time source selection
    uint32_t fifty      :  1; // 11     calibrationd duty cycle mode
    uint32_t fcal       :  3; // 12-14  calibration frequency selection
    uint32_t reserved1  :  5; // 15-19
    uint32_t vmag       :  1: // 20     calibration voltage magnitude
    uint32_t vmode      :  1; // 21     calibration voltage mode
    uint32_t vcal       :  1; // 22     calibration voltage enable
    uint32_t reserved1  :  1; // 23
    uint32_t reserved   :  8; // 24-31
  } bit;

} max30001_cnfg_cal_t;

/**
 * @brief CNFG_EMUX  (0x14)
 * page 50
 */
typedef union max30001_cnfg_emux_reg
{
  uint32_t all;
  struct
  {
    uint32_t reserved1 : 16; // 0-15 
    uint32_t caln_sel  :  2; // 16,17 ECGN calibration selection
    uint32_t calp_sel  :  2; // 18,19 ECGP calibration selection
    uint32_t openn     :  1; // 20 open the ECG input switch
    uint32_t openp     :  1; // 21 open the ECG input switch
    uint32_t reserved2 :  1; // 22 
    uint32_t pol       :  1; // 23 ECG input polarity selection
    uint32_t reserved  :  8; // 24-31
  } bit;

} max30001_cnfg_emux_t;

/**
 * @brief CNFG_ECG   (0x15)
 * page 51
 */
typedef union max30001_cnfg_ecg_reg
{
  uint32_t all;
  struct
  {
    uint32_t reserved1  : 12; //  0-11
    uint32_t dlpf       :  2; // 12,13 ecg digital low pass cut off
    uint32_t dhpf       :  1; // 14 ecg digtical high pass cut off 
    uint32_t reserved2  :  1; // 15
    uint32_t gain       :  2; // 16,17 ecg gain
    uint32_t reserved3  :  4; // 18-21 
    uint32_t rate       :  2; // 22,23 ecg rate
    uint32_t reserved   :  8;
  } bit;

} max30001_cnfg_ecg_t;

/**
 * @brief CNFG_BMUX   (0x17)
 * page 52
 */
typedef union max30001_cnfg_bmux_reg
{
  uint32_t all;
  struct
  {
    uint32_t fbist      : 2; //  0,1  BIOZ frequency selection
    uint32_t reserved1  : 2; //  2,3  
    uint32_t rmod       : 3; //  4-6  BIOZ modulated resistance selection
    uint32_t reserved2  : 1; //  7    
    uint32_t rnom       : 3; //  8-10 BIOZ nominal resistance selection
    uint32_t en_bist    : 1; // 11    BIOZ bistable mode enable
    uint32_t cg_mode    : 2; // 12,13 BIOZ current generator mode selection
    uint32_t reserved3  : 2; // 14,15  
    uint32_t caln_sel   : 2; // 16,17 BI n calibration selection
    uint32_t calp_sel   : 2; // 18,19 BI p calibration selection
    uint32_t openn      : 1; // 20    open the BIOZ N input switch
    uint32_t openp      : 1; // 21    open the BIOZ P input switch 
    uint32_t reserved4  : 2; // 22,23 
    uint32_t reserved   : 8; // 24-31
  } bit;

} max30001_cnfg_bmux_t;

/**
 * @brief CNFG_BIOZ   (0x18)
 * page 54
 */
typedef union max30001_bioz_reg
{
  uint32_t all;
  struct
  {
    uint32_t phoff      : 4; // 0-3   BIOZ modulation phase offset
    uint32_t cgmag      : 3; // 4-6   BIOZ current generator magnitude 8,16,32,48,64,80,96 microA
    uint32_t cgmon      : 1; // 7     BIOZ current generator monitor
    uint32_t fcgen      : 4; // 8-11  BIOZ current generator frequency
    uint32_t dlpf       : 2; // 12,13 BIOZ digital low pass cut off 4,8,16Hz
    uint32_t dhpf       : 2; // 14,15 BIOZ digital high pass cut off 0.05, 0.5Hz
    uint32_t gain       : 2; // 16,17 BIOZ gain 10,20,40,80V/V
    uint32_t ln_bioz    : 1; // 18    BIOZ INA power mode
    uint32_t ext_rbias  : 1; // 19    BIOZ external resistive bias enable
    uint32_t ahpf       : 3; // 20-22 BIOZ analog high pass cut off, 60..4000Hz
    uint32_t rate       : 1; // 23    BIOZ rate samples per second
    uint32_t reserved   : 8; // 24-31 
  } bit;

} max30001_cnfg_bioz_t;

/**
 * @brief CNFG_BIOZ_LC (0x1A)
 * page 57
 */
typedef union max30001_bioz_lc_reg
{
  uint32_t all;
  struct
  {
    uint32_t cmag_lc    : 4; // 0-3   Bioz low current manitude selection
    uint32_t cmres      : 4; // 4-7   Bioz low current mode resistor selection
    uint32_t reserved1  : 4; // 8-11
    uint32_t bistr      : 2; // 12,13 High resistance programmable load value selection
    uint32_t en_bistr   : 1; // 14    High resistance programmable load enable
    uint32_t reserved2  : 4; // 15-18
    uint32_t lc2x       : 1; // 19    Bioz low current 2x mode enable
    uint32_t reserved3  : 3; // 20-22
    uint32_t hi_lob     : 1; // 23    Bioz high or low current mode selection
    uint32_t reserved3  : 8; // 24-31 
  } bit;

} max30001_cnfg_bioz_lc_t;

/**
 * @brief CNFG_RTOR1   (0x1D)
 * page 59
 */
typedef union max30001_cnfg_rtor1_reg
{
  uint32_t all;
  struct
  {
    uint32_t reserved1  : 8; // 0-7   
    uint32_t ptsf       : 4; // 8-11  R tp R peak threshold scaling facto
    uint32_t pavg       : 2; // 12,13 R to R peak averaging weight factor
    uint32_t reserved2  : 1; // 14    
    uint32_t en_rtor    : 1; // 15    ECG R to R detection enable
    uint32_t gain       : 4; // 16-19 R to R gain 
    uint32_t wndw       : 4; // 20-23 Width of averaging window
    uint32_t reserved   : 8; // 24-31
  } bit;

} max30001_cnfg_rtor1_t;

/**
 * @brief CNFG_RTOR2 (0x1E)
 * page 59
 */
typedef union max30001_cnfg_rtor2_reg
{
  uint32_t all;
  struct
  {
    uint32_t reserved1  : 8; // 0-7
    uint32_t rhsf       : 3; // 8-10 R to R peak threshold hold off scaling factor
    uint32_t reserved2  : 1; // 11
    uint32_t ravg       : 2; // 12,13 R to R peak averaging interval weight factor
    uint32_t reserved3  : 2; // 14,15
    uint32_t hoff       : 6; // 16-21 R to R peak detection threshold hold off
    uint32_t reserved4  : 2; // 22,23
    uint32_t reserved   : 8; // 24-31
  } bit;

} max30001_cnfg_rtor2_t;

/**
 * @brief ECG_FIFO_BURST
 * page 51
 */
typedef union max30001_ecg_burst_reg
{
  uint32_t all;
  struct
  {
    uint32_t reserved :   3; //  0-2
    uint32_t etag     :   3; //  3-5 ECG tag
    uint32_t data      :  18; //  6-23 ECG data  
  } bit;

} max30001_ecg_burst_t;

/**
 * @brief BIOZ_FIFO_BURST
 * page 51
 */
typedef union max30001_bioz_burst_reg
{
  uint32_t all;
  struct
  {
    uint32_t btag     :   3;  //  0-2 BIOZ tag
    uint32_t reserved :   1;  //  3
    uint32_t data      :  18; //  4-23 BIOZ data  
  } bit;

} max30001_bioz_burst_t;

// We want to emulate the MAX30001 Evaluation Kit User Interface
// =============================================================
// Max Global
//   Clock 3276800
//   Enable ECG, BioZ, RtoR
//
//
// Set ECG Channel
// ---------------
//  AAF 
//   Fast Recovery Mode
//   Fast Recovery Threshold
//  PGA
//   Gain
//  ADC
//   Sampling Rate
//  Digital Filters
//   Low Pass Filter Cut Off
//   High Pass Filter Cut Off
//  R to R
//   Gain (Auto)
//   Window 12*8ms
//   Minimum Hold Off 32
//   Peak Averaging Weight Factor 8
//   Peak Threshold Scaling Factor 4/16
//   Interval Hold Off Scaling Factor 4/8
//   Interval Averaging Weight Factor 8
//  FIFO 
//   EFIT 16
//
// Set ECG MUX
// -----------
// DC Lead Off
//   Enable
//   Current Polarity
//   Current Magnitude
//   Lead Off Voltage Threshold
// Load On Check
//   Enable
// Switches
//   ECGP Switch, Isolated
//   ECGN Switch, Isolated
//   ECGP/N Polarity, Non-Inverted
// Lead Bias
//   Resistive Lead Bias Enable
//   Resistive Bias Value, 100mOhm
//   Positive Input Bias Enable, Connected
//   Negative Input Bias Enable, Connected
// Calibration Test Value
//   ECGP Calibration, None
//   ECGN Calibration, None
//   Calibration Enable
//   Mode Enable
//   Voltage
//   Frequency
//   Duty Cycle, 50%
//   Time High, microseconds
//
// BioZ Channel Detection
// ----------------------
// HPF
//   Cut Off Frequency
// INA Power Mode
// Modulation Phase Offset
// Channel Gain
// ADC
//   Sampling Rate
// Digital Filters
//   Low Pass Filter Cut Off, 4Hz
//   High Pass Filter Cut Off, bypass
// BioZ Channel Driver
// -------------------
// External Resisistor Bias Enable
// Current Generator Frequency
// Mode
// CM Resistor, 40MOhm
// Monitor, disabled
// Drive Current Select 55to550nA
// Low Driver Current Range 110/220nA
// High Drive Current Magnitude, 8microA
//
// Set BIOZ MUX
// -----------
// DC Lead Off
//   Enable
//   Current Polarity
//   Current Magnitude
//   Lead Off Voltage Threshold
// Load On Check
//   Enable
// Switches
//   BIP Switch, Connected
//   BIN Switch, Connected
// Lead Bias
//   Resistive Lead Bias Enable
//   Resistive Bias Value, 100mOhm
//   Positive Input Bias Enable, Connected
//   Negative Input Bias Enable, Connected
// Calibration Test Value
//   BIP Calibration, None
//   BIN Calibration, None
//   Calibration Enable
//   Mode Enable, Unipolar
//   Voltage
//   Frequency
//   Duty Cycle, 50%
//   Time High, microseconds
//
// BIOZ Load
// ---------
// RMOD BIST enable, 
// Normal Resistance, 5000Ohm
// Frequency
// BIST resistor enable
// BIST resistor value 27kOhm
//
// Check Status
// ---------------
// ECG Parameter
// BIOZ Parameter
// R to R Parameter
// FMSTR Parameter
// Interrupts
//
// Read Registers
// --------------
// Read all


class MAX30001 {

    public:
        MAX30001(int csPin);

        // Setup Functions
        void beginECGonly(); // initialize driver for ECG
        void beginECGandBIOZ(); // initialize driver for ECG and BIOZ
        void beginBIOZonly(); // initialize driver for BIOZ
        void beginRtoR(); // initialize driver ECG RTOR

        // Configuration
        void setSamplingRate(uint8_t ECG_samplingRate, uint8_t BIOZ_samplingRate);

        void setInterrupts(uint32_t interrupts);
        void serviceAllInterrupts();
        

        // Accessing readings
        void getECGData(int32_t &???);
        void getBIOZData(int32_t &???);
        void getHRandRtoRData(int32_t &???);


        // Diagnostics
        bool readInfo(void);
        bool readStatus(void);
        void printStatus(uint32_t &status);
        // bool selfCheck();
        // void dumpRegisters();

        // Data Variables
        // --------------
        volatile int ecg_available;
        volatile int bioz_available;

        float   ECG_data[128];
        int32_t BIOZ_dataData[128];
        int     ecg_counter;
        int     bioz_counter;
        
        volatile int rtor_available;
        int32_t heart_rate;
        int32_t rr_interval;

        // Status of the MAX30001 timing
        // -----------------------------
        float    ECG_samplingRate;
        float    BIOZ_samplingRate;
        float    RtoR_resolution;
        float    CAL_resolution;
        float    fmstr;
        float    tres;
        float    ECG_progression;
        float    ECG_hpf;
        float    ECG_lpf;
        int      ECG_gain;
        int      BIOZ_gain;
        int      BIOZ_cgmag;
        float    BIOZ_frequency;// HZ
        uint32_t BIOZ cmres;    // [kOhm]
        int32_t  V_ref = 1000;  // [mV]

        // All Configuration and Status Registers

        max30001_status_t status;                   // 0x01
        max30001_en_int_t en_int;                   // 0x02
        max30001_en_int_t en_int2;                  // 0x03
        max30001_mngr_int_t mngr_int;               // 0x04
        max30001_mngr_dyn_t mngr_dyn;               // 0x05
        max30001_info_t info;                       // 0x0f
        max30001_cnfg_gen_t cnfg_gen;               // 0x10
        max30001_cnfg_cal_t cnfg_cal;               // 0x12
        max30001_cnfg_emux_t cnfg_emux;             // 0x14
        max30001_cnfg_ecg_t cnfg_ecg;               // 0x15
        max30001_cnfg_bmux_t cnfg_bmux;             // 0x17
        max30001_cnfg_bioz_t cnfg_bioz;             // 0x18
        max30001_cnfg_bioz_lc_t cnfg_bioz_lc;       // 0x1A
        max30001_cnfg_rtor1_t cnfg_rtor1;           // 0x1D
        max30001_cnfg_rtor2_t cnfg_rtor2;           // 0x1D
        
    private:

        // Higher level
        void readECGFIFO(int num_bytes);
        void readBIOZFIFO(int num_bytes);
        void synch(void);
        void swReset(void);
        void fifoReset(void);

        // SPI service functions
        void     writeRegister(uint8_t address, uint32_t data);
        uint32_t readRegister(uint8_t address);
        uint8_t  readRegister(uint8_t address);

        // void     spiWrite(uint8_t address, uint32_t data);
        // uint32_t spiRead(uint8_t address);

        // Interrupt service routine
        // static void dataReadyISR();

        // Global Variables
        max30001_status_reg status;  // content of status register
        int _csPin;                  // chip select for SPI
        uint32_t _bufferECG[32];  // 32 samples
        uint32_t _bufferBIOZ[32]; // 32 samples
};

#endif // MAX30001_H

