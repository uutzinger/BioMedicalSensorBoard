/******************************************************************************************************/
// Analog to Digital Conversion
/******************************************************************************************************/
#include "CommonDefs.h"
#include "ADC.h"
#include "ESPNow.h"
#include "Print.h"
#include "Config.h"

/**************************************************************************************/
// Global Variables
/**************************************************************************************/

bool                 adc_avail;                                           // ADC is working
bool                 buffer_full = false;                                 // buffer is full
int16_t              buffer_in[MAX_ADC_BUFFER_SIZE];                      // local copy of ADC data
int16_t              buffer_out[MAX_ADC_BUFFER_SIZE];                     // for averaging and processing
uint16_t             buffer_in_index;                                     // when ever we receive ADC data we will start inserting it at this location
uint16_t             bytes_needed;                                        // until we are ready for processing
uint16_t             samples_needed;                                      // same as bytes needed but in samples
uint16_t             avg_samples_per_channel;                             // Depends on number of channels and sampling rate

// On board analog to digital converter
AnalogAudioStream    analog_in; 

bool initializeADC() {

  bool ok;
  LOG_Dln("D:I:ADC..");

  // Include logging to serial
  // Debug, Warning, Info, Error
  //AudioLogger::instance().begin(Serial, AudioLogger::Error);
  
  // Start ADC input
  auto adcConfig        = analog_in.defaultConfig(RX_MODE);
  adcConfig.sample_rate = mySettings.samplerate;
  adcConfig.channels    = mySettings.channels; 

  // ESP32 specific configuration for ESP32 version 3.0.0 and later from Espressif Systems:
  adcConfig.adc_bit_width = mySettings.bitwidth;
  adcConfig.adc_calibration_active = true;                                // use calibration fitting to correct for the response of the ADC
  adcConfig.is_auto_center_read = false;                                  // do not subtract current estimated average from samples
  adcConfig.adc_attenuation = (uint8_t) mySettings.atten;
  if (mySettings.channels > 0) { adcConfig.adc_channels[0] = (adc_channel_t) mySettings.channel0; }
  if (mySettings.channels > 1) { adcConfig.adc_channels[1] = (adc_channel_t) mySettings.channel1; }
  if (mySettings.channels > 2) { adcConfig.adc_channels[2] = (adc_channel_t) mySettings.channel2; }
  if (mySettings.channels > 3) { adcConfig.adc_channels[3] = (adc_channel_t) mySettings.channel3; }
  if (mySettings.channels > 4) { adcConfig.adc_channels[4] = (adc_channel_t) mySettings.channel4; }
  if (mySettings.channels > 5) { adcConfig.adc_channels[5] = (adc_channel_t) mySettings.channel5; }
  // does this return a value?
  ok = analog_in.begin(adcConfig);
  buffer_in_index = 0; // initialize index to start of buffer

  // This will not change during runtime
  bytes_needed = ((ESPNOW_NUMDATABYTES/BYTES_PER_SAMPLE) / mySettings.channels) * (BYTES_PER_SAMPLE * mySettings.channels * mySettings.avglen);
  samples_needed = bytes_needed/BYTES_PER_SAMPLE;

  // Make sure we dont have some weired number of bytes
  if (bytes_needed > MAX_ADC_BUFFER_SIZE) {
    snprintf(tmpStr, sizeof(tmpStr), "ADC: requesting more bytes %u than buffer can hold: %u", bytes_needed, MAX_ADC_BUFFER_SIZE); 
    if (mySettings.debuglevel > 0) { LOG_Iln(tmpStr); } 
    ok = false;
  }

  if (mySettings.debuglevel > 1) { LOG_Iln("ADC: initialized"); }

  return ok;
}

bool updateADC() {

  bool ok;
  int16_t *sample_in;                                                     // pointer to input data
  int16_t *sample_out;                                                    // pointer to output data
  int32_t mysum[6];                                                       // register to hold sum of up to 6 channels 

  LOG_Dln("D:U:ADC:..");
  
  // If the buffer has not yet been processed by by ECG, Temp, or Sound, etc.
  // buffer_full remains true and we will not read new data from the ADC

  if (!buffer_full) {  // we have a fresh buffer

    ok = true;

    // how many bytes do we need to request?
    // buffer can be partially filled, so we need to request the remaining bytes
    size_t bytes_requested = bytes_needed - buffer_in_index*BYTES_PER_SAMPLE; // number of bytes needed to fill buffer

    // read the bytes to the local buffer
    size_t bytes_read = analog_in.readBytes((uint8_t*) buffer_in[buffer_in_index], bytes_requested ); // can only read byte stream, need to cast to destination type

    if (bytes_read % BYTES_PER_SAMPLE > 0) {
      if (mySettings.debuglevel > 0) { LOG_Iln("ADC Error: We received a number of bytes not devisable by bytes per sample"); }
      ok = false;
    }

    // check if did not got all the bytes we requested
    if (bytes_read < bytes_requested) {
      size_t samples_missing = (bytes_requested - bytes_read)/BYTES_PER_SAMPLE;
      size_t samples_requested = bytes_requested/BYTES_PER_SAMPLE;
      snprintf(tmpStr, sizeof(tmpStr), "ADC: missing samples: %d, requested: %d", samples_missing, samples_requested); 
      if (mySettings.debuglevel > 0) { LOG_Iln(tmpStr); } 
    }

    // increment buffer index by number of samples read
    buffer_in_index += (bytes_read/BYTES_PER_SAMPLE);                         // increment buffer index

    // check if buffer is not yet full
    if (buffer_in_index < samples_needed) {
      buffer_full = false;                                                  // if buffer is not yet full
    } 

    else {                                                                   // buffer has been filled
      buffer_in_index = 0;                                                   // reset buffer index
      buffer_full = true;                                                    // set buffer full flag
      // Average the samples over avglen, resulting in a reduction of avglen times less samples
      // The data buffer contains values in following order 
      //   ch1,ch2,ch3,...,ch1,ch2,ch3,...
      avg_samples_per_channel  = ( (samples_needed / mySettings.avglen) / mySettings.channels );
      sample_in  = (int16_t*) buffer_in;                                      // reset sample pointer (input)
      sample_out = (int16_t*) buffer_out;                                     // reset result pointer (output)
      for(uint16_t m=0; m<avg_samples_per_channel; m++){                      // number of samples after averaging per channel
        // initialize sum for each channel by reading first data point for each channel
        for(uint16_t n=0; n<mySettings.channels; n++){
          mysum[n] = *sample_in++;                                            // initialize sum
        }
        // add values over avg_len and channels
        for (uint16_t o=1; o<mySettings.avglen ; o++){                        // loop over number of average
          for(uint16_t n=0; n<mySettings.channels; n++){                      // loop over number of channels
            mysum[n] += *sample_in++;                                         // sum the values
          }
        }
        // compute average and store average in output buffer
        for(uint16_t n=0; n<mySettings.channels; n++){
          *sample_out++ = (int16_t) (mysum[n]/mySettings.avglen);             // average and increment output pointer
        }
      } // end of averaging
    } // end of buffer has been filled
  } // end buffer is not full

  else {
    if (mySettings.debuglevel > 0) { LOG_Iln("ADC: buffer full"); }
    ok = true;
  } // end buffer is full

  return ok;
}
