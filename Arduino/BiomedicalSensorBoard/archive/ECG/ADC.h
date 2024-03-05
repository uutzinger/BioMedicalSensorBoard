#ifndef ADC_H_
#define ADC_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp32/rom/gpio.h"
#include "esp_adc/adc_continuous.h"
#include "soc/adc_channel.h"
#include <math.h>
#include "AudioTools.h"
#include "ESPNow.h"

// The maximum buffer size we might need is the number of samples that fit into the ESPNow payload times the maximum averaging legnth
// The number of samples we will read form ADC will calculated in ESP
#define BYTES_PER_SAMPLE sizeof(int16_t)  // We will work with int16 data when signals are measured
#define MAX_AVG_LEN                   32  // We want to average over up to MAX_AVG_LEN values 
#define MAX_ADC_BUFFER_SIZE ESPNOW_NUMDATABYTES/BYTES_PER_SAMPLE*MAX_AVG_LEN

bool initializeADC(void);
bool updateADC(void);

#endif