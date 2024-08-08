/**
   SavitzkyGolayFilter Basic Example
   Creates a generic sin wave and adds noise from random numbers
   Two filters are then run on it with a large windowsize and a small windowsize
   It is best to open Serial plotter for best results

   For non-Teensy users, elapsedMillis is required
   It can be downloaded from: https://github.com/pfeerick/elapsedMillis/archive/master.zip
   
   Author: James Deromedi
   Updates: Urs Utzinger
   License: MIT License
**/


#include <Arduino.h>
#include "SavitzkyGolayFilter.h"

#undef SMALL               // max window size is 25 if SMALL is defined, otherwise 63

// Window size, order, and derivative
#define WINDOW_SIZE_1 5    // must be odd 3...25 or 63 
#define WINDOW_SIZE_2 25   // must be odd 3...25 or 63
#define ORDER 2            // 1 = Linear, 2 = Quadratic, 3 = Cubic, 4 = Quartic, 5 = Quintic
#define DERIVATIVE 0       // 0 = Smooth, 1 = First Derivative, 2 = Second Derivative

float phase = 0.0;
float phase_inc = 0.02;
float phase_inc_factor = 1.5;
float twopi = 3.14159 * 2;

int32_t signalValue;
unsigned long lastTime;
unsigned long currentTime;

// Initialize the SavLayFilter
SavLayFilter smallFilter(WINDOW_SIZE_1, ORDER, DERIVATIVE);
SavLayFilter largeFilter(WINDOW_SIZE_2, ORDER, DERIVATIVE);

//==================================================================================================

void setup() {
  // Start the serial communication
  Serial.begin(500000);
  delay(2000);
  Serial.println("Starting SavGol");
  lastTime = micros();
}

//==================================================================================================

void loop() {

  currentTime = micros();

  if ((currentTime - lastTime) > 3000){
    lastTime = currentTime;
    // Generate a random value
    int32_t randomValue = random(0, 500);

    signalValue = int32_t(sin(phase) * 1000.0 + 2000.0) + randomValue;   // creates sin wave pattern with A = 1000 and shifted up by 2000
    phase = phase + phase_inc;                                     // propaget the sine wave
    if (phase >= twopi) {
      phase = 0;                            // resets the phase
      phase_inc = phase_inc * phase_inc_factor;
      if (phase_inc > twopi/5) {
        phase_inc_factor = 0.66;
        phase_inc = twopi/5.;
      } else if (phase_inc < 0.02) {
        phase_inc_factor = 1.5;
        phase_inc = 0.02;
      }
    }

    unsigned long tic = micros();
    // Update the filter with the random value
    int32_t filteredValue_small = smallFilter.update(signalValue);
    int32_t filteredValue_large = largeFilter.update(signalValue);
    unsigned long toc = micros();

    Serial.print(signalValue);             // Raw Value [Blue line]
    Serial.print(",");
    Serial.print(filteredValue_small);     // Smoothed value of smaller window [Orange line]
    Serial.print(",");
    Serial.print(filteredValue_large);     // Smoothed value of smaller window [Red line]
    Serial.print(",");
    Serial.println((toc-tic));             // Amount of time two filter update take

  } else {
    delay(0);
  }

}
