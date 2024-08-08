/**
 * For the MediBrick 3 Channel Thermistor Board
 * @author Urs Utzinger
 * @copyright GPLv3 
 **/

#include "Arduino.h"
#include "AudioTools.h"

// | attenuation     | range   | accurate range |
// | ------------    | --------| -------------- |
// | ADC_ATTEN_DB_0  | 0..1.1V | 100-950mV      |
// | ADC_ATTEN_DB_2_5| 0..1.5V | 100-1250mV     |
// | ADC_ATTEN_DB_6  | 0..2.2V | 150-1750mV     |
// | ADC_ATTEN_DB_12 | 0..3.9V | 150-2450mV     | * depends on resistors

#define M_NUM_CHANNELS   3                    // # of processed channels, do not change
#define M_NUM_ADC_CHANNELS (M_NUM_CHANNELS*2) // # of measured channels
#define M_ADC_BIT_WIDTH 12                    // the ADC bit width, values will be stored in 16 bit integers
#define M_ADC_ATTENUATION ADC_ATTEN_DB_11     // ADC attenuation, see above
#define M_BIN_SIZE     168                    // large averaging should reduce noise max is 168
#define M_AVERAGE     true                    // Enable averaging, otherwise we sum
#define M_BIT_DEPTH     16                    // default bit width of the ADC data, do not change
#define M_BYTES_PER_SAMPLE sizeof(int16_t)

// Reading order of the channels on the board

#define M_ADC0 ADC_CHANNEL_5 //    33
#define M_ADC1 ADC_CHANNEL_4 //    32

#define M_ADC2 ADC_CHANNEL_6 // A2 34
#define M_ADC3 ADC_CHANNEL_3 // A3 39

#define M_ADC4 ADC_CHANNEL_0 // A4 36
#define M_ADC5 ADC_CHANNEL_7 // A5 35

// Lets figure out some reasonable buffer settings
#define M_BUFFER_FACTOR      (2048/ (M_NUM_ADC_CHANNELS*M_BIN_SIZE*M_BYTES_PER_SAMPLE)) //
#define M_ADC_SAMPLES        (M_NUM_ADC_CHANNELS*M_BIN_SIZE*M_BUFFER_FACTOR) // Total number of samples (not bytes)
#define M_BUFFER_SIZE        (M_ADC_SAMPLES*M_BYTES_PER_SAMPLE) // Total number of bytes, needs to be divisible by 4 
#define M_CSVOUT_BUFFER_SIZE (M_BUFFER_SIZE/M_BIN_SIZE) // There will be factor times less data after binnibng

// Sampling rate
#define M_CSVOUT_SAMPLE_RATE 50                               // samples per channel per second
#define M_SAMPLE_RATE        M_CSVOUT_SAMPLE_RATE*M_BIN_SIZE  // number of samples per channel before decimating
#define M_ADC_SAMPLE_RATE    (M_SAMPLE_RATE*M_NUM_ADC_CHANNELS ) // Should be more than 20,000 for ESP32
// BAUD rate required: 
//   CSVOUT_SAMPLE_RATE * (NUM_CHANNELS * approx 6 chars + 2 EOL) * 10

// Choose if we want Voltage or Temperature readings
#define PROCESS
//#undef PROCESS

#ifdef PROCESS
  // These are the values measured on the board before it got assembled
  // If you did not measure them, you can use R=10k and measure V_IN
  // You still should measure TP2-TP3 resistance for R3

  #define V_IN   3320

  #define CH1_R1  9960
  #define CH1_R2  9970
  #define CH1_R3  9960
  //#define CH1_R3  9940

  #define CH2_R1  9950
  #define CH2_R2  9990
  #define CH2_R3  9970
  // #define CH2_R3  9900

  #define CH3_R1 10000
  #define CH3_R2  9980
  #define CH3_R3 10030
  // #define CH3_R3 10000

  // From thermistor manufacturer datasheet
  // See python program calculatuing the coefficients from manufacturerer provide temperature, resistance curve
  #define A_COEFF 0.001111285538
  #define B_COEFF 0.0002371215953
  #define C_COEFF 0.00000007520676806

#endif

AudioInfo                     info_serial(M_CSVOUT_SAMPLE_RATE, M_NUM_CHANNELS, M_BIT_DEPTH);
AnalogAudioStream             analog_in;                                              // On board analog to digital converter
ChannelBinDiff                bindiffer;                                              // Binning each channel by average length, setup see below
ConverterStream<int16_t>      converted_stream(analog_in, bindiffer);                 // Pipe the sound to the binnning and differ converter

void setup() {
  
  // Serial Interface ------------
  Serial.begin(115200);
  while(!Serial); // Wait for Serial to be ready

  // Logging level ---------------
  AudioLogger::instance().begin(Serial, AudioLogger::Warning); // Debug, Warning, Info, Error

  // ADC ------------------------
  // Calibrated readings from ESP32 ADC are in units of milli Volts
  LOGI("Configuring ADC");
  auto adcConfig = analog_in.defaultConfig(RX_MODE);
  adcConfig.sample_rate = M_SAMPLE_RATE;                // sample rate per channel?
  adcConfig.channels = M_NUM_ADC_CHANNELS;              // 6 channels on this board
  adcConfig.adc_bit_width = M_ADC_BIT_WIDTH;            // Supports only 12
  adcConfig.buffer_size = M_ADC_SAMPLES;                // in total number of samples
  adcConfig.adc_calibration_active = true;              // use and apply the calibration settings of ADC stored in ESP32
  adcConfig.is_auto_center_read = false;                // do not engage auto-centering of signal (would subtract average of signal)
  adcConfig.adc_attenuation = M_ADC_ATTENUATION;        // set analog input range
  adcConfig.adc_channels[0] = M_ADC0;                   // ensure configuration for each channel
  adcConfig.adc_channels[1] = M_ADC1;
  adcConfig.adc_channels[2] = M_ADC2; 
  adcConfig.adc_channels[3] = M_ADC3; 
  adcConfig.adc_channels[4] = M_ADC4; 
  adcConfig.adc_channels[5] = M_ADC5; 

  if (analog_in.begin(adcConfig)) { 
    LOGI("ADC configured"); 
  } else {
    LOGE("ADC configuration failed");
    while(1) {delay(50);} 
  }

  // Converters -----------------
  LOGI("Configuring Converter");
  bindiffer.setChannels(M_NUM_ADC_CHANNELS);
  bindiffer.setBits(M_BIT_DEPTH);
  bindiffer.setBinSize(M_BIN_SIZE);
  bindiffer.setAverage(M_AVERAGE);
  LOGI("Converter initialized");

}

int16_t process_buffer[M_ADC_SAMPLES];
char out_text_buffer[64];

void loop() {

  // Read the values from the conversion stream to the temperature processing buffer
  size_t bytes_read   = converted_stream.readBytes((uint8_t*) process_buffer, M_BUFFER_SIZE);
  size_t samples_read = bytes_read / M_BYTES_PER_SAMPLE; 
  int16_t* sample_buffer = (int16_t*) process_buffer;

  if (samples_read != M_NUM_CHANNELS*M_BUFFER_FACTOR) {
     LOGE("Bytes read %d (want %d), Samples read %d (want %d)", bytes_read, M_BYTES_PER_SAMPLE * M_NUM_CHANNELS * M_BUFFER_FACTOR, samples_read, M_NUM_CHANNELS*M_BUFFER_FACTOR)
  }


#ifdef PROCESS

  // Print temperature converted numbers
  for (int i=0; i < M_BUFFER_FACTOR; i++) {
    int16_t T_R1 = calcres(sample_buffer[0], V_IN, CH1_R1, CH1_R2, CH1_R3); // resistance
    int16_t T_R2 = calcres(sample_buffer[1], V_IN, CH2_R1, CH2_R2, CH2_R3); 
    int16_t T_R3 = calcres(sample_buffer[2], V_IN, CH3_R1, CH3_R2, CH3_R3);
    int16_t T1 = calctemp(T_R1, A_COEFF, B_COEFF, C_COEFF); // Temperature 
    int16_t T2 = calctemp(T_R2, A_COEFF, B_COEFF, C_COEFF); 
    int16_t T3 = calctemp(T_R3, A_COEFF, B_COEFF, C_COEFF);
    // Calibration is valid between 0 and 50C
    float T1_f; 
    float T2_f;
    float T3_f;
    if (T1<0 || T1>5000) { T1_f = NAN; } else { T1_f = float(T1)/100.;}
    if (T2<0 || T2>5000) { T2_f = NAN; } else { T2_f = float(T2)/100.;}
    if (T3<0 || T3>5000) { T3_f = NAN; } else { T3_f = float(T3)/100.;}
    //snprintf(out_text_buffer, sizeof(out_text_buffer), "%5d, %5d, %5d", T_R1, T_R2, T_R3);
    snprintf(out_text_buffer, sizeof(out_text_buffer), "%5.1f, %5.1f, %5.1f", T1_f, T2_f, T3_f);
    Serial.println(out_text_buffer);
    sample_buffer += M_NUM_CHANNELS;
  }

#else

  // Print raw voltage differences
  for (int i = 0; i < M_BUFFER_FACTOR; i++) {
    snprintf(out_text_buffer, sizeof(out_text_buffer), "%d, %d, %d", sample_buffer[0], sample_buffer[1], sample_buffer[2]);
    Serial.println(out_text_buffer);
    sample_buffer += M_NUM_CHANNELS;
  }

#endif

} 

// Calculating the actual resistance of the thermistor based on the measurements
// [Wikipedia Wheatstone bridge formula](https://en.wikipedia.org/wiki/Wheatstone_bridge):
// $ R_{\text{Thermistor}} = \frac{R_3 \left( V_{\text{in}} R_2 - V_{\text{diff}} (R_1 + R_2) \right)}{V_{\text{in}} R_1 + V_{\text{diff}} (R_1 + R_2)} $
int32_t calcres(
  int16_t Vdiff, // Voltage difference between two measurement points in Wheatstone bridge in mV
  int32_t Vin,   // Voltage from power supply for Wheatstone bridge input in mV
  int32_t R1,    // Resistor 1 in Wheatstone bridge in Ohms
  int32_t R2,    // Resistor 2 in Wheatstone bridge in Ohms
  int32_t R3     // Resistor 3 in Wheatstone bridge in Ohms
) {
  // Implemented with integer math: We need to use uint64_t because resistors are 10,000 Ohm and Vin is 3,300 mV resulting in numbers larger than int32_t (32,000)
  int64_t numerator = int64_t(R3) * ( (int64_t(Vin) * R2) - (int64_t(Vdiff) * (R1 + R2)) );
  int64_t denominator = (int64_t(Vin) * R1) + (int64_t(Vdiff) * (R1 + R2));
  int32_t R4 = int32_t(numerator / denominator);
  return int32_t(R4);
}

// Calculate the temperature based on measured component values and manufacturer data: https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation
// This is optimized to use minimal number of floating point operations and to avoid integer overflow
int16_t calctemp(
  int32_t R,     // Resistance in Ohm
  float A,       // Steinhart-Hart Coefficient A
  float B,       // Steinhart-Hart Coefficient B
  float C       // Steinhart-Hart Coefficient C
) {
  // Steinhart-Hart Equation
  float lnR = log(float(R));
  //LOGD("R4: %d, %f", Rr, lnR);
  float Tinv = (A + (B * lnR) + (C * (lnR * lnR * lnR)));
  //LOGD("Tinv %f", Tinv );
  float T = 1.0 / Tinv;
  T -= 273.15; // Convert Kelvin to Centigrade
  //LOGD("T %f", T);
  //LOGD("T %d", int16_t(T*100.0));  
  return int16_t(T * 100.0); // Convert temperature to centidegrees Celsius
}
