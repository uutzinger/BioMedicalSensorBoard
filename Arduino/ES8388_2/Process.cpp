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

void clipData(Buffer& buffer, int32_t max_value, int32_t min_value) {
// =======================================================================================
// Makes sure data in the buffer does not exceed the max and min values
// This is useful when reserving some values to indicate start and end bit pattern for 
// binary transmission as we do not want that bit pattern to occur in the data
// =======================================================================================

  if(buffer.bytes_per_sample == 1) {
    for(uint16_t i = 0; i < buffer.bytes_recorded; i += buffer.bytes_per_sample) {
      // Handle 8-bit data with manual clipping
      int32_t value = static_cast<int32_t>(buffer.data[i]); // Cast to int32_t for comparison
      value = (value < min_value) ? min_value : value;
      value = (value > max_value) ? max_value : value;
      buffer.data[i] = static_cast<uint8_t>(value); // Cast back to uint8_t
    }

  } else if(buffer.bytes_per_sample == 2) {
    for(uint16_t i = 0; i < buffer.bytes_recorded; i += buffer.bytes_per_sample) {
      // Handle 16-bit data
      int16_t* sample16 = reinterpret_cast<int16_t*>(buffer.data + i);
      int32_t value = static_cast<int32_t>(*sample16); // Cast to int32_t for comparison
      value = (value < min_value) ? min_value : value;
      value = (value > max_value) ? max_value : value;
      *sample16 = static_cast<int16_t>(value); // Cast back to int16_t
    }

  } else if(buffer.bytes_per_sample == 3) {
    for(uint16_t i = 0; i < buffer.bytes_recorded; i += buffer.bytes_per_sample) {
      // Handle 24-bit data
      // Construct the 24-bit integer from three bytes
      int32_t sample24 = (buffer.data[i] | (buffer.data[i + 1] << 8) | (buffer.data[i + 2] << 16));
      if(sample24 & 0x800000) sample24 |= 0xFF000000; // Sign extend if negative

      sample24 = (sample24 < min_value) ? min_value : sample24;
      sample24 = (sample24 > max_value) ? max_value : sample24;
      
      // Decompose the 24-bit integer back into three bytes
      buffer.data[i] = sample24 & 0xFF;
      buffer.data[i + 1] = (sample24 >> 8) & 0xFF;
      buffer.data[i + 2] = (sample24 >> 16) & 0xFF;
    }

  } else if(buffer.bytes_per_sample == 4) {
    for(uint16_t i = 0; i < buffer.bytes_recorded; i += buffer.bytes_per_sample) {
      // Handle 32-bit data
      int32_t* sample32 = reinterpret_cast<int32_t*>(buffer.data + i);
      int32_t value = *sample32; // Directly use dereferenced int32_t
      value = (value < min_value) ? min_value : value;
      value = (value > max_value) ? max_value : value;
      *sample32 = value; // Cast back to int32_t
    }
  }
}

void processBuffer(Buffer& buffer_in, Buffer& buffer_out) {
// =======================================================================================
// Process the buffer by averaging over AVG_LEN and decimating the buffer.
// If 44.1 kHz is audio sampling rate and we averge over 4 samples we will have 10kHz sampling rate after averaging
// The data buffer contains values in following order: ch1,ch2,ch3,...,ch1,ch2,ch3,...
// We will have same order of channels but AVG_LEN less times data in each channel after processing
// =======================================================================================
  
  // The number of samples in buffer_in should be a multiPle of the averaging length and number of channels
  // The size of the input buffer needs to be the same as the output buffer
  if ( ((buffer_in.bytes_recorded/buffer_in.bytes_per_sample) % AVG_LEN == 0) && 
       ((buffer_in.bytes_recorded/buffer_in.bytes_per_sample) % buffer_in.num_chan == 0) ) {

    if ( buffer_out.size >= (buffer_in.size/AVG_LEN) ) {

      uint16_t out_samples_per_channel = (buffer_in.bytes_recorded / buffer_in.bytes_per_sample) / AVG_LEN / buffer_in.num_chan;

      int16_t* mysum_16;
      int32_t* mysum_32;
      int64_t* mysum_64;

      int8_t*  sample_in8;
      int8_t*  sample_out8;
      int16_t* sample_in16;
      int16_t* sample_out16;
      uint8_t* sample_in24;
      uint8_t* sample_out24;
      int32_t* sample_in32;
      int32_t* sample_out32;

      // 8 bit data ----------
      if (buffer_in.bytes_per_sample == 1) {

        mysum_16 = (int16_t*)malloc(buffer_in.num_chan * sizeof(int16_t)); // register to compute the averages
        if (mysum_16 == NULL) {
          // unable to allocate memory
          LOG_Eln("Processing could not allocate memory"); 
          buffer_out.bytes_recorded = 0;
          buffer_out.num_chan = 0;
          return;
        }

        sample_in8  = (int8_t*) buffer_in.data;                                            // reset sample pointer (input)
        sample_out8 = (int8_t*) buffer_out.data;                                           // reset result pointer (output)

        for(uint16_t m=0; m<out_samples_per_channel; m++) {                     // number of samples after averaging per channel
          // initialize sum for each channel by reading first sample for each channel
          for(uint16_t n=0; n<buffer_in.num_chan; n++){
            mysum_16[n] = (int16_t) *sample_in8++;                                            // initialize sum
          } // end initialize
          // add subsequent sets of samplers to each channel's sum
          for (uint16_t o=1; o<AVG_LEN ; o++) {                                 // loop over number of average
            for(uint16_t n=0; n<buffer_in.num_chan; n++) {                      // loop over number of channels
              mysum_16[n] += (int16_t) *sample_in8++;                                         // sum the values
            }
          } // end adding
          // compute the average and store average in output buffer
          for(uint16_t n=0; n<buffer_in.num_chan; n++){
            sample_out8[n] = (int8_t) (mysum_16[n]/AVG_LEN);                       // average and increment output pointer
          } // end average
          sample_out8 += buffer_in.num_chan;
        } // end of averaging

        buffer_out.bytes_recorded = buffer_in.bytes_recorded;
        buffer_out.num_chan = buffer_in.num_chan;

        free(mysum_16);
        LOG_Dln("8 bit averaging completed");

      // 16 bit data -----------
      } else if (buffer_in.bytes_per_sample == 2) {

        int32_t* mysum_32;
        mysum_32 = (int32_t*)malloc(buffer_in.num_chan * sizeof(int32_t)); // register to compute the averages
        if (mysum_32 == NULL) {
          // unable to allocate memory
          LOG_Eln("Processing could not allocate memory"); 
          buffer_out.bytes_recorded = 0;
          buffer_out.num_chan = 0;
          return;
        }

        sample_in16  = (int16_t*) buffer_in.data;                                            // reset sample pointer (input)
        sample_out16 = (int16_t*) buffer_out.data;                                           // reset result pointer (output)

        for(uint16_t m=0; m<out_samples_per_channel; m++) {                     // number of samples after averaging per channel
          // initialize sum for each channel by reading first sample for each channel
          for(uint16_t n=0; n<buffer_in.num_chan; n++){
            mysum_32[n] = (int32_t) *sample_in16++;                                            // initialize sum
          } // end initialize
          // add subsequent sets of samplers to each channel's sum
          for (uint16_t o=1; o<AVG_LEN ; o++) {                                 // loop over number of average
            for(uint16_t n=0; n<buffer_in.num_chan; n++) {                      // loop over number of channels
              mysum_32[n] += (int32_t) *sample_in16++;                                         // sum the values
            }
          } // end adding
          // compute the average and store average in output buffer
          for(uint16_t n=0; n<buffer_in.num_chan; n++){
            sample_out16[n] = (int16_t) (mysum_32[n]/AVG_LEN);                       // average and increment output pointer
          } // end average
          sample_out16 += buffer_in.num_chan;
        } // end of averaging

        buffer_out.bytes_recorded = buffer_in.bytes_recorded;
        buffer_out.num_chan = buffer_in.num_chan;

        free(mysum_32);
        LOG_Dln("16 bit averaging completed");

      // 24 bit data -----------
      } else if (buffer_in.bytes_per_sample == 3) {
        // Allocate space for sums
          mysum_32 = (int32_t*)malloc(buffer_in.num_chan * sizeof(int32_t));
          if (mysum_32 == NULL) {
            LOG_Eln("Processing could not allocate memory");
            buffer_out.bytes_recorded = 0;
            buffer_out.num_chan = 0;
            return;
          }

          // Assuming the data is little endian
          sample_in24  = (uint8_t*)buffer_in.data; // Use uint8_t* for byte-wise access
          sample_out24 = (uint8_t*)buffer_out.data; // Assuming output is still 16-bit

          for(uint16_t m = 0; m < out_samples_per_channel; m++) {
            // Initialize sums
            for(uint16_t n = 0; n < buffer_in.num_chan; n++) {
              size_t idx = m * buffer_in.num_chan * 3 + n * 3; // Calculate index for each sample
              // Manual combination of 3 bytes into a 32-bit integer
              mysum_32[n] = sample_in24[idx] | (sample_in24[idx+1] << 8) | (sample_in24[idx+2] << 16);
              if (sample_in24[idx+2] & 0x80) { // Extend sign if the original data is signed
                mysum_32[n] |= 0xFF000000;
              }
            }

            // Add subsequent samples to each channel's sum
            for (uint16_t o = 1; o < AVG_LEN; o++) {
              for(uint16_t n = 0; n < buffer_in.num_chan; n++) {
                size_t idx = (m + o) * buffer_in.num_chan * 3 + n * 3; // Adjust index for each sample
                int32_t temp = sample_in24[idx] | (sample_in24[idx+1] << 8) | (sample_in24[idx+2] << 16);
                if (sample_in24[idx+2] & 0x80) { // Extend sign if the original data is signed
                  temp |= 0xFF000000;
                }
                mysum_32[n] += temp;
              }
            }

            // Compute the average and store in the output buffer as 24-bit data
            for(uint16_t n = 0; n < buffer_in.num_chan; n++) {
              int32_t average = mysum_32[n] / AVG_LEN;
              size_t idx = m * buffer_in.num_chan * 3 + n * 3;
              sample_out24[idx] = average & 0xFF; // Least significant byte
              sample_out24[idx+1] = (average >> 8) & 0xFF;
              sample_out24[idx+2] = (average >> 16) & 0xFF; // Most significant byte
            }

          }

          buffer_out.bytes_recorded = buffer_in.bytes_recorded;
          buffer_out.num_chan = buffer_in.num_chan;

          free(mysum_32);
          LOG_Dln("24 bit averaging completed");
    
      // 32 bit data -----------
      } else if (buffer_in.bytes_per_sample == 4) {

        int64_t* mysunm_64;
        mysum_64 = (int64_t*)malloc(buffer_in.num_chan * sizeof(int64_t)); // register to compute the averages
        if (mysum_64 == NULL) {
          // unable to allocate memory
          LOG_Eln("Processing could not allocate memory"); 
          buffer_out.bytes_recorded = 0;
          buffer_out.num_chan = 0;
          return;
        }

        int32_t* sample_in32  = (int32_t*) buffer_in.data;                                            // reset sample pointer (input)
        int32_t* sample_out32 = (int32_t*) buffer_out.data;                                           // reset result pointer (output)

        for(uint16_t m=0; m<out_samples_per_channel; m++) {                     // number of samples after averaging per channel
          // initialize sum for each channel by reading first sample for each channel
          for(uint16_t n=0; n<buffer_in.num_chan; n++){
            mysum_64[n] = (int64_t) *sample_in32++;                                            // initialize sum
          } // end initialize
          // add subsequent sets of samplers to each channel's sum
          for (uint16_t o=1; o<AVG_LEN ; o++) {                                 // loop over number of average
            for(uint16_t n=0; n<buffer_in.num_chan; n++) {                      // loop over number of channels
              mysum_64[n] += *sample_in32++;                                         // sum the values
            }
          } // end adding
          // compute the average and store average in output buffer
          for(uint16_t n=0; n<buffer_in.num_chan; n++){
            sample_out32[n] = (int32_t) (mysum_64[n]/AVG_LEN);                       // average and increment output pointer
          } // end average
          sample_out32 += buffer_in.num_chan;
        } // end of averaging

        buffer_out.bytes_recorded = buffer_in.bytes_recorded;
        buffer_out.num_chan = buffer_in.num_chan;

        free(mysum_64);
        LOG_Dln("23 bit averaging completed");

      } else {
        LOG_Eln("Bytes per sample needs to be 1,2,3 or 4"); 
        buffer_out.bytes_recorded = 0;
        buffer_out.num_chan = 0;
        return;
      }

    } else {
        LOG_Eln("Output buffer too small"); 
        buffer_out.bytes_recorded = 0;
        buffer_out.num_chan = 0;
        return;
    }

  } else {

    LOG_Eln("Number of samples is not a multiple of num channels or averaging length or output buffer is not as large as input buffer");
    buffer_out.bytes_recorded = 0;
    buffer_out.num_chan = 0;
    
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
  if (  (buffer_in.num_chan % 2 == 0) && 
       ((buffer_in.bytes_recorded/buffer_in.bytes_per_sample) % buffer_in.num_chan == 0) ) {

    // make sure we have sufficient output buffer size
    if (buffer_delta.size >= (buffer_in.size/2) ) {

      int8_t*  sample_in8;
      int8_t*  sample_delta8;
      int16_t* sample_in16;
      int16_t* sample_delta16;
      uint8_t* sample_in24;
      uint8_t* sample_delta24;
      int32_t* sample_in32;
      int32_t* sample_delta32;

      int8_t  diff8;
      int16_t diff16;
      int16_t diff32;
      
      uint16_t total_iterations = buffer_in.bytes_recorded / buffer_in.bytes_per_sample / buffer_in.num_chan;
      uint16_t pairs = buffer_in.num_chan / 2; // Number of channel pairs

      // 8 bit data -----------
      if (buffer_in.bytes_per_sample == 1) {
        sample_in8    = (int8_t*) buffer_in.data;                          // reset pointer
        sample_delta8 = (int8_t*) buffer_delta.data;                       // reset pointer

        for(uint16_t m = 0; m < total_iterations; m++) {
          for(uint16_t n = 0; n < pairs; n++) {                                     // process pairs
            diff8 = sample_in8[2*n] - sample_in8[2*n + 1];                     // Calculate the difference between consecutive channels (e.g., ch0 - ch1, ch2 - ch3)
            *sample_delta8++ = diff8;                                                 // Store the difference in the output buffer and increment pointer
          }
          sample_in8 += buffer_in.num_chan;                                          // Move the sample_in16 pointer ahead to process the next set of samples
        }
        buffer_delta.bytes_recorded = total_iterations * pairs * buffer_in.bytes_per_sample;
        buffer_delta.num_chan = pairs;
        LOG_Dln("Channel difference computed");

      // 16 bit data -----------
      } else if (buffer_in.bytes_per_sample == 2) {

        sample_in16    = (int16_t*) buffer_in.data;                          // reset pointer
        sample_delta16 = (int16_t*) buffer_delta.data;                       // reset pointer

        for(uint16_t m = 0; m < total_iterations; m++) {
          for(uint16_t n = 0; n < pairs; n++) {                                     // process pairs
            diff16 = sample_in16[2*n] - sample_in16[2*n + 1];                     // Calculate the difference between consecutive channels (e.g., ch0 - ch1, ch2 - ch3)
            *sample_delta16++ = diff16;                                                 // Store the difference in the output buffer and increment pointer
          }
          sample_in16 += buffer_in.num_chan;                                          // Move the sample_in16 pointer ahead to process the next set of samples
        }
        buffer_delta.bytes_recorded = total_iterations * pairs * buffer_in.bytes_per_sample;
        buffer_delta.num_chan = pairs;
        LOG_Dln("Channel difference computed");

      // 24 bit data -----------
      // This is computed on 3x8bit intervals
      } else if (buffer_in.bytes_per_sample == 3) {

        sample_in24    = (uint8_t*) buffer_in.data;                          // reset pointer
        sample_delta24 = (uint8_t*) buffer_delta.data;                    // reset pointer

        for(uint16_t m = 0; m < total_iterations; m++) {
            for(uint16_t n = 0; n < pairs; n++) {
                // Extract two consecutive 24-bit samples, combine into int32_t for arithmetic
                int32_t firstSample = (sample_in24[6*n] | (sample_in24[6*n+1] << 8) | (sample_in24[6*n+2] << 16));
                int32_t secondSample = (sample_in24[6*n+3] | (sample_in24[6*n+4] << 8) | (sample_in24[6*n+5] << 16));
                // Sign extend if the sample is negative
                if (firstSample & 0x00800000) firstSample |= 0xFF000000;
                if (secondSample & 0x00800000) secondSample |= 0xFF000000;
                // Calculate the difference
                diff32 = firstSample - secondSample;
                // Store the 24-bit difference back into the buffer
                sample_delta24[3*n]   = diff32 & 0xFF;
                sample_delta24[3*n+1] = (diff32 >> 8) & 0xFF;
                sample_delta24[3*n+2] = (diff32 >> 16) & 0xFF;
            }
            sample_in24 += buffer_in.num_chan * 3;     // Move the input pointer ahead
            sample_delta24 += pairs * 3;               // Move the delta pointer ahead
        }

        buffer_delta.bytes_recorded = total_iterations * pairs * 3; // 3 bytes per sample in delta
        buffer_delta.num_chan = pairs;
        LOG_Dln("Channel difference computed for 24-bit data");

      // 32 bit data -----------
      } else if (buffer_in.bytes_per_sample == 4) {
        sample_in32    = (int32_t*) buffer_in.data;                          // reset pointer
        sample_delta32 = (int32_t*) buffer_delta.data;                       // reset pointer

        for(uint16_t m = 0; m < total_iterations; m++) {
          for(uint16_t n = 0; n < pairs; n++) {                                       // process pairs
            diff32 = sample_in16[2*n] - sample_in16[2*n + 1];                   // Calculate the difference between consecutive channels (e.g., ch0 - ch1, ch2 - ch3)
            *sample_delta32++ = diff32;                                                 // Store the difference in the output buffer and increment pointer
          }
          sample_in32 += buffer_in.num_chan;                                          // Move the sample_in16 pointer ahead to process the next set of samples
        }
        buffer_delta.bytes_recorded = total_iterations * pairs * buffer_in.bytes_per_sample;
        buffer_delta.num_chan = pairs;
        LOG_Dln("Channel difference computed");

      } else {
        LOG_Eln("Bytes per sample needs to be 1,2,3 or 4"); 
        buffer_delta.bytes_recorded = 0;
        buffer_delta.num_chan = 0;
        return;
      }
    
    } else {
        LOG_Eln("Output buffer too small"); 
        buffer_delta.bytes_recorded = 0;
        buffer_delta.num_chan = 0;
        return;
    }

  } else { 
    
    // we dont have channel pairs

    if (buffer_in.num_chan == 1) { 
      
      // we have single channel

      if ( buffer_delta.size >= buffer_in.size ) {
        // copy the data
        buffer_delta.bytes_recorded = buffer_in.bytes_recorded;
        buffer_delta.num_chan = buffer_in.num_chan;
        memcpy(buffer_delta.data, buffer_in.data, buffer_in.bytes_recorded); 
        LOG_Eln("Number of channels is 1, copied input to output without difference");
      } else {

        // we create no result
        buffer_delta.bytes_recorded = 0;
        buffer_delta.num_chan = 0;
        LOG_Eln("Output buffer too small");
      }

    } else { 
      
      // we have no channel or no data
      buffer_delta.bytes_recorded = 0;
      buffer_delta.num_chan = 0;
      LOG_Eln("Number of samples is not a multiple of num channels or 2, no data available");
    }
  }

}

