/******************************************************************************************************/
// Include file for the MAX30001G ECG and Bioimpedance module
//
// Urs Utzinger, July 2024
/******************************************************************************************************/
#ifndef MAX30001_H
#define MAX30001_H

#include "Arduino.h"
#include <SPI.h>
#include "logger.h"
#include <cstddef>

/******************************************************************************************************/
/* Ring Buffer */
/******************************************************************************************************/

class RingBuffer {
public:
    // Constructor and destructor
    explicit RingBuffer(size_t size);
    ~RingBuffer();

    // Public methods
    void push(float value);
    float pop();
    bool isFull() const;
    bool isEmpty() const;
    bool isOverflow() const;
    void resetOverflow();
    size_t size() const;

private:
    // Private members
    float* buffer;
    size_t capacity;
    size_t start;
    size_t end;
    size_t count;
    bool overflow;
};

/******************************************************************************************************/
/* MAX30001G */
/******************************************************************************************************/

// Define SPI speed
#define MAX30001_SPI_SPEED 1000000

// MAX30001 Register definitions
// Table 12
///////////////////////////////////////////////////
#define MAX30001_NO_OP           0x00 // No operation R/W
#define MAX30001_STATUS          0x01 // Status register R   
#define MAX30001_EN_INT1         0x02 // INT B output R/W
#define MAX30001_EN_INT2         0x03 // INT2B output R/W
#define MAX30001_MNGR_INT        0x04 // Interrupt management R/W
#define MAX30001_MNGR_DYN        0x05 // Dynamic modes managememt R/W
#define MAX30001_SW_RST          0x08 // Software reset W
#define MAX30001_SYNCH           0x09 // Synchronize, begins new operations W
#define MAX30001_FIFO_RST        0x0A // FIFO reset W
#define MAX30001_INFO            0x0F // Information on MAX30001 R
#define MAX30001_CNFG_GEN        0x10 // General settings R/W
#define MAX30001_CNFG_CAL        0x12 // Internal calibration settings R/W
#define MAX30001_CNFG_EMUX       0x14 // Input multiplexer for ECG R/W 
#define MAX30001_CNFG_ECG        0x15 // ECG channle R/W
#define MAX30001_CNFG_BMUX       0x17 // Input multiplext for BIOZ R/W
#define MAX30001_CNFG_BIOZ       0x18 // BIOZ channel R/W
#define MAX30001_CNFG_BIOZ_LC    0x1A // BIOZ low current ranges R/W
#define MAX30001_CNFG_RTOR1      0x1D // R to R heart rate dection R/W
#define MAX30001_CNFG_RTOR2      0x1E // second part of Register R/W

// FIFO is a circular memory of 32*24 bits
// There is interrupt when threshold for number of samples is reached
// There is overflow if write pointer reaches read pointer
// FIFO Data:
// ECG[ 23: 6] two s complement, 
// ECG[  5: 3] Tag: Valid, Fast, Valid EOF, Fast EOF, Empty, Overflow
// BIOZ[23: 4] two s complement
// BIOZ[ 2: 0] Tag,Valid, Over/Under range, Valid EOF, Over/Under EOF, Empty, Overflow
// RTOR[23:10] two s complement
#define MAX30001_ECG_FIFO_BURST  0x20 // 
#define MAX30001_ECG_FIFO        0x21 //  
#define MAX30001_BIOZ_FIFO_BURST 0x22 // 
#define MAX30001_BIOZ_FIFO       0x23 // 
#define MAX30001_RTOR            0x25 // 

// MAX30001 Commands
#define MAX30001_WRITEREG        0x00
#define MAX30001_READREG         0x01

#define MAX30001_RTOR_INTR_MASK  0x04

// MAX30001 Registers
///////////////////////////////////////////////////
// MAX30001 EN_INT Register Bit Masks
#define MAX30001_EN_INT_EINT             (1 << 23)
#define MAX30001_EN_INT_EOVF             (1 << 22)
#define MAX30001_EN_INT_FSTINT           (1 << 21)
#define MAX30001_EN_INT_DCLOFFINT        (1 << 20)
#define MAX30001_EN_INT_BINT             (1 << 19)
#define MAX30001_EN_INT_BOVF             (1 << 18)
#define MAX30001_EN_INT_BOVER            (1 << 17)
#define MAX30001_EN_INT_BUNDER           (1 << 16)
#define MAX30001_EN_INT_BCGMON           (1 << 15)

#define MAX30001_EN_INT_LONINT           (1 << 11)
#define MAX30001_EN_INT_RRINT            (1 << 10)
#define MAX30001_EN_INT_SAMP             (1 << 9)
#define MAX30001_EN_INT_PLLINT           (1 << 8)

#define MAX30001_EN_INT_BCGMP            (1 << 5)
#define MAX30001_EN_INT_BCGMN            (1 << 4)
#define MAX30001_EN_INT_LDOFF_PH         (1 << 3)
#define MAX30001_EN_INT_LDOFF_PL         (1 << 2)
#define MAX30001_EN_INT_LDOFF_NH         (1 << 1)
#define MAX30001_EN_INT_LDOFF_NL         (1 << 0)

/******************************************************************************************************/
/* Register Structures
/******************************************************************************************************/

/**
 * @brief STATUS (0x01)
 * page 42
 **/
typedef union
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

    uint32_t reserved3 : 8; //24-31

  } bits;

} max30001_status_reg_t;

/**
 * @brief EN_INT (0x02) and (0x03)
 * page 43
 * we can attach two interrupt lines to functions of the MAX30001
 * multiple interrupts can be enabled at the same time as the bits are OR-ed
 **/
typedef union
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
} max30001_en_int_reg_t;

/**
 * @brief MNGR_INT (0x04)
 * page 44
 */
typedef union
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

} max30001_mngr_int_reg_t;

/**
 * @brief MNGR_DYN (0x05)
 * page 45
 */
typedef union
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

} max30001_mngr_dyn_reg_t;

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

} max30001_info_reg_t;

/**
 * @brief CNFG_GEN (0x10)
 * page 47
 */
typedef union
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
    uint32_t en_pace    : 1; // 17 *  not in the datasheet
    uint32_t en_bioz    : 1; // 18    enable BIOZ channel
    uint32_t en_ecg     : 1; // 19    enable ECG channel
    uint32_t fmstr      : 2; // 20,21 master clock frequency
    uint32_t en_ulp_lon : 2; // 22,23 ultra low power lead on detection
    uint32_t reserved   : 8; // 24-31
  } bit;

} max30001_cnfg_gen_reg_t;

/**
 * @brief CNFG_CAL (0x12)
 * page 49
 */
typedef union
{
  uint32_t all;
  struct
  {
    uint32_t thigh      : 11; //  0-10, calibration time source selection
    uint32_t fifty      :  1; // 11     calibrationd duty cycle mode
    uint32_t fcal       :  3; // 12-14  calibration frequency selection
    uint32_t reserved1  :  5; // 15-19
    uint32_t vmag       :  1; // 20     calibration voltage magnitude
    uint32_t vmode      :  1; // 21     calibration voltage mode
    uint32_t vcal       :  1; // 22     calibration voltage enable
    uint32_t reserved2  :  1; // 23
    uint32_t reserved3  :  8; // 24-31
  } bit;

} max30001_cnfg_cal_reg_t;

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
    uint32_t reserved3 :  8; // 24-31
  } bit;

} max30001_cnfg_emux_reg_t;

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

} max30001_cnfg_ecg_reg_t;

/**
 * @brief CNFG_BMUX   (0x17)
 * page 52
 */
typedef union
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

} max30001_cnfg_bmux_reg_t;

/**
 * @brief CNFG_BIOZ   (0x18)
 * page 54
 */
typedef union
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

} max30001_cnfg_bioz_reg_t;

/**
 * @brief CNFG_BIOZ_LC (0x1A)
 * page 57
 */
typedef union
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

} max30001_cnfg_bioz_lc_reg_t;

/**
 * @brief CNFG_RTOR1   (0x1D)
 * page 59
 */
typedef union
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

} max30001_cnfg_rtor1_reg_t;

/**
 * @brief CNFG_RTOR2 (0x1E)
 * page 59
 */
typedef union
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

} max30001_cnfg_rtor2_reg_t;

/**
 * @brief ECG_FIFO_BURST
 * page 61
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

} max30001_ecg_burst_reg_t;

/**
 * @brief BIOZ_FIFO_BURST
 * page 61
 */
typedef union
{
  uint32_t all;
  struct
  {
    uint32_t btag     :   3; //  0-2 BIOZ tag
    uint32_t reserved :   1; //  3
    uint32_t data     :  20; //  4-23 BIOZ data  
  } bit;

} max30001_bioz_burst_reg_t;


/**
 * @brief R to R FIFO
 * page 61
 */
typedef union max30001_rtor_reg
{
  uint32_t all;
  struct
  {
    uint32_t reserved :  10; // 
    uint32_t data     :  14; //  10-23 BIOZ data  
  } bit;

} max30001_rtor_reg_t;

/******************************************************************************************************/
/* Structures */
/******************************************************************************************************/

struct ImpedanceModel {
    float magnitude;
    float phase;
};

/******************************************************************************************************/
/* Device Driver */
/******************************************************************************************************/

class MAX30001 {

    public:
        MAX30001(int csPin);

        // Clock
        void setFMSTR(uint8_t fmstr);

        // Setup Functions
        void setupECG();          // initialize AFE for ECG & RtoR mode
        void setupECGCal();       // initialize AFE for ECG calibration mode
        void setupBIOZ();         // initialize AFE for BIOZ
        void setupBIOZcalibrationInternal();
        void setupBIOZcalibrationExternal();
        void setupECGandBIOZ();   // initialize AFE for ECG and BIOZ
        void scanBIOZ();          // impedance scan from 1..100kHz

        // Interrupt
        void serviceAllInterrupts();
        void setInterrupt1(bool ecg, bool bioz, bool leadon, bool leadoff, bool fourwire);
        void setInterrupt2(bool ecg, bool bioz, bool leadon, bool leadoff, bool fourwire);
        void setInterruptClearing(void);
        void setFIFOInterruptThreshold(uint8_t ecg,  uint8_t bioz);

        // Update globals based on master clock and settings
        void updateGlobalECG_samplingRate(void);
        void updateGlobalBIOZ_samplingRate(void);
        void updateGlobalCAL_fcal(void);
        void updateGlobalECG_latency(void);
        void updateGlobalBIOZ_frequency(void);
        void updateGlobalECG_lpf(void);
        void updateGlobalBIOZ_lpf(void);
        void updateGlobalRCAL_freq(void);

        // ECG and BIOZ settings
        // ---------------------
        void enableAFE(bool ECG, bool BIOZ, bool RtoR);
        // enable subsystems
        void setLeadDetection(bool off_detect, bool on_detect, bool bioz_4, int electrode_impedance);
        // set after systems are enabled
        // bioz_4: 4 wire or 2 wire bioz
    -   // Impedance in MOhm 
        // 0     0        0nA    
        // 2  <= 2 MOhm 100nA
        // 4  <= 4 MOhm  50nA
        // 10 <=10 MOhm  20nA
        // 20 <=20 MOhm  10nA
        // 40  >20 MOhm   5nA
        void setLeadBias(bool enableECG, bool enableBIOZ, uint8_t resistance);
        // Internal Lead Bias if VMC is not used
        // enable ECG
        // enable BIOZ
        // 0..75MOhm, 75..150MOhm, >150MOhm
        void setCalibration(bool enableECGCal, bool enableBIOZCal, bool unipolar, bool cal_vmag, uint8_t freq, uint8_t dutycycle)
        // apply calibration signal to input stage
        // cal_vmag true for ±0.5 mV, false for ±0.25 mV 
        // frequ 0..7 approx 250..0.01 Hz
  -     // dutycycle: default 50%
        void setDefaultNoCalibration(); // off
        void setDefaultECGCalibration(); // 1Hz
        void setDefaultBiozCalibration() // 1Hz


        // ECG settings
        // ------------
        void setECGSamplingRate(uint8_t  ECG);
        // 0:125, 1:256, 2:512sps
        void setECGfilter(uint8_t lpf, uint8_t hpf);
        // 0:bypass, 1:40, 2:200 3:150Hz LPF
        // 0:bypass, 1:0.5Hz HPF
        void setECGgain(uint8_t gain);
        // 0:  20V/V
        // 1:  40V/V
        // 2:  80V/V
        // 3: 160V/V 
        void setECGLeadPolarity(bool inverted, bool open);
        // 0: normal, 1: inverted
        // 0: open for calibration, 1: closed to measure samples
        void setECGAutoRecovery(int threshold_voltage);
        // should be set to <= 98% of the V_ref / ECG_gain
        // 0.98 * 1V / 80V/V * 1000 mV/V = 12.25 mV
        void setECGNormalRecovery();
        // recovery disabled
        void startECGManualRecovery();
        // call after saturation detected with your own program
        void stopECGManualRecovery();
        // manual start needs manual stop
    
        // RtoR
        // ----
        void setRtoR(bool enable, uint8_t ptsf, uint8_t pavg, uint8_t gain, uint8_t wndw, uint8_t hoff, uint8_t ravg, uint8_t rhsf);
        void setDefaultRtoR();
        // ptsf: 0b0011 (4) (4/16) or 6
        // pavg: 0b10 (8)
        // gain: 0b1111 (Auto-Scale)
        // wndw: 0b011 12 (96ms)
        // hoff: 0b100000 (32)
        // ravg: 0b10 (8)
        // rhsf: 0b100 (4/8)

        // BIOZ settings
        // -------------
        void setBIOZSamplingRate(uint8_t BIOZ);
        // 0 =  low   25-32 sps * default
        // 1 =  high  50-64 sps
        void setBIOZgain(uint8_t gain);
        // 0 = 10V/V * default
        // 1 = 20V/V
        // 2 = 40V/V
        // 3 = 80V/V
        void setBIOZModulationFrequencyByFrequency(uint16_t frequency);
        //idx    freq          nA
        //  0 128 000  55 - 96000
        //  1  80 000  55 - 96000
        //  2  40 000  55 - 96000
        //  3  18 000  55 - 96000
        //  4   8 000  55 - 80000
        //  5   4 000  55 - 32000
        //  6   2 000  55 - 16000
        //  7   1 000  55 -  8000
        //  8     500  55 -  8000
        //  9     250  55 -  8000
        // 10     125  55 -  8000
        void setBIOZModulationFrequencyByIndex(uint8_t index);
        void setBIOZmag(uint32_t current);
        // Set modulation frequency first
        //     nA
        // high current
        // 96 000
        // 80 000
        // 64 000
        // 48 000
        // 32 000
        // 16 000
        //  8 000 works with all frequencies
        // low current
        //  1 100
        //    880
        //    660
        //    440
        //    330
        //    220
        //    110
        //     55
        //      0
        // also sets common mode current appropriate feedback
        void setBIOZPhaseOffset(uint8_t selector);
        // Freq   FCGEN  Phase               Phase selector.
        // lower  other  0.. 11.25 ..168.75  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        //  80kHz 0b0001 0.. 22.5  ..157.50  0,1,2,3,4,5,6,7
        // 128kHz 0b0000 0.. 45.0  ..135     0,1,2,3
        void setBIOZfilter(uint8_t ahpf, unit8_t lpf, uint8_t hpf);
        // Analog high pass is
        //  [6,7]  0 Hz
        //  [0]   60 Hz
        //  [1]  150 Hz
        //  [2]  500 Hz * default
        //  [3] 1000 Hz
        //  [4] 2000 Hz 
        //  [5] 4000 Hz
        // Digital low pass
        //  [0] 0 
        //  [1] 4 * default
        //  [2] 8
        //  [3] 16Hz 
        // Digital high pass 
        //  [0] 0 Hz * default
        //  [1] 0.05 Hz 
        //  [2] 0.5 Hz
        void setBIOZmodulation(uint8_t mode);
        // 0 = Unchopped Sources with Low Pass Filter * default
        //  (higher noise, excellent 50/60Hz rejection, recommended for ECG, BioZ applications)
        // 1 = Chopped Sources without Low Pass Filter
        //  (low noise, no 50/60Hz rejection, recommended for BioZ applications with digital LPF, possibly battery powered ECG, BioZ applications)
        // 2 = Chopped Sources with Low Pass Filter
        //  (low noise, excellent 50/60Hz rejection)
        // 3 = Chopped Sources with Resistive CM
        //  (Not recommended to be used for drive currents >32µA, low noise, excellent 50/60Hz rejection, lower input impedance)
        void setBIOZImpedanceTest(bool enable, bool useHighResistance, bool enableModulation, uint8_t resistance, uint8_t rmodValue,  uint8_t modFreq);
        // high resistance
        //  0 00 =    27 kΩ
        //  1 01 =   108 kΩ
        //  2 10 =   487 kΩ
        //  3 11 =  1029 kΩ
        // low resistance
        //  0 000 =    5 kΩ
        //  1 001 =    2.5kΩ
        //  2 010 =    1.667kΩ
        //  3 011 =    1.25kΩ
        //  4 100 =    1kΩ
        //  5 101 =    0.833kΩ
        //  6 110 =    0.714kΩ
        //  7 111 =    0.625kΩ
        void setDefaultBIOZImpedanceTest(); // 1k, 3 mod at 1 Hz
        void setNoBIOZImpedanceTest();

        // Calibration
        void setCalibration(bool ECGenable, bool BIOZenable, bool unipolar, bool cal_vmag, uint8_t freq, uint8_t dutycycle) {
        void setDefaultECGCalibration();
        void setDefaultBIOZCalibration();
        void setDefaultNoCalibration();

        // Diagnostics
        void readAllRegisters();
        void dumpRegisters();
        void readInfo();
        void readStatusRegisters();
        void printStatus();
        void printEN_INT();
        void printMNGR_INT();
        void printMNGR_DYN();
        void printInfo();
        void printCNFG_CAL();
        void printCNFG_EMUX();
        void printCNFG_ECG();
        void printCNFG_BMUX();
        void printCNFG_BIOZ();
        void printCNFG_BIOZ_LC();
        void printCNFG_RTOR1();
        void printCNFG_RTOR2();

        bool spiCheck(void);  // check if SPI is working

        // Data Variables
        // --------------
        volatile bool ecg_available;   // ECG data available interrupt
        volatile bool bioz_available;  // BIOZ data available interrupt
        volatile bool rtor_available;  // R to R data available interrupt

        RingBuffer ECG_data;
        RingBuffer BIOZ_data;
        int        ecg_counter;           // number of ECG samples in buffer
        int        bioz_counter;          // number of BIOZ samples in buffer
        float      heart_rate;            // in beats per minute
        float      rr_interval;           // in milliseconds

        float impedance_magnitude[8];
        float impedance_phase[8]; 
        float impedance_frequency[8];

        volatile bool ecg_lead_off;              // ECG lead off detection interrupt
        volatile bool ecg_overflow_occured;      // ECG FIFO overflow
        volatile bool bioz_cgm_occured;          // BIOZ current generator monitor
        volatile bool bioz_undervoltage_occured; // BIOZ under voltage
        volatile bool bioz_overvoltage_occured;  // BIOZ over voltage
        volatile bool bioz_overflow_occured;     // BIOZ FIFO overlow
        volatile bool leads_on_detected;         // Ultra low power leads on
        volatile bool pll_unlocked_occured;      // PLL is not locked

        bool over_voltage_detected;
        bool under_voltage_detected;
        bool valid_data_detected;
        bool EOF_detected;

        // Status of the MAX30001 timing
        // and measurement settings
        // -----------------------------
        float    ECG_samplingRate;     // [sps]
        float    BIOZ_samplingRate;    // [sps] 
        float    RtoR_resolution;      // [ms] R to R resolution time
        float    CAL_resolution;       // [ms] calibration resolition time
        float    CAL_fcal;             // 
        float    fmstr;                // [Hz] main clock frequency
        float    tres;                 // [micro s] R to R peak detection threshold
        float    ECG_progression;      // [Hz] ECG progression rate
        float    ECG_hpf;              // [Hz] ECG high pass filter
        float    ECG_lpf;              // [Hz] ECG low pass filter
        int      ECG_gain;             // [V/V] ECG gain
        int      BIOZ_gain;            // [V/V] BIOZ gain
        int      BIOZ_cgmag;           // [nano A] BIOZ current generator magnitude 
        float    BIOZ_frequency;       // [HZ]
        float    BIOZ_phase;           // [degrees]
        uint32_t BIOZ cmres;           // [kOhm] common mode feedback resistance
        int32_t  V_ref = 1000;         // [mV]
        int32_t  V_AVDD = 18000;       // [mV]
        float    RCAL_freq = 0;        // Hz resistance modulation for test impedance

        // All Configuration and Status Registers

        max30001_status_reg_t       status;           // 0x01
        max30001_en_int_reg_t       en_int1;          // 0x02
        max30001_en_int_reg_t       en_int2;          // 0x03
        max30001_mngr_int_reg_t     mngr_int;         // 0x04
        max30001_mngr_dyn_reg_t     mngr_dyn;         // 0x05
        max30001_info_reg_t         info;             // 0x0f
        max30001_cnfg_gen_reg_t     cnfg_gen;         // 0x10
        max30001_cnfg_cal_reg_t     cnfg_cal;         // 0x12
        max30001_cnfg_emux_reg_t    cnfg_emux;        // 0x14
        max30001_cnfg_ecg_reg_t     cnfg_ecg;         // 0x15
        max30001_cnfg_bmux_reg_t    cnfg_bmux;        // 0x17
        max30001_cnfg_bioz_reg_t    cnfg_bioz;        // 0x18
        max30001_cnfg_bioz_lc_reg_t cnfg_bioz_lc;     // 0x1A
        max30001_cnfg_rtor1_reg_t   cnfg_rtor1;       // 0x1D
        max30001_cnfg_rtor2_reg_t   cnfg_rtor2;       // 0x1D
        
    private:

        void readECG_FIFO_BURST(bool reportRaw);
        void readBIOZ_FIFO_BURST(bool reportRaw);
        void readBIOZ_FIFO(bool_reportRaw);
        void readHRandRR(void);

        void synch(void);            // synchronize ECG and BIOZ measurements, needed after config changes, starts measurements
        void swReset(void);          // reset entire device
        void FIFOReset(void);        // reset FIFO after overflow

        // SPI service functions
        void     writeRegister(uint8_t address, uint32_t data);
        uint32_t readRegister24(uint8_t address);
        uint8_t  readRegisterByte(uint8_t address);

        // BIOZ functions
        int32_t closestCurrent(int32_t current); // given a desired current, find the closest valid current
        float impedancemodel(float theta, float magnitude, float phase); // calculate impedance for a given phase offset theta
        ImpedanceModel fitImpedance(const float* phases, const float* impedances, int num_points);

        // Global Variables
        int _csPin;                  // chip select for SPI

};

#endif // MAX30001_H

