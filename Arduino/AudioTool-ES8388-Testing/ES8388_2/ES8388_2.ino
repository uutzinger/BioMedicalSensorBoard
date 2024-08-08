/**
 * @brief We define a custom board with the i2c and i2s pins and record 2 input channels
 * with the help of the AudioTools I2SCodecStream. 
 * We process the data by averaging and calculating the difference between consecutive channels
 * Data istransmitted to serial port using text or binary mode.
 * Binary mode includes checksum calculation.
 * @author Urs Utzinger
 */

// Debug or not to debug
//#define DEBUG
#undef DEBUG

// ===============================================================================================================
// Includes

#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"
#include "Commons.h"
#include "myPrint.h"
#include "Process.h"
#include "Stream.h"

// ===============================================================================================================
//  Global variables, structures and devices

AudioInfo           audio_info(SAMPLE_RATE, NUM_CHAN, BIT_WIDTH); // sampling rate, # of channels, bit depth
DriverPins          board_pins;                                   // pins used to connect to ES8388
AudioBoard          audio_board(AudioDriverES8388, board_pins);   // create an audio board
I2SCodecStream      i2s_stream(audio_board);                      // i2s coded that will read and write data from ES8388
TwoWire             myWire = TwoWire(0);                          // I2C interface

// Buffers for processing and streaming
Buffer buffer_i2s_in;                                                // data from i2s
Buffer buffer_avg;                                                // data after averaging
Buffer buffer_delta;                                              // data after background subtraction, dont yet know if idea for background subtraction will work.
Buffer buffer_i2s_out;                                              // data after background subtraction, dont yet know if idea for background subtraction will work.

// Buffer to broadcast data to serial or ESPNow
Message message;                                                  // message structure for binary transmission

// Buffer for formatted text printing
char tmpStr[256];                                                 //snprintf buffer

void fillBufferFromI2S(Buffer& buffer, uint16_t bytes_wanted) {
// =======================================================================================
// Fill the input buffer with values from i2s
// We need to deal with insufficient amount of data returned from the i2s stream.
// If that is the case we will need to call this function again later and repeat the calls until we have
// all data.  
// =======================================================================================

  // how many bytes do we need to request?
  // buffer can be partially filled, so we need to request the remaining bytes
  size_t bytes_requested = bytes_wanted - buffer.bytes_recorded;  // number of bytes needed to fill the buffer

  // request the data
  size_t bytes_read = i2s_stream.readBytes((uint8_t*) buffer.data + buffer.bytes_recorded, bytes_requested);

  // check for issues

  // received less than requested?
  // we will continue requesting data until we have amount of requested data
  if (bytes_read < bytes_requested) {
    size_t bytes_missing = (bytes_requested - bytes_read);
    snprintf(tmpStr, sizeof(tmpStr), "I2S: missing bytes: %d, requested: %d", bytes_missing, bytes_requested); 
    LOG_Dln(tmpStr); 
  }

  // received more than requested?
  // this would be an issue as our buffer can not hold extra data.
  // if we ever receive more than requested we will neeed to store it in second buffer which is not implemented here.
  if (bytes_read > bytes_requested) {
    size_t bytes_excess = bytes_read - bytes_requested;
    snprintf(tmpStr, sizeof(tmpStr), "I2S: received %d excess bytes, requested: %d", bytes_excess, bytes_requested); 
    LOG_Eln(tmpStr); 
  }

  // Increment buffer index by number of samples read
  buffer.bytes_recorded += bytes_read;                            // increment buffer index

}

void fillI2SFromBuffer(Buffer& buffer_in, Buffer& buffer_out) {
// =======================================================================================
// Stream data from buffer to I2S
// Microcontroller is sourse
// =======================================================================================

  if (buffer_in.num_chan == buffer_out.num_chan) {
    if (buffer_in.size == buffer_out.size){
      size_t bytes_written = i2s_stream.write((const uint8_t*) buffer_in.data,  buffer_in.bytes_recorded);
      // check for issues
      if (bytes_written < buffer.bytes_recorded) {
        size_t bytes_refused = (buffer.bytes_recorded - bytes_written);
        snprintf(tmpStr, sizeof(tmpStr), "I2S: unable to send bytes: %d, available: %d", bytes_refused, buffer.bytes_recorded); 
        LOG_Dln(tmpStr); 
      }
    } else {
      // need to expand from smaller to larger size
      LOG_Eln("Upsampling output not implemented yet.");
    }
  } else if (buffer_in.num_chan == 1){
    // We need to copy the single channel to the other channels.
    LOG_Eln("Copy single channel to stereo not implemented yet.");
  }
 }

// ===============================================================================================================
// Setup
// ===============================================================================================================

void setup() {

  // Setup logging
  Serial.begin(BAUD_RATE);
  delay(2000);
  LOG_Iln("Setup starting...");

  #if defined DEBUG
  LOGLEVEL_AUDIODRIVER = AudioDriverDebug;                        // Debug, Info, Warning, Error
  AudioLogger::instance().begin(Serial, AudioLogger::Debug);
  #else
  LOGLEVEL_AUDIODRIVER = AudioDriverInfo;                         // Debug, Info, Warning, Error
  AudioLogger::instance().begin(Serial, AudioLogger::Info);
  #endif  

  // Setup pins to which ES8388 is connected to. 
  // Pin Functions:
  // HEADPHONE_DETECT, AUXIN_DETECT, PA (Power Amplifier), POWER, LED, KEY, SD, CODEC, CODEC_ADC, MCLK_SOURCE,
  board_pins.addI2C(PinFunction::CODEC, SCLPIN, SDAPIN, ES8388ADDR, I2CSPEED, myWire);
  board_pins.addI2S(PinFunction::CODEC, MCLKPIN, BCLKPIN, WSPIN, DOPIN, DIPIN);
  //addPin (for LED, Buttons, Amp)
  //addSPI (clk, miso, mosi, cs, SPI)
  board_pins.begin();

  // Codec on audio board
  // ----------------------
  // ADC_INPUT_LINE2, ADC_INPUT_LINE3, ADC_INPUT_ALL, ADC_INPUT_DIFFERENCE, ADC_INPUT_NONE
  // DAC_OUTPUT_LINE1, DAC_OUTPUT_LINE2, DAC_OUTPUT_ALL
  // RX_Mode: Microcontroller is the Audio Sink
  // TX_Mode: Microcontroller is the Audio Source
  CodecConfig board_config;
  if (I2S_MODE == RX_MODE) { // Microcontroller is the Audio Sink
    if (NUM_CHAN == 1) {
      board_config.input_device  = ADC_INPUT_LINE1;
    } else {
      board_config.input_device  = ADC_INPUT_ALL;
    }
    board_config.output_device   = DAC_OUTPUT_NONE;
  } else if (I2S_MODE == RXTX_MODE){ // The Microcontroller is the Audio Source and Sink
    if (NUM_CHAN == 1) {
      board_config.input_device  = ADC_INPUT_LINE1;
      board_config.output_device = DAC_OUTPUT_LINE1;
    } else {
      board_config.input_device  = ADC_INPUT_ALL;
      board_config.output_device = DAC_OUTPUT_ALL;
    }
  } else if (I2S_MODE == TX_MODE) { // The Microcontroller is the Audio Source
    if (NUM_CHAN == 1) {
      board_config.output_device = DAC_OUTPUT_LINE1;
    } else {
      board_config.output_device = DAC_OUTPUT_ALL;
    }
    board_config.input_device    = ADC_INPUT_NONE;
  } else { // Unspecified
     board_config.input_device   = ADC_INPUT_NONE;
     board_config.output_device  = DAC_OUTPUT_NONE;
  }
  board_config.i2s.mode          = MODE_SLAVE;                    // Micro controller is Master, otherwise MODE_MASTER
  board_config.i2s.fmt           = I2S_NORMAL;                    // I2S_NORMAL, I2S_LEFT, I2S_RIGHT, I2S_DSP
  board_config.setBitsNumeric(BIT_WIDTH);                         // 16, 24, 32 bits
  board_config.setRateNumeric(SAMPLE_RATE);                       // 8000, 11000, 16000, 22000, 32000, 44100
  audio_board.begin(board_config); 
     
  // audio_board.setInputVolume(MIC_VOLUME);                      // 0 to -96dB attenuation in 0.5dB steps
  // audio_board.setVolume(out_VOLUME);                           // 0 to -96dB attenuation in 0.5dB steps

  // I2S Stream
  // ----------------------
  I2SCodecConfig i2s_config;
  if (I2S_MODE == RXTX_MODE){
    i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
  } else if (I2S_MODE == TX_MODE) {
    i2s_config = i2s_stream.defaultConfig(TX_MODE);
  } else if (I2S_MODE == RX_MODE) {
    i2s_config = i2s_stream.defaultConfig(RX_MODE);
  } else {
    i2s_config = i2s_stream.defaultConfig(UNDEFINED_MODE);
  }
  i2s_config.copyFrom(audio_info);                                // bits per sample, samplerate, channels
  // i2s_config.input_device = ...
  // i2s_config.output_device = ...
  // i2s_config.sda_active = true // false
  i2s_stream.begin(i2s_config);

  // Setting up the buffers

  // I2S input buffer
  buffer_i2s_in  = initBuffer(BYTES_WANTED);
  buffer_i2s_in.num_chan = NUM_CHAN;
  buffer_i2s_in.bytes_per_sample = BYTES_PER_SAMPLE;

  // Processing output (filtering) buffer
  buffer_avg   = initBuffer(BYTES_WANTED/AVG_LEN);
  buffer_avg.num_chan = NUM_CHAN;
  buffer_avg.bytes_per_sample = BYTES_PER_SAMPLE;

  // check the buffers
  if (buffer_i2s_in.data == NULL){ LOG_Eln("Failed to allocate input buffer"); }
  if (buffer_avg.data == NULL){ LOG_Eln("Failed to allocate processing buffer"); }

  // Buffer holding difference between channel 1 and channel 2
  if (NUM_CHAN % 2 == 0) {
    buffer_delta = initBuffer(BYTES_WANTED/AVG_LEN/2); 
    buffer_delta.num_chan = NUM_CHAN/2;
    buffer_delta.bytes_per_sample = BYTES_PER_SAMPLE;
    if (buffer_delta.data == NULL){ LOG_Eln("Failed to allocate differential buffer"); }
  } else if (NUM_CHAN == 1) {
    buffer_delta = initBuffer(BYTES_WANTED/AVG_LEN); 
    buffer_delta.num_chan = NUM_CHAN;
    buffer_delta.bytes_per_sample = BYTES_PER_SAMPLE;
    if (buffer_delta.data == NULL){ LOG_Eln("Failed to allocate differential buffer"); }
  }

  // I2S input buffer
  buffer_i2s_out = initBuffer(BYTES_WANTED);
  buffer_i2s_out.num_chan = NUM_CHAN;
  buffer_i2s_out.bytes_per_sample = BYTES_PER_SAMPLE;
  if (buffer_i2s_out.data == NULL){ LOG_Eln("Failed to allocate output buffer"); }

  LOG_Iln("Setup completed ...");
}

// ===============================================================================================================
// Main Loop 
// ===============================================================================================================
// We fill the input buffer until its full, then process the data and stream it.

void loop() { 

  // Obtain samples
  if (buffer_i2s_in.bytes_recorded < BYTES_WANTED) {
    if (i2s_stream.available()) {
      fillBufferFromI2S(buffer_i2s_in, BYTES_WANTED);
    }
  }

  // Process and Stream
  if (buffer_i2s_in.bytes_recorded == BYTES_WANTED) {
    processBuffer(buffer_i2s_in, buffer_avg);
    if (NUM_CHAN % 2 == 0) {
      computeChannelDifferences(buffer_avg, buffer_delta);
      streamCustomDataSerial(buffer_delta, SENSORID_SOUND);
      // streamBinaryDataSerial(buffer_delta, message, SENSORID_SOUND);
    } else {
      streamCustomDataSerial(buffer_avg, SENSORID_SOUND);
      // streamBinaryDataSerial(buffer_avg, message, SENSORID_SOUND);
    }
    
    // reset the buffers 
    resetBuffer(buffer_i2s_in);
    resetBuffer(buffer_avg);
    resetBuffer(buffer_delta);
  }

}

