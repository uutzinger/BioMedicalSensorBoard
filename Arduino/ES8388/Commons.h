#ifndef COMMONS_H_
#define COMMONS_H_

#include <Arduino.h>

// ===============================================================================================================
// I2C
#define SDAPIN            3 // I2C Data,  Adafruit ESP32 S3 3, Sparkfun Thing Plus C 23
#define SCLPIN            4 // I2C Clock, Adafruit ESP32 S3 4, Sparkfun Thing Plus C 22
#define I2CSPEED     100000 // Clock Rate
#define ES8388ADDR     0x10 // Address of 82388 I2C port

// I2S
#define MCLKPIN          14 // Master Clock
#define BCLKPIN          36 // Bit Clock
#define WSPIN             8 // Word select
#define DOPIN            37 // This is connected to DI on ES8388 (MISO)
#define DIPIN            35 // This is connected to DO on ES8388 (MOSI)
#define I2S_MODE  RXTX_MODE // RXTX_MODE, RX_MODE, TX_MODE, UNDEFINED_MODE 
#define MIC_VOLUME        0 // 0 to -96dB attenuation in 0.5dB steps
#define OUT_VOLUME        0 // 0 to -96dB attenuation in 0.5dB steps

// Serial output
#define BAUD_RATE    500000 // Up to 2,000,000

// Sampling, Buffer, Averaging
#define SAMPLE_RATE   44100 // 8k, 11.025k 16k 22.050k 24k 32k 44.1k 48k
#define BIT_WIDTH        16 // 16, 18, 20, 24, 32
#define AVG_LEN           4 // averaging (decimate)
#define NUM_CHAN          1 // audio channels
#define PAYLOAD_NUMDATABYTES 240 // 250 is maximum ESPNow message size but we need some additional data like id, and checksum

// Sensors
#define SENSORID_ECG      1 // sensor ID
#define SENSORID_TEMP     2 // sensor ID
#define SENSORID_SOUND    3 // sensor ID
#define SENSORID_OX       4 // sensor ID
#define SENSORID_IMP      5 // sensor ID

#if BIT_WIDTH == 16
  #define BYTES_PER_SAMPLE 2
#else
  #error "Unsupported Bitwidth"
#endif

// compute how many samples fit into payload
#define SAMPLES_IN_PAYLOAD (PAYLOAD_NUMDATABYTES / BYTES_PER_SAMPLE / NUM_CHAN) * NUM_CHAN // this is caculated to remove fractional values
// since we average the samples, we want more samples from i2s stream
#define BYTES_WANTED (PAYLOAD_NUMDATABYTES / BYTES_PER_SAMPLE / NUM_CHAN) * NUM_CHAN * BYTES_PER_SAMPLE * AVG_LEN // need more samples from i2s because we average

// Buffer for formatted text printing
extern char tmpStr[256];   // snprintf buffer

// Buffer to receive and process data
// We will fill this buffer with the i2s stream and process it into other buffers
typedef struct Buffer {
  uint8_t* data;                            // Pointer to the buffer
  uint16_t bytes_recorded;                  // Index to the next free position in the buffer, this is equivalent to the total number of valid samples in the buffer
  uint8_t  num_chan;                        // data is organized in ch0_1,ch1_1,... ch0_2,ch1_2,... ch0_n,ch1_n,..
  uint16_t bytes_per_sample;                // 
  uint16_t size;                            // Size of the buffer in maximum amount of samples the buffer can hold
} Buffer;

// Buffer to transmit data with serial binary or ESPNow
//   we need to agree to a data structure we want to send in binary format
#if BYTES_PER_SAMPLE  == 2
struct Message {
  uint8_t sensorID;                         // Sensor number
  uint8_t numData;                          // Number of valid samples per channel
  uint8_t numChannels;                      // Number of channels
  int16_t data[PAYLOAD_NUMDATABYTES/2];     // Array of 16-bit integers
  byte checksum;                            // checksum over structure excluding the checksum
};
#else
  #error "Unsupported Bitwidth"
#endif

#endif
