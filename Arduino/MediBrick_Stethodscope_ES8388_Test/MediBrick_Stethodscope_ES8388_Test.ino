/**
 * Read two microphones, Output to audio line 1 and CSV
 * Filter input to 35-2500Hz with 2nd order Butterworth filter (hp & lp)
 * @author Urs Utzinger, Daniel Campa
 * @copyright GPLv3 
 **/

#include "Arduino.h"
#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"

//
// #define SAMPLE_RATE
// 8k, 11k, 16k, 22k, 32k, 44k, 48k, need to fix 48 it will take 44

#define SAMPLE_RATE RATE_8K
#define NUM_CHANNELS 2
#define BIT_DEPTH 16
#define DAC_OUT DAC_OUTPUT_LINE2
#define ADC_IN  ADC_INPUT_LINE1

// I2C
#define SDAPIN             SDA // I2C Data,  Adafruit ESP32 S3 3, Sparkfun Thing Plus C 23
#define SCLPIN             SCL // I2C Clock, Adafruit ESP32 S3 4, Sparkfun Thing Plus C 22
#define I2CSPEED        400000 // Clock Rate
#define ES8388ADDR        0x10 // Address of ES8388 I2C port

// I2S, your configuration for the ES8388 board
#define MCLKPIN             A4 // Master Clock
#define BCLKPIN            SCK // Bit Clock
#define WSPIN               A5 // Word select
#define DOPIN             MISO // This is connected to DI on ES8388 (MISO)
#define DIPIN             MOSI // This is connected to DO on ES8388 (MOSI)


int toSamplingFreq(samplerate_t rate) {
    switch (rate) {
        case RATE_8K:   return 8000;
        case RATE_11K:  return 11025;
        case RATE_16K:  return 16000;
        case RATE_22K:  return 22050;
        case RATE_24K:  return 24000;
        case RATE_32K:  return 32000;
        case RATE_44K:  return 44100;
        case RATE_48K:  return 48000;
        case RATE_64K:  return 64000;
        case RATE_88K:  return 88200;
        case RATE_96K:  return 96000;
        case RATE_128K: return 128000;
        case RATE_176K: return 176400;
        case RATE_192K: return 192000;
        default:        return -1;  // Handle error for unknown rate
    }
}


int sample_rate = toSamplingFreq(SAMPLE_RATE);

AudioInfo               info_i2s_in(sample_rate, NUM_CHANNELS, BIT_DEPTH);
AudioInfo               info_serial_out(sample_rate, NUM_CHANNELS, BIT_DEPTH);
AudioInfo               info_i2s_out(sample_rate, NUM_CHANNELS, BIT_DEPTH);

DriverPins              ES8388pins; // board pins
AudioBoard              audio_board(AudioDriverES8388, ES8388pins); // audio board
I2SCodecStream          i2s_stream(audio_board); // i2s coded
TwoWire                 ES8388Wire = TwoWire(0); // universal I2C interface

CsvOutput<int16_t>      serial_out(Serial); // serial output

// FilteredStream<int16_t, float> filtered_stream(i2s_stream, NUM_CHANNELS);

// MultiOutput          out; // allows multiplke outputs

// StreamCopy             copier(out, filtered_stream); // stream the i2s input to serial port and i2s output
// StreamCopy             copier(out, i2s_stream); // stream the i2s input to serial port and i2s output
StreamCopy              copier(serial_out, i2s_stream); // stream the i2s input to serial port and i2s output

// Current specifications of the filter: fc_high = 100, fc_low = 1150, fs = 8000. 
// See Python program to compute coefficients
// const float b_coefficients[] = { 0.0167599486, 0.0000000000,-0.0335198972, 0.0000000000, 0.0167599486 };
// const float a_coefficients[] = { 1.0000000000,-3.5940402281, 4.8522352268,-2.9218654388, 0.6636721072 };


void setup() {
  
  // Serial Interface ------------
  Serial.begin(500000);
  while(!Serial){} // Wait for Serial to be ready
  delay(500);

  // Logging level ---------------
  AudioLogger::instance().begin(Serial, AudioLogger::Warning); // Debug, Info, Warning, Error
  LOGLEVEL_AUDIODRIVER = AudioDriverInfo;

  LOGI("Defining I2C pins for codec");
  ES8388pins.addI2C(PinFunction::CODEC, SCLPIN, SDAPIN, ES8388ADDR, I2CSPEED, ES8388Wire);
  LOGI("Defining I2S pins for codec");
  ES8388pins.addI2S(PinFunction::CODEC, MCLKPIN, BCLKPIN, WSPIN, DIPIN, DOPIN);
  ES8388pins.begin();

  LOGI("Defining ES8388 In/Out lines")
  CodecConfig cfg;
  cfg.output_device = DAC_OUT;
  cfg.input_device  = ADC_IN;  
  cfg.i2s.bits      = BIT_LENGTH_16BITS;
  cfg.i2s.rate      = RATE_44K; 
  cfg.i2s.channels  = CHANNELS2;
  cfg.i2s.fmt       = I2S_NORMAL;
  cfg.i2s.mode      = MODE_SLAVE;
  audio_board.begin(cfg);

  LOGI("I2S config and begin");
  auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE); //RXTX for douplex //RX for sink //TX for source
  i2s_config.copyFrom(info_i2s_in);
  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration

  // Filter -----------------
  // inFiltered.setFilter(0, new IIR<float>(b_coefficients, a_coefficients));

  // CSV Out --------------------
  LOGI("Configuring Serial");
  serial_out.begin(info_serial_out);
  LOGI("Serial initialized");

  //out.add(serial_out);
  //out.add(is_stream);

  LOGI("Setup complete");
}

void loop() {

// #ifdef PROCESS

// GET THIS FROM TEMPRATURE TEST PROGRAM

//   // Read the values from the conversion stream to the temperature processing buffer
//   size_t bytes_read   = converted_stream.readBytes((uint8_t*) process_buffer, BYTES_PER_SAMPLE * NUM_CHANNELS);
//   size_t samples_read = bytes_read / BYTES_PER_SAMPLE; 
//   int16_t* sample_buffer = (int16_t*) process_buffer;

//   assert (samples_read == NUM_CHANNELS);

//   for (size_t ch = 0; ch < NUM_CHANNELS; ch++) {
//     sample_buffer[ch] = (int16_t) calctemp(sample_buffer[ch], A_COEFF, B_COEFF, C_COEFF, V_IN, R1[ch], R2[ch], R3[ch]); // Temperature 
//   }

//   serial_out.write((uint8_t*)sample_buffer, NUM_CHANNELS * BYTES_PER_SAMPLE); // stream to output as byte type

// #else
  copier.copy();
// #endif
} 
