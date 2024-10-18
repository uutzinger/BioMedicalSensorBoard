/**
 * Generate test tones in left and right channel on ouput line 2
 * @file streams-generator-i2s.ino
 * @author Phil Schatzmann
 * @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/streams-generator-i2s/README.md 
 * @copyright GPLv3
 */
 
#include "AudioTools.h"
#include "AudioTools/AudioLibs/I2SCodecStream.h"
#include "logger.h"

#define AMPLITUDE          32000 // MAX 32000
#define TONE_L             N_AS5 
#define TONE_R             N_CS6
#define DAC_OUT DAC_OUTPUT_LINE2 // Plug your head phone into line2, the left connector for 4 pin audio plug
#define OUTPUT_VOLUME         66 // 0..100

#define BAUDRATE 500000

#define SAMPLE_RATE        44100
#define NUM_CHANNELS           2
#define BIT_DEPTH             16

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

AudioInfo                     info_L(SAMPLE_RATE, 1, BIT_DEPTH);
AudioInfo                     info_R(SAMPLE_RATE, 1, BIT_DEPTH);
AudioInfo                     info(SAMPLE_RATE, 2, BIT_DEPTH);
SineWaveGenerator<int16_t>    sineWave_L(AMPLITUDE);            // subclass of SoundGenerator with max amplitude of 32000
SineWaveGenerator<int16_t>    sineWave_R(AMPLITUDE);            // subclass of SoundGenerator with max amplitude of 32000
GeneratedSoundStream<int16_t> sound_L(sineWave_L);              // Stream generated from sine wave
GeneratedSoundStream<int16_t> sound_R(sineWave_R);              // Stream generated from sine wave
InputMerge<int16_t>           imerge;                           // merge two inputs to stereo
DriverPins                    ES8388pins;                       // board pins
AudioBoard                    audio_board(AudioDriverES8388, ES8388pins); // audio board
I2SCodecStream                i2s_stream(audio_board);          // i2s coded
StreamCopy                    copier(i2s_stream, imerge);       // copies merged tones into i2s output

TwoWire                       ES8388Wire = TwoWire(0);          // universal I2C interface

// Arduino Setup
void setup(void) {  
  // Open Serial 
 
  Serial.begin(BAUDRATE);
  while(!Serial){delay(100);} // Wait for serial port, do not use for autonomous opertion

  AudioLogger::instance().begin(Serial, AudioLogger::Warning); // None, Info, Warning, Debug
  LOGLEVEL_AUDIODRIVER = AudioDriverWarning;

  LOGln("I2C pins ...");
  ES8388pins.addI2C(PinFunction::CODEC, SCLPIN, SDAPIN, ES8388ADDR, I2CSPEED, ES8388Wire);
  LOGln("I2S pins ...");
  ES8388pins.addI2S(PinFunction::CODEC, MCLKPIN, BCLKPIN, WSPIN, DIPIN, DOPIN);
  LOGln("Pins begin ...");
  ES8388pins.begin();

  LOGln("Defining ES8388 In/Out lines")
  CodecConfig cfg;
  cfg.output_device = DAC_OUT;
  cfg.input_device  = ADC_INPUT_LINE1;  
  cfg.i2s.bits      = BIT_LENGTH_16BITS;
  // cfg.i2s.rate = RATE_8K;     
  cfg.i2s.rate      = RATE_44K;
  cfg.i2s.channels  = CHANNELS2;
  cfg.i2s.fmt       = I2S_NORMAL;
  cfg.i2s.mode      = MODE_SLAVE;
  LOGln("ES8388 begin ...");
  audio_board.begin(cfg);

  LOGln("Set Volume ...");
  auto success = audio_board.setVolume(OUTPUT_VOLUME); 
  LOGln("Setting OUT1 and OUT2 volume to %d: %s", OUTPUT_VOLUME, success ? "ok" : "failed"); 

  // start I2S
  LOGln("I2S config ...");
  auto config = i2s_stream.defaultConfig(TX_MODE); // sending out (TX), receiving (RX), both (RXTX)
  config.copyFrom(info); 
  LOGln("I2S begin ...");
  i2s_stream.begin(config);
  LOGln("I2S stream volume is: %f", i2s_stream.volume()); // 0..1

  LOGln("Starting sound generation...");
  // Setup sine wave
  sineWave_L.begin(info, TONE_L);
  sineWave_R.begin(info, TONE_R);
  // Merge input to stereo
  LOGln("Merging left and righ channel ...");
  imerge.add(sound_L);
  imerge.add(sound_R);
  LOGln("Merging begin ...");
  imerge.begin(info);

  LOGln("Config completed...");
  delay(2000); // Can not see all debug info otherwise

}

// Arduino loop - copy sound to out 
void loop() {
  copier.copy();
}
