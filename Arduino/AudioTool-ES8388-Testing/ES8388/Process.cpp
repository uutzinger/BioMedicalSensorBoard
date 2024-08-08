/******************************************************************************************************/
// Sound Support Functions
/******************************************************************************************************/
#include "Process.h"
#include "myPrint.h"
#include "Commons.h"

Buffer initBuffer(uint16_t numBytes) {
// Function to initialize the buffer structure and allocate memory for the buffer
//  provide number of samples (not bytes) as buffer size
    Buffer buf;
    buf.data = (uint8_t*)malloc(numBytes);
    buf.bytes_recorded = 0;  // Initially, no samples are stored
    buf.bytes_per_sample = 0;
    buf.num_chan = 0;
    if (buf.data == NULL) {
      buf.size = 0;
    } else {
      buf.size = numBytes; // This represents how many bytes the buffer can hold
    }
    return buf;
}

void freeBuffer(Buffer& buf) {
// Function to safely deallocate the buffer and clean up
  free(buf.data); // Free the allocated memory
  buf.data = NULL; // Set the pointer to NULL to avoid using freed memory
  buf.bytes_recorded = 0;
  buf.bytes_per_sample = 0;
  buf.size = 0;
  buf.num_chan = 0;
}

void resetBuffer(Buffer& buffer) {
// Reset the buffer for continued use
  buffer.bytes_recorded = 0;
}

byte calculateChecksum(const byte* data, size_t len) {
// XOR checksum calculation
  byte checksum = 0;
  for (size_t i = 0; i < len; i++) {
    checksum ^= data[i];  // XOR checksum
  }
  return checksum;
}

void processBuffer(Buffer& buffer_in, Buffer& buffer_out) {
// =======================================================================================
// Process the buffer by averaging over AVG_LEN.
// 44.1 kHz is regular audio sampling rate. If singal of interest has smaller bandwidth
// we can still record at 44kHz and then decimate it which will reduce the noise.
// The data buffer contains values in following order: ch1,ch2,ch3,...,ch1,ch2,ch3,...
// We will have same order of channels but AVG_LEN less times data in each channel after processing
// =======================================================================================
  
  // The number of samples in buffer_in should be a multiPle of the averaging length and number of channels
  if ( ((buffer_in.bytes_recorded/buffer_in.bytes_per_sample) % AVG_LEN == 0) && (((buffer_in.bytes_recorded/buffer_in.bytes_per_sample) % buffer_in.num_chan) == 0) ) {
  
    int32_t* mysum = (int32_t*)malloc(buffer_in.num_chan * sizeof(int32_t)); // register to compute the averages

    if (mysum == NULL) {
      // unable to allocate memory
      buffer_out.bytes_recorded = 0;
      buffer_out.num_chan = 0;
      return;
    }

    uint16_t out_samples_per_channel = (buffer_in.bytes_recorded / buffer_in.bytes_per_sample) / AVG_LEN / buffer_in.num_chan;

    int16_t* sample_in  = (int16_t*) buffer_in.data;                                            // reset sample pointer (input)
    int16_t* sample_out = (int16_t*) buffer_out.data;                                           // reset result pointer (output)

    for(uint16_t m=0; m<out_samples_per_channel; m++) {                     // number of samples after averaging per channel
      // initialize sum for each channel by reading first sample for each channel
      for(uint16_t n=0; n<buffer_in.num_chan; n++){
        mysum[n] = *sample_in++;                                            // initialize sum
      } // end initialize
      // add subsequent sets of samplers to each channel's sum
      for (uint16_t o=1; o<AVG_LEN ; o++) {                                 // loop over number of average
        for(uint16_t n=0; n<buffer_in.num_chan; n++) {                      // loop over number of channels
          mysum[n] += *sample_in++;                                         // sum the values
        }
      } // end adding
      // compute the average and store average in output buffer
      for(uint16_t n=0; n<buffer_in.num_chan; n++){
        sample_out[n] = (int16_t) (mysum[n]/AVG_LEN);                       // average and increment output pointer
      } // end average
      sample_out += buffer_in.num_chan;
    } // end of averaging

    buffer_out.bytes_recorded = out_samples_per_channel * buffer_in.bytes_per_sample * buffer_in.num_chan;
    buffer_out.num_chan = buffer_in.num_chan;

    free(mysum);
    LOG_Dln("Averaging completed");
    
  } else {

    buffer_out.bytes_recorded = 0;
    buffer_out.num_chan = 0;
    LOG_Eln("Number of samples is not a multiple of num channels or averaging length");
    
  }
  
}

void computeChannelDifferences(Buffer& buffer_in, Buffer& buffer_delta) {
// =======================================================================================
// Compute the difference between two consecutive channels.
// This can be used for background supression if one channel contains the background pluse
// signal of interest and the other channel contains the background o0nly.
// Output will have half the number of channels. Its uncommen to have 4 input channels
// but this routine will work on more than two channels as long as its a multiple of 2.
// =======================================================================================

  // make sure we have correct number of samples in the input buffer to create channel pairs
  if ((buffer_in.num_chan % 2 == 0) && ((buffer_in.bytes_recorded/buffer_in.bytes_per_sample) % buffer_in.num_chan == 0) ) {

    int16_t* sample_in    = (int16_t*) buffer_in.data;                          // reset pointer
    int16_t* sample_delta = (int16_t*) buffer_delta.data;                       // reset pointer

    uint16_t total_iterations = buffer_in.bytes_recorded / buffer_in.bytes_per_sample / buffer_in.num_chan;
    uint16_t pairs = buffer_in.num_chan / 2; // Number of channel pairs

    for(uint16_t m = 0; m < total_iterations; m++) {
      for(uint16_t n = 0; n < pairs; n++) {                                     // process pairs
        int16_t diff = sample_in[2*n] - sample_in[2*n + 1];                     // Calculate the difference between consecutive channels (e.g., ch0 - ch1, ch2 - ch3)
        *sample_delta++ = diff;                                                 // Store the difference in the output buffer and increment pointer
      }
      sample_in += buffer_in.num_chan;                                          // Move the sample_in pointer ahead to process the next set of samples
    }
    buffer_delta.bytes_recorded = total_iterations * pairs * buffer_in.bytes_per_sample;
    buffer_delta.num_chan = pairs;
    LOG_Dln("Channel difference computed");
  } else { // we dont have channel pairs
    if (buffer_in.num_chan == 1) { // we have single channel
      buffer_delta.bytes_recorded = buffer_in.bytes_recorded;
      buffer_delta.num_chan = buffer_in.num_chan;
      memcpy(buffer_delta.data, buffer_in.data, buffer_in.bytes_recorded); 
      LOG_Eln("Number of channels is 1, copied input to output without difference");
    } else { // we have no channel or no data
      buffer_delta.bytes_recorded = 0;
      buffer_delta.num_chan = 0;
      LOG_Eln("Number of samples is not a multiple of num channels or 2, no data available");
    }
  }
}

