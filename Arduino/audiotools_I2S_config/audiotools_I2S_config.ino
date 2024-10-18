/**
 * @brief We define a custom board with the i2c and read from microphone and print the serial
 * with the help of the AudioTools I2SCodecStream
 * @author phil schatzmann
 */

#include "AudioTools.h" // install https://github.com/pschatzmann/arduino-audio-tools
#include "AudioLibs/I2SCodecStream.h"

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

AudioInfo                     audio_info(44200, 2, 16);                // sampling rate, # channels, bit depth
DriverPins                    my_pins;                                 // board pins
AudioBoard                    audio_board(AudioDriverES8388, my_pins); // audio board
I2SCodecStream                i2s_stream(audio_board);                 // i2s codec
CsvOutput<int16_t>            serial_stream(Serial);                   // serial output
TwoWire                       myWire = TwoWire(0);                     // universal I2C interface

void setup() {
  Serial.begin(115200);
  while(!Serial){delay(50);} // Wait for Serial to be ready
  delay(5000);

  AudioLogger::instance().begin(Serial, AudioLogger::Debug); // Debug, Info, Warning, Error
  LOGLEVEL_AUDIODRIVER = AudioDriverDebug;

  Serial.println("Setup starting...");

  Serial.println("I2C pin ...");
  my_pins.addI2C(PinFunction::CODEC, SCLPIN, SDAPIN, ES8388ADDR, I2CSPEED, myWire);
  Serial.println("I2S pin ...");
  my_pins.addI2S(PinFunction::CODEC, MCLKPIN, BCLKPIN, WSPIN, DIPIN, DOPIN);

  Serial.println("Pins begin ..."); 
  my_pins.begin();

  Serial.println("Board begin ..."); 
  CodecConfig cfg;
  cfg.input_device  = ADC_INPUT_LINE1;  
  cfg.output_device = DAC_OUTPUT_ALL;  
  cfg.i2s.bits      = BIT_LENGTH_16BITS;
  cfg.i2s.rate      = RATE_44K; 
  cfg.i2s.channels  = CHANNELS2;
  cfg.i2s.fmt       = I2S_NORMAL;
  cfg.i2s.mode      = MODE_SLAVE;  
  audio_board.begin(cfg);

  Serial.println("Set Volume ...");
  audio_board.setInputVolume(77); // 0 to 100
  audio_board.setVolume(100); // 0 to 100

  Serial.println("I2S begin ...");
  auto i2s_config = i2s_stream.defaultConfig(RXTX_MODE);
  i2s_config.copyFrom(audio_info);
  i2s_stream.begin(i2s_config); // this should apply I2C and I2S configuration

  Serial.println("Setup completed ...");
}

void loop() { 
}
