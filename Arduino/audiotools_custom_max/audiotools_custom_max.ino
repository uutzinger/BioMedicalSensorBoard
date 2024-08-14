/**
 * @brief We define a custom board with the i2c and i2s pins and output_device a sine
 * with the help of the AudioTools I2SCodecStream
 * @author phil schatzmann
 */

#include "AudioTools.h" // install https://github.com/pschatzmann/arduino-audio-tools
#include "AudioLibs/I2SCodecStream.h"

// I2C
#define SDAPIN             SDA // I2C Data,  Adafruit ESP32 S3 3, Sparkfun Thing Plus C 23
#define SCLPIN             SCL // I2C Clock, Adafruit ESP32 S3 4, Sparkfun Thing Plus C 22
#define I2CSPEED        100000 // Clock Rate
#define ES8388ADDR        0x10 // Address of ES8388 I2C port

// I2S, your configuration for the ES8388 board
#define MCLKPIN             A4 // Master Clock
#define BCLKPIN            SCK // Bit Clock
#define WSPIN               A5 // Word select
#define DOPIN             MISO // This is connected to DI on ES8388 (MISO) 37
#define DIPIN             MOSI // This is connected to DO on ES8388 (MOSI) 35

AudioInfo                     audio_info(44200, 2, 16);                // sampling rate, # channels, bit depth
SineWaveGenerator<int16_t>    sine_wave(32000);                        // amplitude
GeneratedSoundStream<int16_t> sound_stream(sine_wave);                 // sound generator
DriverPins                    my_pins;                                 // board pins
AudioBoard                    audio_board(AudioDriverES8388, my_pins); // audio board
I2SCodecStream                i2s_out_stream(audio_board);             // i2s coded
StreamCopy                    copier(i2s_out_stream, sound_stream);    // stream copy sound generator to i2s codec
TwoWire                       myWire = TwoWire(0);                     // universal I2C interface

void setup() {
  // Serial Interface ------------
  Serial.begin(500000);
  while(!Serial){delay(50);} // Wait for Serial to be ready
  delay(5000);

  // Logging level ---------------
  AudioLogger::instance().begin(Serial, AudioLogger::Error); // Debug, Info, Warning, Error
  LOGLEVEL_AUDIODRIVER = AudioDriverInfo;

  Serial.println("Setup starting...");

  Serial.println("I2C pin ...");
  my_pins.addI2C(PinFunction::CODEC, SCLPIN, SDAPIN, ES8388ADDR, I2CSPEED, myWire);
  Serial.println("I2S pin ...");
  my_pins.addI2S(PinFunction::CODEC, MCLKPIN, BCLKPIN, WSPIN, DIPIN, DOPIN);

  Serial.println("Pins begin ..."); 
  my_pins.begin();

  Serial.println("Board begin ..."); 
  audio_board.begin();

  Serial.println("I2S begin ..."); 
  auto i2s_config = i2s_out_stream.defaultConfig();
  i2s_config.copyFrom(audio_info);  
  i2s_out_stream.begin(i2s_config); // this should apply I2C and I2S configuration

  // Setup sine wave
  Serial.println("Sine wave begin...");
  sine_wave.begin(audio_info, N_B4); // 493.88 Hz

  Serial.println("Setup completed ...");
}

// Arduino loop - copy sound to out
void loop() { copier.copy(); }
