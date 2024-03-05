/**
 * @file base-adc-customprocess-serial.ino
 * @brief Pseudo differential and average from two analog channels
 * Reads two analog channels
 * Creates differential between the two channels
 * Averages multiple samples and outputs to serial at lower sampling rate
 * This can be used to read a full bridge circuit such as https://en.wikipedia.org/wiki/Wheatstone_bridge
 * @author Urs Utzinger
 * @copyright GPLv3
 **/

#include "Arduino.h"
#include "AudioTools.h"

// On board analog to digital converter
AnalogAudioStream analog_in; 

// Serial terminal output
CsvStream<int16_t> serial_out(Serial);

#define SAMPLE_RATE 40000
#define BUFFER_SIZE 256
#define AVG_LEN 64
#define BYTES_PER_SAMPLE sizeof(int16_t)
int16_t buffer[BUFFER_SIZE];

void setup() {
  
  delay(3000); // wait for serial to become available

  // Serial Interface
  Serial.begin(512000);

  // Include logging to serial
  AudioLogger::instance().begin(Serial, AudioLogger::Error); // Debug, Warning, Info, Error
  
  // Start ADC input
  Serial.println("Starting ADC...");
  auto adcConfig = analog_in.defaultConfig(RX_MODE);
  adcConfig.sample_rate = SAMPLE_RATE;

  // For ESP32 by Espressif Systems version 3.0.0 and later 
  // see examples/README_ESP32.md
  // adcConfig.sample_rate = SAMPLE_RATE;
  // adcConfig.adc_bit_width = 12;
  // adcConfig.adc_calibration_active = true;
  // adcConfig.is_auto_center_read = false;
  // adcConfig.adc_attenuation = ADC_ATTEN_DB_11; 
  // adcConfig.channels = 2;
  // adcConfig.adc_channels[0] = ADC_CHANNEL_4; 
  // adcConfig.adc_channels[1] = ADC_CHANNEL_5;

  analog_in.begin(adcConfig);

  // Start Serial Output CSV
  Serial.println("Starting Serial Out...");
  auto csvConfig = serial_out.defaultConfig();
  csvConfig.sample_rate = SAMPLE_RATE/AVG_LEN;
  csvConfig.channels = 1;
  csvConfig.bits_per_sample = 16;
  serial_out.begin(csvConfig);
}

void loop() {

  // Read the values from the ADC buffer to local buffer
  size_t bytes_read   = analog_in.readBytes((uint8_t*) buffer, BUFFER_SIZE * BYTES_PER_SAMPLE); // read byte stream and cast to destination type

  size_t samples_read = bytes_read/BYTES_PER_SAMPLE; // adjust by bytes per sample
  size_t diff_samples = samples_read / 2;            // after computing difference we have 2 times less samples
  size_t avg_samples  = diff_samples / AVG_LEN;      // after averaging over AVG_LEN we have AVG_LEN times less samples

  // Calculate difference between two channels
  int16_t *sample = (int16_t*) buffer; // pointer to start of buffer
  for(uint16_t i=0; i<diff_samples; i++){
    buffer[i] = *sample++ - *sample++; // difference and increment pointer
  }

  // Average the samples over AVG_LEN
  sample = (int16_t*) buffer; // point to start of buffer
  for(uint16_t j=0; j<avg_samples; j++){
    int32_t sum = *sample++; // initialize sum
    for (uint16_t i=1; i<AVG_LEN; i++){
      sum += *sample++; // sum the samples
    }
    buffer[j] = (int16_t) (sum/AVG_LEN); // compute average and store in buffer
  }

  // stream to output
  serial_out.write((uint8_t*)buffer, avg_samples*BYTES_PER_SAMPLE); // stream to output as byte type
  
}
