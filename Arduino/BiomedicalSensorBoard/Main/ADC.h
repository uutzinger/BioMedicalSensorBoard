/******************************************************************************************************/
// Analog to Digital Conversion
/******************************************************************************************************/
#ifndef ADC_H_
#define ADC_H_
#include <Arduino.h>
#include <AudioTools.h>

/******************************************************************************************************/
// ADC functions
/******************************************************************************************************/
bool initializeADC(void);
bool updateADC(void);

/******************************************************************************************************/
// ADC globals
/******************************************************************************************************/
extern bool     adc_avail;                                          // by default we have ADC
extern bool     buffer_full;
extern int16_t  buffer_in[MAX_ADC_BUFFER_SIZE];
extern int16_t  buffer_out[MAX_ADC_BUFFER_SIZE];
extern uint16_t avg_samples_per_channel;                             // Depends on number of channels and sampling rate
extern AnalogAudioStream analog_in;                                  // the audio device 

#endif