#include <Arduino.h>
#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali_scheme.h"

// ADC Channels (GPIO pins) using the correct gpio_num_t enumeration
#define ADC1_CHANNEL_0_PIN GPIO_NUM_36 // ADC1_CHANNEL_0
#define ADC1_CHANNEL_3_PIN GPIO_NUM_39 // ADC1_CHANNEL_3
#define ADC1_CHANNEL_6_PIN GPIO_NUM_34 // ADC1_CHANNEL_6
#define ADC1_CHANNEL_7_PIN GPIO_NUM_35 // ADC1_CHANNEL_7

// Number of channels
#define NUM_CHANNELS 4
#define SKIP 64

// ADC calibration parameters
esp_adc_cal_characteristics_t *adc_chars;
const adc_channel_t channels[NUM_CHANNELS] = {ADC_CHANNEL_0, ADC_CHANNEL_3, ADC_CHANNEL_6, ADC_CHANNEL_7};
const gpio_num_t channel_pins[NUM_CHANNELS] = {ADC1_CHANNEL_0_PIN, ADC1_CHANNEL_3_PIN, ADC1_CHANNEL_6_PIN, ADC1_CHANNEL_7_PIN};
const adc_bits_width_t width = ADC_WIDTH_BIT_12;
const adc_atten_t atten = ADC_ATTEN_DB_11;

static const char *TAG = "ADC_CONTINUOUS";

// ADC continuous driver handle
adc_continuous_handle_t adc_handle;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize ADC
  adc1_config_width(width);

  for (int i = 0; i < NUM_CHANNELS; i++) {
    adc1_config_channel_atten((adc1_channel_t)channels[i], atten);
  }

  // Initialize ADC calibration
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT_1, atten, width, 3300, adc_chars);

  // Configure ADC continuous mode
  adc_continuous_handle_cfg_t adc_continuous_handle_cfg = {
      .max_store_buf_size = 4096,
      .conv_frame_size = 2048,
  };

  // Initialize ADC continuous driver
  esp_err_t err = adc_continuous_new_handle(&adc_continuous_handle_cfg, &adc_handle);
  if (err != ESP_OK) {
    Serial.printf("adc_continuous_new_handle unsuccessful with error: %d\n", err);
  } else {
    Serial.println("adc_continuous_new_handle successful");
  }

  // Allocate memory for ADC pattern
  adc_digi_pattern_config_t adc_pattern[NUM_CHANNELS];

  for (int i = 0; i < NUM_CHANNELS; i++) {
    adc_pattern[i].atten = atten;
    adc_pattern[i].bit_width = width;
    adc_pattern[i].channel = channels[i];
    adc_pattern[i].unit = ADC_UNIT_1;
  }

  // Configure ADC continuous driver
  adc_continuous_config_t cont_config = {
      .pattern_num = NUM_CHANNELS,
      .adc_pattern = adc_pattern,
      .sample_freq_hz = 20000,
      .conv_mode = ADC_CONV_SINGLE_UNIT_1,
      .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1
  };

  // Configure ADC continuous driver
  err = adc_continuous_config(adc_handle, &cont_config);
  if (err != ESP_OK) {
    Serial.printf("adc_continuous_config unsuccessful with error: %d\n", err);
    return;
  } else {
    Serial.println("adc_continuous_config successful");
  }

  // Start ADC continuous mode
  err = adc_continuous_start(adc_handle);
  if (err != ESP_OK) {
    Serial.printf("adc_continuous_start unsuccessful with error: %d\n", err);
    return;
  } else {
    Serial.println("adc_continuous_start successful");
  }
}

void loop() {
  static uint16_t throttle = 0; // Make throttle static to retain its value across loop iterations
  uint8_t result[512];
  uint32_t ret_num = 0;

  // Read data from ADC
  esp_err_t ret = adc_continuous_read(adc_handle, result, sizeof(result), &ret_num, 1000 / portTICK_PERIOD_MS);
  if (ret == ESP_OK) {
    for (int i = 0; i < ret_num; i += sizeof(adc_digi_output_data_t) * NUM_CHANNELS) {
      for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i + ch * sizeof(adc_digi_output_data_t)];
        uint32_t voltage = esp_adc_cal_raw_to_voltage(p->type1.data, adc_chars);
        if (throttle >= SKIP) {
          Serial.print("Channel ");
          Serial.print(p->type1.channel);
          Serial.print(": ");
          Serial.print(voltage);
          if (ch < NUM_CHANNELS - 1) {
            Serial.print(", ");
          }
        }
      }
      if (throttle >= SKIP) {
        Serial.println();
      }
    }
    if (throttle >= SKIP) {
      throttle = 0;
    } else {
      throttle++;
    }
  } else if (ret == ESP_ERR_TIMEOUT) {
    Serial.println("ADC Read Timeout");
  }
}