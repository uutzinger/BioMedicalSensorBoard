/**
 * @file read-esp32-multi-channel-decimate-custom-out.ino
 * @author Urs Utzinger
 * @copyright GPLv3
 */
#include "Arduino.h"
#include "AudioTools.h"

// Decimator as converter
#define M_FACTOR             96

#define M_NUM_CHANNELS       6
#define M_ADC_SAMPLES        (M_NUM_CHANNELS*M_FACTOR*4) // number of samples over all channels
#define M_ADC_BUFFER_SIZE    (M_ADC_SAMPLES*sizeof(int16_t)) // number of bytes over all channels
#define M_CSVOUT_BUFFER_SIZE (M_NUM_CHANNELS*4)*sizeof(int16_t) // number of bytes at output of converter going into CsvOutput

#define M_ADC_BIT_WIDTH      12
#define M_BIT_WIDTH          16

#define M_SERIAL_SAMPLE_RATE 120 // samples per channel
#define M_ADC_SAMPLE_RATE    M_SERIAL_SAMPLE_RATE*M_FACTOR // samples per channel 23040

#define M_ADC0 ADC_CHANNEL_5 // 33
#define M_ADC1 ADC_CHANNEL_4 // 32

#define M_ADC2 ADC_CHANNEL_6 // A2
#define M_ADC3 ADC_CHANNEL_3 // A3

#define M_ADC4 ADC_CHANNEL_0 // A4
#define M_ADC5 ADC_CHANNEL_7 // A5

AudioInfo                    info_serial(M_SERIAL_SAMPLE_RATE, M_NUM_CHANNELS, M_BIT_WIDTH);

AnalogAudioStream            analog_in;
Decimate                     converter;
ConverterStream<int16_t>     converted_stream(analog_in, converter);

void setup() {
  
  Serial.begin(115200);
  while(!Serial); // Wait for Serial to be ready
  AudioLogger::instance().begin(Serial, AudioLogger::Debug); // Debug, Warning, Info, Error

  auto adcConfig = analog_in.defaultConfig(RX_MODE);
  adcConfig.sample_rate = M_ADC_SAMPLE_RATE; // sample rate per channel
  adcConfig.channels = M_NUM_CHANNELS;
  adcConfig.adc_bit_width = M_ADC_BIT_WIDTH;
  adcConfig.buffer_size = M_ADC_SAMPLES; // in total number of samples over all channels
  adcConfig.adc_calibration_active = true;
  adcConfig.is_auto_center_read = false;  
  adcConfig.adc_attenuation = ADC_ATTEN_DB_11;
  adcConfig.adc_channels[0] = M_ADC0;                   // ensure configuration for each channel
  adcConfig.adc_channels[1] = M_ADC1;
  adcConfig.adc_channels[2] = M_ADC2; 
  adcConfig.adc_channels[3] = M_ADC3; 
  adcConfig.adc_channels[4] = M_ADC4; 
  adcConfig.adc_channels[5] = M_ADC5; 

  analog_in.begin(adcConfig);

  converter.setChannels(M_NUM_CHANNELS);
  converter.setBits(M_BIT_WIDTH);
  converter.setFactor(M_FACTOR);

}

// Custom Process Buffer to read values from converter stream and process them in main loop
int16_t converter_buffer[M_ADC_SAMPLES]; 
// I would expect it to be FACTOR less because converter decimates, but that does not seem to be correct

void loop() {
  size_t bytes_read   = converted_stream.readBytes((uint8_t*) converter_buffer, M_ADC_BUFFER_SIZE);
  size_t samples_read = bytes_read / sizeof(int16_t); 
  int16_t* sample_buffer = (int16_t*) converter_buffer;

  // I will replace this with custom processing
  for (int i = 0; i < samples_read/M_NUM_CHANNELS; i++) {
    for (int j = 0; j<M_NUM_CHANNELS; j++) {
      Serial.print(sample_buffer[j]);
      Serial.print(" ");
    }
    sample_buffer += M_NUM_CHANNELS;
    Serial.println();
  }
}

