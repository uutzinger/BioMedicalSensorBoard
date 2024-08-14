/**
 * Read two microphones, Output to  CSV
 * @author Urs Utzinger
 * @copyright GPLv3 
 **/

#include "Arduino.h"
#include "AudioTools.h"
#include "AudioLibs/I2SCodecStream.h"

#define SAMPLE_RATE RATE_44K // 8k, 11k, 16k, 22k, 32k, 44k
#define NUM_CHANNELS 2
#define BIT_DEPTH BIT_LENGTH_16BITS
#define ADC_IN  ADC_INPUT_LINE1

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


int toSamplingFreq(samplerate_t rate) {
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

int toBitDepth(sample_bits_t bits) {
    switch (bits) {
        case BIT_LENGTH_16BITS: return 16;
        case BIT_LENGTH_18BITS: return 18;
        case BIT_LENGTH_20BITS: return 20;
        case BIT_LENGTH_24BITS: return 24;
        case BIT_LENGTH_32BITS: return 32;
        default:                return -1;  // Handle error for unknown bit length
    }
}

int sample_rate = toSamplingFreq(SAMPLE_RATE);
int bit_depth = toBitDepth(BIT_DEPTH);

AudioInfo               info_i2s_in(    sample_rate, NUM_CHANNELS, bit_depth);
AudioInfo               info_serial_out(sample_rate, NUM_CHANNELS, bit_depth);

DriverPins              ES8388pins; // board pins
AudioBoard              audio_board(AudioDriverES8388, ES8388pins); // audio board
I2SCodecStream          i2s_stream(audio_board); // i2s coded
CsvOutput<int16_t>      serial_out(Serial); // serial output
StreamCopy              copier(serial_out, i2s_stream); // stream the i2s input to serial port and i2s output
TwoWire                 ES8388Wire = TwoWire(0); // universal I2C interface

void setup() {
  
  // Serial Interface ------------
  Serial.begin(500000);
  while(!Serial){delay(50);} // Wait for Serial to be ready
  delay(5000);

  // Logging level ---------------
  AudioLogger::instance().begin(Serial, AudioLogger::Error); // Debug, Info, Warning, Error
  LOGLEVEL_AUDIODRIVER = AudioDriverInfo;

  LOGI("Defining I2C pins for codec");
  ES8388pins.addI2C(PinFunction::CODEC, SCLPIN, SDAPIN, ES8388ADDR, I2CSPEED, ES8388Wire);
  LOGI("Defining I2S pins for codec");
  ES8388pins.addI2S(PinFunction::CODEC, MCLKPIN, BCLKPIN, WSPIN, DIPIN, DOPIN);
  ES8388pins.begin();

  LOGI("Defining ES8388 In/Out lines")
  CodecConfig cfg;
  cfg.input_device  = ADC_IN;  
  cfg.i2s.bits      = BIT_DEPTH;
  cfg.i2s.rate      = RATE_44K; 
  cfg.i2s.channels  = NUM_CHANNELS;
  cfg.i2s.fmt       = I2S_NORMAL;
  cfg.i2s.mode      = MODE_SLAVE;  
  audio_board.begin(cfg);

  // Volume 
  auto success = audio_board.setInputVolume(1.0); // 0 to 100?
  LOGI("Setting input volume to 1: %s", success ? "ok" : "failed"); 
  
  LOGI("I2S config and begin");
  auto i2s_config = i2s_stream.defaultConfig(RX_MODE); //RXTX for duplex //RX for sink (audio in) //TX for source (audio out)
  i2s_config.copyFrom(info_i2s_in);
  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration
  LOGI("I2S stream volume is: %f", i2s_stream.volume());
  delay(1000);

  // CSV Out --------------------
  LOGI("Configuring Serial");
  serial_out.begin(info_serial_out);
  LOGI("Serial initialized");

  LOGI("Setup complete");

}

int vol = 0;
unsigned long lastTime = millis();
void loop() {

  unsigned long currentTime=millis();
  if ((currentTime - lastTime)> 200) {
    vol+=5;
    if (vol >= 100) {vol =0;}
    audio_board.setInputVolume(vol);
  }

  copier.copy();

} 
