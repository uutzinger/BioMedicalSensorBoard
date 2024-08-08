/**
 * Read two microphones, Output to audio line 1 and CSV
 * Filter input to 35-2500Hz with 2nd order Butterworth filter (hp & lp)
 * @author Urs Utzinger
 * @copyright GPLv3 
 **/

#include "Arduino.h"
#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"

//
#define SAMPLE_RATE 44100
#define NUM_CHANNELS 2
#define BIT_DEPTH 16

// I2C
#define SDAPIN             SDA // I2C Data,  Adafruit ESP32 S3 3, Sparkfun Thing Plus C 23
#define SCLPIN             SCL // I2C Clock, Adafruit ESP32 S3 4, Sparkfun Thing Plus C 22
#define I2CSPEED        100000 // Clock Rate
#define ES8388ADDR        0x10 // Address of ES8388 I2C port

// I2S, your configuration for the ES8388 board
#define MCLKPIN             A4 // Master Clock
#define BCLKPIN            SCK // Bit Clock
#define WSPIN               A5 // Word select
#define DOPIN             MISO // This is connected to DI on ES8388 (MISO)
#define DIPIN             MOSI // This is connected to DO on ES8388 (MOSI)

AudioInfo                     info_i2s_in(SAMPLE_RATE, NUM_CHANNELS, BIT_DEPTH);
AudioInfo                     info_serial_out(SAMPLE_RATE, NUM_CHANNELS/2, BIT_DEPTH);
AudioInfo                     info_i2s_out(SAMPLE_RATE, NUM_CHANNELS, BIT_DEPTH);

DriverPins                    ES8388pins;                                 // board pins
AudioBoard                    audio_board(AudioDriverES8388, ES8388pins); // audio board
I2SCodecStream                i2s_stream(audio_board);             // i2s coded
TwoWire                       ES8388Wire = TwoWire(0);                     // universal I2C interface
FilteredStream<int16_t, float> filtered_stream(i2s_out_stream, NUM_CHANNELS);
StreamCopy                    copier(serial_out, filtered_stream); // stream the binner output to serial port
CsvOutput<int16_t>            serial_out(Serial);                   // serial output

Need to setup dual output: i2s and csv Serial

// Current specifications of the filter: fc_high = 100, fc_low = 1150, fs = 8000. See Python program to compute coefficients
const float b_coefficients[] = { 0.0167599486, 0.0000000000,-0.0335198972, 0.0000000000, 0.0167599486 };
const float a_coefficients[] = { 1.0000000000,-3.5940402281, 4.8522352268,-2.9218654388, 0.6636721072 };

void setup() {
  
  // Serial Interface ------------
  Serial.begin(500000);
  while(!Serial); // Wait for Serial to be ready

  // Logging level ---------------
  AudioLogger::instance().begin(Serial, AudioLogger::Info); // Debug, Info, Warning, Error
  LOGLEVEL_AUDIODRIVER = AudioDriverInfo;

  ES8388pins.addI2C(PinFunction::CODEC, SCLPIN, SDAPIN, ES8388ADDR, I2CSPEED, ES8388Wire);
  ES8388pins.addI2S(PinFunction::CODEC, MCLKPIN, BCLKPIN, WSPIN, DIPIN, DOPIN);
  ES8388pins.begin();

  CodecConfig cfg;
  cfg.output_device = DAC_OUTPUT_LINE1;
  cfg.input_device  = ADC_INPUT_LINE1;  
  cfg.i2s.bits      = BIT_LENGTH_16BITS;
  // cfg.i2s.rate = RATE_8K;     
  cfg.i2s.rate      = RATE_44K;
  cfg.i2s.channels  = CHANNELS2;
  cfg.i2s.fmt       = I2S_NORMAL;
  cfg.i2s.mode      = MODE_SLAVE;
  audio_board.begin(cfg);

  auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE); //RXTX for douplex //RX for sink //TX for source
  i2s_config.copyFrom(info_in)_in;
  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration

  serial_out.begin(audio_info);


  // Filter -----------------
  inFiltered.setFilter(0, new IIR<float>(b_coefficients, a_coefficients));

  // CSV Out --------------------
  LOGI("Configuring Serial");
  serial_out.begin(info_out);
  LOGI("Serial initialized");
}

void loop() {

#ifdef PROCESS

  // Read the values from the conversion stream to the temperature processing buffer
  size_t bytes_read   = converted_stream.readBytes((uint8_t*) process_buffer, BYTES_PER_SAMPLE * NUM_CHANNELS);
  size_t samples_read = bytes_read / BYTES_PER_SAMPLE; 
  int16_t* sample_buffer = (int16_t*) process_buffer;

  assert (samples_read == NUM_CHANNELS);

  for (size_t ch = 0; ch < NUM_CHANNELS; ch++) {
    sample_buffer[ch] = (int16_t) calctemp(sample_buffer[ch], A_COEFF, B_COEFF, C_COEFF, V_IN, R1[ch], R2[ch], R3[ch]); // Temperature 
  }

  serial_out.write((uint8_t*)sample_buffer, NUM_CHANNELS * BYTES_PER_SAMPLE); // stream to output as byte type

#else
  copier.copy();
#endif
} 
