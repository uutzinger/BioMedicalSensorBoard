/*
Continuous Analog to Digital Conversion on ESP32
ADC is continuously read in the background
Values are stored in circular buffer
Buffer is printed to serial in main loop

This Requires Arduino IDE ESP 3.x, which is pre release/alpha currently
This also requires CircularBuffer library

Hardware limitations
ESP32 has two ADC. ADC2 is also used for WiFi so you can not use ADC2 and WiFi together.
ADC continuous mode uses I2S0 peripherals, therefor I2S0 will be in use and can not be used.

Urs Utzinger, 2023
*/

// Circular Buffer
//
#define CIRCULAR_BUFFER_INT_SAFE
#define BUFFER_SIZE 256
#include "CircularBuffer.h"
CircularBuffer<int, BUFFER_SIZE> readings_buffer;
CircularBuffer<unsigned long, BUFFER_SIZE> timings_buffer;

// ADC Configuration
//
// https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-adc.h
const int SAMPLING_FREQ = 22000; // 44.1kHz misses regularly samples
// you can average multiple readings on each A/D pin
#define CONVERSIONS_PER_PIN 1
// Set the resolution to 9-12 bits (default is 12 bits), data type is uint8_t
#define CONVERSION_RESOLUTION 12
// ADC_0db,: 0 mV ~ 950 mV
// ADC_2_5db: 0 mV ~ 1250 mV
// ADC_6db: 0 mV ~ 1750 mV
// ADC_11db: 0 mV ~ 3100 mV
// ADC_ATTENDB_MAX
#define CONVERSION_ATTENUATION ADC_11db
// uint8_t adc_pins[] = {1, 2, 3, 4}; //ADC1 common pins for ESP32S2/S3 + ESP32C3/C6 + ESP32H2
uint8_t adc_pins[] = {3};
uint8_t adc_pins_count = sizeof(adc_pins) / sizeof(uint8_t);

// ADC Interrupt Service Routine
//
// Result structure for ADC Continuous reading
adc_continuos_data_t * result = NULL;
volatile bool adc_read_failure = false;
// ISR Function that will be triggered when ADC conversion is done
void ARDUINO_ISR_ATTR adcComplete() {
  if (analogContinuousRead(&result, 0)) { // read value(s), timeout 0
    readings_buffer.push((int) result[0].avg_read_mvolts);
    timings_buffer.push((unsigned long) micros());
  } else {
    adc_read_failure=true;
  }
}

// Main Loop Variables
// 
unsigned long MICROS_PER_LOOP = ((unsigned long)BUFFER_SIZE * 1000000) / SAMPLING_FREQ; 
float cpu_usage = 0;

void setup() {
    // Initialize serial communication at 115200 bits per second:
    delay(3000);
    Serial.begin(2000000);
    Serial.println("Setting up:");

    // Optional for ESP32: Set the resolution to 9-12 bits (default is 12 bits)
    analogContinuousSetWidth(CONVERSION_RESOLUTION);

    // Optional: Set different attenaution (default is ADC_11db)
    analogContinuousSetAtten(ADC_11db);

    // Setup ADC Continuous with following input:
    // array of pins, count of the pins, how many conversions per pin in one cycle will happen, sampling frequency, callback function
    if (analogContinuous(adc_pins, adc_pins_count, CONVERSIONS_PER_PIN, SAMPLING_FREQ, &adcComplete)) {
      Serial.println("Continous ADC setup successful.");
    } else {
      Serial.println("Continous ADC setup unsuccessful.");
    }

    // Start ADC Continuous conversions
    if (analogContinuousStart()) {
      Serial.println("Continous ADC start sucessful.");
    } else {
      Serial.println("Continous ADC start unsucessful.");
    }

    delay(3000);
    Serial.println("Setup completed.");

}

void loop() {
  unsigned long current_time = micros();

  while (!timings_buffer.isEmpty()) {
    int meas = readings_buffer.shift();
    unsigned long t = timings_buffer.shift();
    Serial.printf("%d, %d, %.0f, %s\n", t, meas, cpu_usage, adc_read_failure ? "y" : "n");
  }
  adc_read_failure = false;

  // Delay until buffer is expected to be full
  long elapsedTime = (long) (micros() - current_time); // will be positive number
  int32_t delayTime = MICROS_PER_LOOP - elapsedTime; // might become negative, if program can not keep up
  cpu_usage = 100.0 * (float)elapsedTime / MICROS_PER_LOOP;
  if (delayTime > 0) {
    delayMicroseconds(static_cast<uint32_t>(delayTime));
  } 

}
