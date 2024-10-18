//////////////////////////////////////////////////////////////////////////////////////////
// AFE49XX Heart Rate Algorithm 
/////////////////////////////////////////////////////////////////////////////////////////
/*
  Preprocessing Options
    Noise Filtering: Apply filters to remove noise from the PPG signals. Commonly used filters include:
        Low-pass filter to remove high-frequency noise.
        High-pass filter to eliminate baseline wander.
        Bandpass filter to focus on the frequency range of interest (0.5 Hz to 5 Hz for heart rate).
    Normalization: Normalize the signal to a common scale to make it easier to process.
    Smoothing: Apply a moving average filter or other smoothing techniques to reduce signal variability.
  Peak Detection
    Thresholding: Set a dynamic threshold to detect peaks above a certain value.
    Differentiation: Use the first or second derivative of the signal to find zero-crossings that indicate peaks.
    Moving Window Integration (MWI): Sum the signal values within a moving window to highlight peaks.
  HR calculation
    Measure the time intervals between consecutive peaks (R-R intervals).
    Calculate the heart rate in beats per minute (BPM) using the formula:
    Heart Rate (BPM)= 60 * Average R-R Interval (seconds)
  HR Post-Processing
    Apply algorithms to smooth the heart rate values over time and reduce variability.
    Implement error-checking and outlier removal to ensure reliable readings.
  R-R variability
    Calculate R-R variability, its a stress indicator
*/

/******************************************************************************************************/
// Signal Processing Plan
/******************************************************************************************************/

// Ring Buffer
//   Add data to ring buffer
//     Update SG filter with the new data 
//     Add filtered data to buffer
// Update Moving Window
//   Find Max
//   Find Min
//   Record peak if Max is not on Window Edge
//     update heart rate
//     update variability
//     Add window average to breathing rate buffer
// Update breathing buffer
//   Find Max
//   Find Min

#include "hr_algo.h"
