/**
 * Read two microphones, Output to  CSV, Change Microphone Volume up and down
 * @author Urs Utzinger
 * @copyright GPLv3 
 **/

#include "Arduino.h"
#include "AudioTools.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "logger.h"

#define BAUDRATE 500000

#define SAMPLE_RATE RATE_44K // 8k, 11k, 16k, 22k, 32k, 44k
#define NUM_CHANNELS CHANNELS2
#define BIT_DEPTH BIT_LENGTH_16BITS
#define ADC_IN  ADC_INPUT_LINE1

#define INPUT_VOLUME        0 // 0..100 (not linear)

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


uint16_t toSamplingFreq(samplerate_t rate) {
    switch (rate) {
        case RATE_8K:   return   8000;
        case RATE_11K:  return  11025;
        case RATE_16K:  return  16000;
        case RATE_22K:  return  22050;
        case RATE_24K:  return  24000;
        case RATE_32K:  return  32000;
        case RATE_44K:  return  44100;
        case RATE_48K:  return  48000;
        case RATE_64K:  return  64000;
        case RATE_88K:  return  88200;
        case RATE_96K:  return  96000;
        case RATE_128K: return 128000;
        case RATE_176K: return 176400;
        case RATE_192K: return 192000;
        default:        return -1;  // Handle error for unknown rate
    }
}

uint8_t toBitDepth(sample_bits_t bits) {
    switch (bits) {
        case BIT_LENGTH_16BITS: return 16;
        case BIT_LENGTH_18BITS: return 18;
        case BIT_LENGTH_20BITS: return 20;
        case BIT_LENGTH_24BITS: return 24;
        case BIT_LENGTH_32BITS: return 32;
        default:                return -1;  // Handle error for unknown bit length
    }
}

uint16_t toNumChannels(channels_t channels) {
    switch (channels) {
        case CHANNELS2:  return  2;
        case CHANNELS8:  return  8;
        case CHANNELS16: return 16;
        default:         return -1;  // Handle error for unknown number of channels
    }
}

uint16_t sample_rate  =  toSamplingFreq(SAMPLE_RATE);
uint8_t bit_depth     =  toBitDepth(BIT_DEPTH);
uint16_t num_channels =  toNumChannels(NUM_CHANNELS);

AudioInfo           info_i2s_in(    sample_rate, num_channels, bit_depth);
AudioInfo           info_serial_out(sample_rate, num_channels, bit_depth);

DriverPins          ES8388pins;                                 // board pins
AudioBoard          audio_board(AudioDriverES8388, ES8388pins); // audio board
I2SCodecStream      i2s_stream(audio_board);                    // i2s coded
CsvOutput<int16_t>  serial_out(Serial);                         // serial output
StreamCopy          copier(serial_out, i2s_stream);             // stream the i2s input to serial port

TwoWire             ES8388Wire = TwoWire(0);                    // universal I2C interface

void setup() {
  
  // Serial Interface ---
  Serial.begin(BAUDRATE);
  while(!Serial){delay(100);} // Wait for Serial to be ready, do not use for autonomous operation
  //delay(1000);

  // Logging level ---
  // Regular
  AudioLogger::instance().begin(Serial, AudioLogger::Error); // Debug, Info, Warning, Error
  LOGLEVEL_AUDIODRIVER = AudioDriverError;
  // Max Debug
  // AudioLogger::instance().begin(Serial, AudioLogger::Debug); // Debug, Info, Warning, Error
  // LOGLEVEL_AUDIODRIVER = AudioDriverDebug;

  LOGln("Setup Starting ...");

  LOGln("I2C pins ...");
  ES8388pins.addI2C(PinFunction::CODEC, SCLPIN, SDAPIN, ES8388ADDR, I2CSPEED, ES8388Wire);
  LOGln("I2S pins ...");
  ES8388pins.addI2S(PinFunction::CODEC, MCLKPIN, BCLKPIN, WSPIN, DIPIN, DOPIN);
  LOGln("Pins begin ...");
  ES8388pins.begin();

  LOGln("ES8388 In/Out lines")
  CodecConfig cfg;
  cfg.input_device  = ADC_IN;  
  cfg.i2s.bits      = BIT_DEPTH;
  cfg.i2s.rate      = SAMPLE_RATE; 
  cfg.i2s.channels  = NUM_CHANNELS;
  cfg.i2s.fmt       = I2S_NORMAL;
  cfg.i2s.mode      = MODE_SLAVE;  
  LOGln("ES8388 begin ...");
  audio_board.begin(cfg);

  // Microphone Volume 
  LOGln("Set Volume ...");
  // 0 to 100, internally it will map to 9 discrete levels
  // Vol:   0, 12.5,  25, 37.5,   50, 62.5,   75, 87.5, 100
  // Gain:  1,    2,   4,    8,   16,   32,   63,  126, 252
  auto success = audio_board.setInputVolume(INPUT_VOLUME); 
  LOGln("Setting microphone volume to %d: %s", INPUT_VOLUME, success ? "ok" : "failed"); 
  //auto success = audio_board.setVolume(100); // 0 to 100, integer
  //LOGln("Setting output volume to 100%: %s", success ? "ok" : "failed"); 
  
  LOGln("I2S config ...");
  auto i2s_config = i2s_stream.defaultConfig(RX_MODE); //RXTX for duplex //RX for sink (audio in) //TX for source (audio out)
  i2s_config.copyFrom(info_i2s_in);
  LOGln("I2S begin ...");
  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration
  LOGln("I2S stream volume is: %f", i2s_stream.volume()); // 0..1
  delay(1000);

  // CSV Out --------------------
  LOGln("CSV output begin ...");
  serial_out.begin(info_serial_out);

  LOGln("Setup complete");

}

//int vol = 0;
//int volStep = 100;
//unsigned long lastTime = millis();

void loop() {

  //unsigned long currentTime=millis();
  //if ((currentTime - lastTime)> 5000) {
  //  lastTime = currentTime;
  //  vol+=volStep;
  //  if ( (vol == 100) || (vol==0) ) {volStep = -volStep;}
  //  audio_board.setInputVolume(vol);
  //}

  copier.copy();
    
} 
