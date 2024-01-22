/******************************************************************************************************/
// Analog to Digital
/******************************************************************************************************/
#include "ADC.h"
#include "Print.h"
#include "Config.h"

bool                 adc_avail = false;                          // do ww have ADC
bool                 buffer_full = false;
int16_t              buffer_in[MAX_ADC_BUFFER_SIZE];
int16_t              buffer_out[MAX_ADC_BUFFER_SIZE];
uint16_t             avg_samples_per_channel;                   // Depends on number of channels and sampling rate
int                  stateLOMinus;                              // Electrode connected ?
int                  stateLOPlus;                               // Electrode connected ?
int32_t              mysum[6];                                    // register to hold sum of up to 6 channels 

extern Settings mySettings;
extern AnalogAudioStream analog_in; 

bool initializeADC() {
  bool ok = false;
  adc_avail = false;
  D_printSerial("D:U:ADC:IN..");

    // Input Output Pins
  pinMode(SDN, OUTPUT);     // ECG enable
  pinMode(LO_MINUS, INPUT); // Electrode disconnected
  pinMode(LO_PLUS, INPUT);  // Electrode disconnected

  // Include logging to serial
  // Debug, Warning, Info, Error
  //AudioLogger::instance().begin(Serial, AudioLogger::Error);
  
  // Start ADC input
  auto adcConfig        = analog_in.defaultConfig(RX_MODE);
  adcConfig.sample_rate = mySettings.samplerate;
  adcConfig.channels    = mySettings.channels; 

  // ESP32 specific configuration for ESP32 version 3.0.0 and later from Espressif Systems:
  adcConfig.adc_bit_width = mySettings.bitwidth;
  adcConfig.adc_calibration_active = true; // use calibration fitting to correct for the response of the ADC
  adcConfig.is_auto_center_read = false; // do not subtract current estimated average from samples
  adcConfig.adc_attenuation = (uint8_t) mySettings.atten;
  if (mySettings.channels > 0) { adcConfig.adc_channels[0] = (adc_channel_t) mySettings.channel0; }
  if (mySettings.channels > 1) { adcConfig.adc_channels[1] = (adc_channel_t) mySettings.channel1; }
  if (mySettings.channels > 2) { adcConfig.adc_channels[2] = (adc_channel_t) mySettings.channel2; }
  if (mySettings.channels > 3) { adcConfig.adc_channels[3] = (adc_channel_t) mySettings.channel3; }
  if (mySettings.channels > 4) { adcConfig.adc_channels[4] = (adc_channel_t) mySettings.channel4; }
  if (mySettings.channels > 5) { adcConfig.adc_channels[5] = (adc_channel_t) mySettings.channel5; }
  analog_in.begin(adcConfig);

  // Enable ECG
  digitalWrite(SDN, HIGH);

  if (mySettings.debuglevel > 1) { R_printSerialln("ADC: initialized"); }
  adc_avail = true;
  ok = true;
  return ok;
}

bool updateADC() {

  buffer_full = false;

  // Read the values from the ADC buffer to local buffer

  // how many bytes shall we request?
  // This will discard fractions and make sure we will have sufficient samples to average and fill both channels equally
  size_t bytes_requested = ((ESPNOW_NUMDATABYTES/BYTES_PER_SAMPLE) / mySettings.channels) * (BYTES_PER_SAMPLE * mySettings.channels * mySettings.avglen);
  if (bytes_requested > ESPNOW_NUMDATABYTES*MAX_AVG_LEN) {
    snprintf(tmpStr, sizeof(tmpStr), "ADC: requesting more bytes %u than buffer can hold: %u", bytes_requested, ESPNOW_NUMDATABYTES*MAX_AVG_LEN); 
    if (mySettings.debuglevel > 0) { R_printSerialln(tmpStr); } 
    return false;
  }

  // read the bytes to the local buffer
  size_t bytes_read   = analog_in.readBytes((uint8_t*) buffer_in, bytes_requested ); // can only read byte stream, need to cast to destination type
  // I dont know if readBytes completes before it has received the requested number of bytes.
  // If that happens we need to figure out what to do.
  // For now we just state the issue.
  if (bytes_read < bytes_requested) {
    size_t samples_missing = (bytes_requested - bytes_read)/BYTES_PER_SAMPLE;
    snprintf(tmpStr, sizeof(tmpStr), "ADC: missign samples: %d", samples_missing); 
    if (mySettings.debuglevel > 0) { R_printSerialln(tmpStr); } 
  }

  // Average the samples over avglen, resulting in a reduction of avglen times less samples
  // The data buffer contains values in following order ch1,ch2,ch3,...,ch1,ch2,ch3,...
  avg_samples_per_channel  = (((bytes_read / BYTES_PER_SAMPLE) / mySettings.avglen) / mySettings.channels);
  int16_t *sample_in  = (int16_t*) buffer_in;                             // reset sample pointer (input)
  int16_t *sample_out = (int16_t*) buffer_out;                            // reset result pointer (output)
  for(uint16_t m=0; m<avg_samples_per_channel; m++){                      // number of samples after averaging per channel
    // initialize sum for each channel by reading first data point for each channel
    for(uint16_t n=0; n<mySettings.channels; n++){
      mysum[n] = *sample_in++;                                              // initialize sum
    }
    // add values over avg_len and channels
    for (uint16_t o=1; o<mySettings.avglen ; o++){                        // loop over number of average
      for(uint16_t n=0; n<mySettings.channels; n++){                      // loop over number of channels
        mysum[n] += *sample_in++;                                           // sum the values
      }
    }
    // compute average and store average in output buffer
    for(uint16_t n=0; n<mySettings.channels; n++){
      *sample_out++ = (int16_t) (mysum[n]/mySettings.avglen);               // average and increment output pointer
    }
  }
  buffer_full = true;
  return true;
}

