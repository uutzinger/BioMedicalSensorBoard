/**
 * @brief Record Audio on Line1 and send to serial port.
 * @author Urs Utzinger
 */

// Sampling, Buffer, Averaging
#define SAMPLE_RATE   44100 // 8000, 11025 16000 22050, 24000, 32000, 44100, 48000
#define BIT_WIDTH        16 // 16, 18, 20, 24, 32
#define NUM_CHAN          2 // 1 or 2

// I2C
#define SDAPIN            3 // I2C Data,  Adafruit ESP32 S3 3, Sparkfun Thing Plus C 23
#define SCLPIN            4 // I2C Clock, Adafruit ESP32 S3 4, Sparkfun Thing Plus C 22
#define I2CSPEED     100000 // Clock Rate
#define ES8388ADDR     0x10 // Address of 82388 I2C port

// I2S
// DI is driven by ESP32 (ESP32 sends data to codec) to ESP32 MOSI (35)
// DO is driven by Codec (codec sends data to esp32) to ESP32 MISO (37)
#define MCLKPIN          14 // Master Clock
#define BCLKPIN          36 // Bit Clock
#define WSPIN             8 // Word select
#define DI8388           35 // This is DI on ES8388 and connected to data_out on ESP32 (e.g. MOSI)
#define DO8388           37 // This is DO on ES8388 and connected to data_in on ESP32 (e.g. MISO)
#define MIC_VOLUME        0 // 0 to -96dB attenuation in 0.5dB steps
#define OUT_VOLUME        0 // 0 to -96dB attenuation in 0.5dB steps

// Serial output
#define BAUD_RATE    500000 // Up to 2,000,000

// ===============================================================================================================
// Includes

#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"

// ===============================================================================================================
//  Global variables, structures and devices

AudioInfo           audio_info(SAMPLE_RATE, NUM_CHAN, BIT_WIDTH); // sampling rate, # of channels, bit depth
DriverPins          board_pins;                                   // pins used to connect to ES8388
AudioBoard          audio_board(AudioDriverES8388, board_pins);   // create an audio board
I2SCodecStream      i2s_stream(audio_board);                      // i2s coded that will read and write data from ES8388
CsvOutput<int16_t>  serial_stream(Serial);                        // serial output stream
StreamCopy          copier(serial_stream, i2s_stream);            // copy i2sStream to csvStream
TwoWire             myWire = TwoWire(0);                          // I2C interface

// ===============================================================================================================
// Setup
// ===============================================================================================================

void setup() {

  // Setup logging
  Serial.begin(BAUD_RATE);
  delay(2000);
  Serial.println("Setup starting...");

  LOGLEVEL_AUDIODRIVER = AudioDriverInfo;                        // Debug, Info, Warning, Error
  AudioLogger::instance().begin(Serial, AudioLogger::Info);

  // Setup pins to which ES8388 is connected to. 
  // Pin Functions:
  // HEADPHONE_DETECT, AUXIN_DETECT, PA (Power Amplifier), POWER, LED, KEY, SD, CODEC, CODEC_ADC, MCLK_SOURCE,
  // add i2c codec pins: scl, sda, port, frequency
  board_pins.addI2C(PinFunction::CODEC, SCLPIN, SDAPIN, ES8388ADDR, I2CSPEED, myWire);
  // add i2s pins: mclk, bck, ws, data_out, data_in ,(port)
  board_pins.addI2S(PinFunction::CODEC, MCLKPIN, BCLKPIN, WSPIN, DI8388, DO8388);
  //addPin if present (for LED, Buttons, Amp)
  //addSPI ir present (clk, miso, mosi, cs, SPI)
  board_pins.begin();

  // Codec on audio board
  // ----------------------
  // ADC_INPUT_LINE2, ADC_INPUT_LINE3, ADC_INPUT_ALL, ADC_INPUT_DIFFERENCE, ADC_INPUT_NONE
  // DAC_OUTPUT_LINE1, DAC_OUTPUT_LINE2, DAC_OUTPUT_ALL
  CodecConfig board_config;
  board_config.output_device = DAC_OUTPUT_NONE;
  if (NUM_CHAN == 2) {
    board_config.input_device = ADC_INPUT_ALL;
  } else {
    board_config.input_device = ADC_INPUT_LINE1;
  }
  board_config.i2s.mode     = MODE_SLAVE;                         // Micro controller is Master, otherwise MODE_MASTER
  board_config.i2s.fmt      = I2S_NORMAL;                         // I2S_NORMAL, I2S_LEFT, I2S_RIGHT, I2S_DSP
  board_config.setBitsNumeric(BIT_WIDTH);                         // 16, 24, 32 bits
  board_config.setRateNumeric(SAMPLE_RATE);                       // 8000, 11000, 16000, 22000, 32000, 44100
  audio_board.begin(board_config); 

  //Additional ADC configuration ---> Everything else is in default mode (See datasheet)
  //set to mono mix in ADC channel 1 (lin1/Mic pin) --> 00001000 == 0x08 in ADCControl 3
  //es8388_write_reg(ES8388_ADCCONTROL3, 0x08);

  audio_board.setInputVolume(MIC_VOLUME);                         // 0 to -96dB attenuation in 0.5dB steps
  audio_board.setVolume(OUT_VOLUME);                              // 0 to -96dB attenuation in 0.5dB steps

  // I2S Stream
  // ----------------------
  // RX_Mode: Microcontroller is the Audio Sink
  // TX_Mode: Microcontroller is the Audio Source
  // RXTX_MODE: Both
  I2SCodecConfig i2s_config;
  i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
  i2s_config.copyFrom(audio_info);                                // bits per sample, samplerate, channels
  i2s_stream.begin(i2s_config);

  // Copier
  // ----------------------
  serial_stream.begin(audio_info);

  Serial.println("Setup completed ...");
}

// ===============================================================================================================
// Main Loop 
// ===============================================================================================================
// We fill the input buffer until its full, then process the data and stream it.

void loop() { 
  copier.copy();
}
