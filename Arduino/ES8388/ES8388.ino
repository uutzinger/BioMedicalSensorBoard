/**
 * @brief We define a custom board with the i2c and i2s pins and record 2 input channels
 * with the help of the AudioTools I2SCodecStream. 
 * We process the data by averaging and calculating the difference between consecutive channels
 * Data can be transmitted to serial port using text or binary mode.
 * Binary mode includes checksum calculation.
 * @author Urs Utzinger
 */

#include "AudioTools.h" // install https://github.com/pschatzmann/arduino-audio-tools
#include "AudioLibs/I2SCodecStream.h"
#include "Print.h"

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

// Serial output
#define BAUD_RATE    500000 //

// Sampling, Buffer, Averaging
#define SAMPLE_RATE   44100 //
#define BIT_WIDTH        16 // 
#define AVG_LEN           4 // averaging (decimate)
#define NUM_CHAN          2 // audio channels
#define BYTES_PER_SAMPLE sizeof(int16_t)  // We will work with int16 data when signals are measured
#define PAYLOAD_NUMDATABYTES 240 // depends on size of a data packets to process and transmit

// Sensors
#define SENSORID_ECG      1 // ESPNow message sensor ID
#define SENSORID_TEMP     2 // ESPNow message sensor ID
#define SENSORID_SOUND    3 // ESPNow message sensor ID
#define SENSORID_OX       4 // ESPNow message sensor ID
#define SENSORID_IMP      5 // ESPNow message sensor ID

// Debug or not to debug
#define DEBUG
//#undef DEBUG

// For Serial Transmission of binary Data:
const byte startSignature[4] = {0xFF, 0x7F, 0x00, 0x80}; // Start signature, max int16 followed by min int16, which is unlikely to occur naturally
const byte stopSignature[4]  = {0x00, 0x80, 0xFF, 0x7F}; // Stop signaturee, min int16 followed by max int16, which is unlikely to occur naturally

// ===============================================================================================================
// Support routines and structures

// Buffer to receive and process data
struct Buffer {
    int16_t* data;        // Pointer to the buffer
    uint16_t num_samples; // Index to the next free position in the buffer, this is equivalent to the number of valid samples in the buffer
    bool full;            // Flag indicating whether the buffer is full
    uint16_t size;        // Size of the buffer in maximum amount of samples the buffer can hold
    uint8_t num_chan;     // data is organized in ch0_1,ch1_1,... ch0_2,ch1_2,... ch0_n,ch1_n,..
};

// Buffer to send data with serial binary or ESPNow
struct DataMessage {
  uint8_t sensorID;                                        // Sensor number
  uint8_t numData;                                         // Number of valid integers per channel
  uint8_t numChannels;                                     // Number of channels
  int16_t dataArray[PAYLOAD_NUMDATABYTES/sizeof(int16_t)]; // Array of 16-bit integers
  byte    checksum;                                        // checksum over structure excluding the checksum
};

// Function to initialize the buffer structure and allocate memory for the buffer
//  provide number of samples (not bytes) as buffer size
Buffer initBuffer(uint16_t bufferSize) {
    Buffer buf;
    buf.data = (int16_t*)malloc(bufferSize * sizeof(int16_t)); // Allocate memory
    buf.num_samples = 0;
    buf.full = false;
    if (buf.data == NULL) {
      buf.size = 0;
    } else {
      buf.size = bufferSize;
    }
    buf.num_chan = 0;
    return buf;
}

// Function to safely deallocate the buffer and clean up
void freeBuffer(Buffer& buf) {
    if (buf.data != NULL) {
        free(buf.data); // Free the allocated memory
        buf.data = NULL; // Set the pointer to NULL to avoid using freed memory
        buf.num_samples = 0;  // points to the next free location in the buffer
        buf.full = false;
        buf.size = 0;
        buf.num_chan = 0;
    }
}

// Reset and re-use the buffer
void resetBuffer(Buffer& buffer) {
  buffer.full = false;
  buffer.num_samples = 0;
  // Reset other properties as needed
}

// XOR checksum calculation
byte calculateChecksum(const byte* data, size_t len) {
  byte checksum = 0;
  for (size_t i = 0; i < len; i++) {
    checksum ^= data[i];  // XOR checksum
  }
  return checksum;
}

// ===============================================================================================================
//  Global variables, structures and devices

AudioInfo           audio_info(SAMPLE_RATE, NUM_CHAN, BIT_WIDTH); // sampling rate, # channels, bit depth
DriverPins          my_pins;                                      // board pins
AudioBoard          audio_board(AudioDriverES8388, my_pins);      // audio board
I2SCodecStream      i2s_stream(audio_board);                      // i2s coded
CsvStream<int16_t>  serial_out(Serial);                           // Serial data output
StreamCopy          copier(serial_out, i2s_stream);               // stream copy to copy sound generator to i2s codec
TwoWire             myWire = TwoWire(0);                          // universal I2C interface

// Buffers for processing and streaming
Buffer buffer_in;
Buffer buffer_out;
Buffer buffer_delta;

// Buffer to broadcast data to serial or ESPNow
DataMessage myBinaryData;

// Calculated later
uint16_t samples_in_payload;
uint16_t samples_wanted;
uint16_t bytes_wanted;
uint16_t avg_samples_per_channel;

// Buffer for formatted text printing
char tmpStr[256];

// ===============================================================================================================
// Setup
// ===============================================================================================================

void setup() {

  // Setup logging
  Serial.begin(BAUD_RATE);
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);

  #if defined DEBUG
  LOGLEVEL_AUDIODRIVER = AudioDriverDebug; // Debug, Info, Warning, Error
  #else
  LOGLEVEL_AUDIODRIVER = AudioDriverInfo; // Debug, Info, Warning, Error
  #endif  
  delay(2000);

  LOG_Iln("Setup starting...");

  // computing how many samples fit into payload
  samples_in_payload      = (PAYLOAD_NUMDATABYTES / BYTES_PER_SAMPLE / NUM_CHAN) * NUM_CHAN;
  // since we average the samples, we want more samples from i2s stream
  samples_wanted          = (PAYLOAD_NUMDATABYTES / BYTES_PER_SAMPLE / NUM_CHAN) * NUM_CHAN * AVG_LEN; // need more samples from i2s because we average
  // when we request samples from i2s we need to request number of bytes
  bytes_wanted            = (PAYLOAD_NUMDATABYTES / BYTES_PER_SAMPLE / NUM_CHAN) * NUM_CHAN * AVG_LEN * BYTES_PER_SAMPLE; // from i2s
  // the number of damples we will have after averaging for each channel
  avg_samples_per_channel = samples_in_payload / NUM_CHAN; 

  uint16_t buffer_in_size    = samples_wanted;
  uint16_t buffer_out_size   = samples_wanted/AVG_LEN;
  uint16_t buffer_delta_size = samples_wanted/AVG_LEN/2;

  // Setting up the buffers
  buffer_in    = initBuffer(buffer_in_size);
  buffer_out   = initBuffer(buffer_out_size);   
  buffer_delta = initBuffer(buffer_delta_size); 

  buffer_in.num_chan = NUM_CHAN;

  if (buffer_in.data == NULL){
    LOG_Eln("Failed to allocate input buffer");
  }
  if (buffer_out.data == NULL){
    LOG_Eln("Failed to allocate processing buffer");
  }
  if (buffer_delta.data == NULL){
    LOG_Eln("Failed to allocate differential buffer");
  }

  // Setup pins
  LOG_Dln("I2C pin ...");
  // - add i2c codec pins:           scl,    sda,    port,       i2c_rate, I2C object)
  my_pins.addI2C(PinFunction::CODEC, SCLPIN, SDAPIN, ES8388ADDR, I2CSPEED, myWire);
  // - add i2s pins:                 mclk, bclk, ws, data_out, data_in
  LOG_Dln("I2S pin ...");
  my_pins.addI2S(PinFunction::CODEC, MCLKPIN, BCLKPIN, WSPIN, DOPIN, DIPIN);

  LOG_Dln("Pins begin ..."); 
  my_pins.begin();

  LOG_Dln("Board begin ..."); 
  audio_board.setInputVolume(0); // 0 to -96dB attenuation in 0.5dB steps
  audio_board.begin();

  // start I2S & codec with i2c and i2s configured above
  LOG_Dln("I2S begin ..."); 
  auto i2s_config = i2s_stream.defaultConfig();
  i2s_config.copyFrom(audio_info);  
  i2s_config.input_device = ADC_INPUT_ALL;
  i2s_config.output_device = DAC_OUTPUT_NONE;

  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration

  LOG_Dln("Starting Serial Out...");

  auto csvConfig = serial_out.defaultConfig();
  csvConfig.sample_rate = SAMPLE_RATE/AVG_LEN;
  csvConfig.channels = 1;
  csvConfig.bits_per_sample = 16;
  serial_out.begin(csvConfig);  

  LOG_Iln("Setup completed ...");
}

// ===============================================================================================================
// Main Loop Support Functions
// ===============================================================================================================

// =======================================================================================
// Fill the input buffer with values from i2s
void fillBuffer(Buffer& buffer, uint16_t samples_wanted) {

  uint16_t bytes_wanted = samples_wanted * BYTES_PER_SAMPLE;

  // how many bytes do we need to request?
  // buffer can be partially filled, so we need to request the remaining bytes
  size_t bytes_requested = bytes_wanted - buffer.num_samples*BYTES_PER_SAMPLE; // number of bytes needed to fill buffer

  size_t bytes_read = i2s_stream.readBytes((uint8_t*) buffer.data + buffer.num_samples * BYTES_PER_SAMPLE, bytes_requested);

  // check for issues

  // partial sample received?
  if (bytes_read % BYTES_PER_SAMPLE > 0) { LOG_Eln("I2S Error: We received a number of bytes that is not a multiple of bytes per sample"); }

  // received less than requested?
  // we will continue requestinf data until we have amount of requested data
  size_t samples_requested = bytes_requested/BYTES_PER_SAMPLE;
  if (bytes_read < bytes_requested) {
    size_t samples_missing = (bytes_requested - bytes_read)/BYTES_PER_SAMPLE;
    snprintf(tmpStr, sizeof(tmpStr), "I2S: missing samples: %d, requested: %d", samples_missing, samples_requested); 
    LOG_Dln(tmpStr); 
  }

  // received more than requested?
  // this would be a real issue as our buffer can not hold extra data
  if (bytes_read > bytes_requested) {
    size_t samples_excess = (bytes_read - bytes_requested)/BYTES_PER_SAMPLE;
    snprintf(tmpStr, sizeof(tmpStr), "I2S: received too many samples: %d, requested: %d", samples_excess, samples_requested); 
    LOG_Eln(tmpStr); 
  }

  // Increment buffer index by number of samples read
  buffer.num_samples += (bytes_read/BYTES_PER_SAMPLE);                             // increment buffer index

  // Check if buffer is not yet full
  //   otherwise process the data
  //   
  if (buffer.num_samples < samples_wanted) {
    buffer.full = false;
    LOG_Dln("Need more data to fill buffer");
  } else {
    buffer.full = true;
    LOG_Dln("Buffer complete");
  }

}

// =======================================================================================
// Process the buffer by averaging over AVG_LEN
// The data buffer contains values in following order: ch1,ch2,ch3,...,ch1,ch2,ch3,...
// We will have same order of channels but AVG_LEN less times data in each channel
void processBuffer(Buffer& buffer_in, Buffer& buffer_out) {

  // The number of samples in buffer_in should be a multiPle of the averaging length and number of channels
  if ( (buffer_in.num_samples % AVG_LEN == 0) && (buffer_in.num_samples % buffer_in.num_chan == 0) ) {

    int32_t* mysum = (int32_t*)malloc(buffer_in.num_chan * sizeof(int32_t)); // register to compute the averages

    if (mysum == NULL) {
      buffer_out.num_samples = 0;
      buffer_out.full = false;
      buffer_out.num_chan = 0;
      return;
    }

    uint16_t out_samples_per_channel = buffer_in.num_samples / AVG_LEN / buffer_in.num_chan;

    int16_t* sample_in  = buffer_in.data;                                            // reset sample pointer (input)
    int16_t* sample_out = buffer_out.data;                                           // reset result pointer (output)

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

    buffer_out.num_samples = out_samples_per_channel * buffer_in.num_chan;
    buffer_out.full = true;
    buffer_out.num_chan = buffer_in.num_chan;

    free(mysum);
    LOG_Dln("Averaging completed");
    
  } else {

    buffer_out.num_samples = 0;
    buffer_out.full = false;
    buffer_out.num_chan = 0;
    LOG_Eln("Number of samples is not a multiple of num channels or averaging length");

  }
  
} // end process buffer

// =======================================================================================
// Compute the difference between two consecutive channels.
// This can be used for background supression.
// Output will have half the number of channels.
void computeChannelDifferences(Buffer& buffer_in, Buffer& buffer_delta) {

  // make sure we have correct number of samples in the input buffer
  if ((buffer_in.num_chan % 2 == 0) && (buffer_in.num_samples % buffer_in.num_chan == 0) ) {

    int16_t* sample_in = buffer_in.data;                                                 // reset pointer
    int16_t* sample_delta = buffer_delta.data;                                           // reset pointer

    uint16_t total_iterations = buffer_in.num_samples / buffer_in.num_chan;
    uint16_t pairs = buffer_in.num_chan / 2; // Number of channel pairs

    for(uint16_t m = 0; m < total_iterations; m++) {
      for(uint16_t n = 0; n < pairs; n++) {                                     // process pairs
        int16_t diff = sample_in[2*n] - sample_in[2*n + 1];                     // Calculate the difference between consecutive channels (e.g., ch0 - ch1, ch2 - ch3)
        *sample_delta++ = diff;                                                 // Store the difference in the output buffer and increment pointer
      }
      sample_in += buffer_in.num_chan;                                          // Move the sample_in pointer ahead to process the next set of samples
    }
    buffer_delta.num_samples = total_iterations * pairs;
    buffer_delta.full = true;
    buffer_delta.num_chan = pairs;
    LOG_Dln("Channel difference computed");
  } else {
    if (buffer_in.num_chan == 1) {
      buffer_delta.num_samples = buffer_in.num_samples;
      buffer_delta.full = buffer_in.full;
      buffer_delta.num_chan = buffer_in.num_chan;
      memcpy(buffer_delta.data, buffer_in.data, buffer_in.num_samples * BYTES_PER_SAMPLE); 
      LOG_Eln("Number of channels is 1, copied input to output without difference");
    } else {
      buffer_delta.num_samples = 0;
      buffer_delta.full = false;
      buffer_delta.num_chan = 0;
      LOG_Eln("Number of samples is not a multiple of num channels or 2, no data available");
    }
  }
}

// =======================================================================================
// Simple stream to serial copy
void streamData(Buffer& buffer) {
    serial_out.write((uint8_t*)buffer.data, buffer.num_samples*BYTES_PER_SAMPLE); // stream to serial
}

// =======================================================================================
// Custom text streaming
// If serial buffer is full, data will be discarded
void streamCustomData(Buffer& buffer) {

  // Format is: SensorID, channel0, channel1, channel2, ... NewLine
  bool ok = true;
  const size_t tmpStrSize = sizeof(tmpStr);
  uint16_t total_iterations = buffer.num_samples / buffer.num_chan;
  for(uint16_t m = 0; m < total_iterations; m++) {
    int pos = snprintf(tmpStr, tmpStrSize, "%u", SENSORID_SOUND);
    for(uint16_t n = 0; n < buffer.num_chan; n++) {
      size_t remaining = tmpStrSize - pos;
      pos += snprintf(tmpStr + pos, remaining, ",%d", buffer.data[m*buffer.num_chan+n]); 
      if (pos >= tmpStrSize) { // break if tmpStr is full and dont print remainder of data
        break;
      }
    }
    if (Serial.availableForWrite() > strlen(tmpStr) + 2) { // +2 for newline characters
        Serial.println(tmpStr);
      } else {
        // Not enough space in serial output buffer, increase baudrate and buffer size
        ok = false;
      } 
  }
  if ( ok == false) { LOG_Eln("Serial print: buffer overlow!"); }
}

// =======================================================================================
// Transmit the data in binary format over the serial port
// First a start signature is sent
// Then the data structure is sent byte by byte
// At the end we send end signature
// Buffer will be reformatted to match the data structure used for ESPNow
void streamBinaryData(Buffer& buffer, DataMessage& message) {

  const byte* bytePointer;
  message.sensorID = (uint8_t) SENSORID_SOUND;                             // ID for sensor, e.g. ECG, Sound, Temp etc.
  message.numData = (uint8_t) buffer.num_samples/buffer.num_chan;          // Number of samples per channel, is computed after every ADC read update
  message.numChannels = (uint8_t) buffer.num_chan;                         // Number of channels
  memcpy(message.dataArray, buffer.data, buffer.num_samples * BYTES_PER_SAMPLE); // copy the data from the ADC buffer to ESPNow structure
  message.checksum = 0;                                                    // We want to exclude the checksum field when calculating the checksum 
  bytePointer = reinterpret_cast<const byte*>(&message);                   //
  message.checksum = calculateChecksum(bytePointer, sizeof(message));      // fill in the checksum
  Serial.write(startSignature, sizeof(startSignature));                    // Start signature
  bytePointer = reinterpret_cast<const byte*>(&message);                   //
  Serial.write(bytePointer, sizeof(message));                              // Message data
  Serial.write(stopSignature, sizeof(stopSignature));                      // Stop signature
}

// ===============================================================================================================
// Main Loop 
// ===============================================================================================================

void loop() { 

  if (buffer_in.full==false) {
    if (i2s_stream.available()){
      fillBuffer(buffer_in, samples_wanted);
    }
  }

  if (buffer_in.full==true) {
    processBuffer(buffer_in, buffer_out);
    resetBuffer(buffer_in);
  }

  if (buffer_out.full==true) {
    computeChannelDifferences(buffer_out, buffer_delta);
    resetBuffer(buffer_out);
  }

  if (buffer_delta.full==true) {
    streamCustomData(buffer_delta);
    // streamBinaryData(buffer, myBinaryData);
    resetBuffer(buffer_delta);
  } 

}
