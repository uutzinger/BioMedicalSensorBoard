/******************************************************************************************************/
// AFE44XX Test Program
// 
// Allows User to Experiment with Settings for Pulse Oximeter AFE during runtime through terminal.
//
// In additions this program shows how to
//   - Startup and Configure AFE
//   - Inspect Registers
//   - Run Diagnostics
//   - Read background subtracted IR and RED readings continously
//
// The AFE functions are organized into the following groups:
//   - Photodiode receiver settings
//     - Transimpedance amplifier feedback resistor and capacitor
//     - Second amplifier stage gain
//   - LED transmitter settings
//     - Infrared and red LED power settings
//   - Timing
//     - Program timing settings from the AFE4490 datasheet
//       Modify default settings by a linear factor but maintaining ADC reset pulse, sampling offset and conversion offset
//     - Averaging of readings
//   - All other hardware options
//
// Urs Utzinger 2024
/******************************************************************************************************/

// Includes
// ---------
#include <Arduino.h>
#include <stdlib.h> 
#include "afe44xx.h"
#include "logger.h"

// Select the  defaultlog level, you can change this at runtime also
// #define LOG_LEVEL LOG_LEVEL_NONE 
// #define LOG_LEVEL LOG_LEVEL_ERROR
// #define LOG_LEVEL LOG_LEVEL_WARN 
#define LOG_LEVEL LOG_LEVEL_INFO 
// #define LOG_LEVEL LOG_LEVEL_DEBUG

// Serial
#define BAUD_RATE 500000  // Up to 2,000,000 on ESP32, however more than 500 kBaud might be unreliable

// Hardware Pin Connection
// -----------------------
// Check assembly instructions for https://github.com/uutzinger/BioMedicalSensorBoard/blob/main/SPO2_Board/assembly.md
// DRDY, CS, PWDN connection are required for this software to run.
// If you do not want to use data ready interrupt, you can poll the data directly from the registers.
//
#define RESET_PIN    14  // active low, hardware reset, use if SPI not longer works with the sensor
#define DRDY_PIN     12  // data ready interrupt from AFE
#define CS_PIN       15  // chip select, active low, needed for SPI, you can hardwire this pin or activate when you have multipole SPI devices
#define PDALARM_PIN  32  // Photodiode Alarm pin, this can also be used to show activity of the AFE measurement process
#define LEDALARM_PIN 26  // LED Alarm pin, this can also be used to show activity of the AFE measurement process
#define DIAGEND_PIN  27  // This can be used to wait until diagnosis is finished, but it is usually not necessary
#define PWDN_PIN     25  // power down pin, active low, you can hardwire for always on and turn off LED, Photodiode and other hardware with software

// AFE Global Configuration Variables
// ----------------------------------
// these are needed for interactive user programming during runtime
/* Diagnostics */
uint32_t afe44xx_diag_status;         // holds results of the diagnosis function
/* Report data */
bool    afe44xx_reporting    = false; // continously report IR and red measurements on serial port
/* Receiver Stage  */
bool    afe44xx_sameGain     = true;  // same gain and feddback ressistor/capacitor for LED1 and LED2
int8_t  afe44xx_stage2Gain1  = 0;     // stage 2 gain for LED1: -1 disable, 0=1, 1=1.5, 2=2, 2=3, 4=4
int16_t afe44xx_CF1          = 5;     // feedback capacitor on Photodiode amplifier for LED1: 5pF, 5..275pF
int16_t afe44xx_RF1          = 500;   // Feedback resistor on Photodiode amplifier for LED1: 500kOhm, -1..1000kOhm, -1 = 0kOhm
uint8_t afe44xx_Fc           = 0;     // Low pass filter edge frequency 0=500Hz or 1=1000Hz
uint8_t afe44xx_CAmbient     = 0;     // Photodiode ambient current 0..9 micro Amp, you can measure by blocking the PD with black tape, depends on temperature
int8_t  afe44xx_stage2Gain2  = 0;     // stage 2 gain for LED 2, -1 disable, 0=1, 1=1.5, 2=2, 2=3, 4=4, its better to increase power of LED than to increase this gain
int16_t afe44xx_CF2          = 5;     // Feedback capacitor: 5pF 0..275 pF, same as LED1
int16_t afe44xx_RF2          = 500;   // Feedback resistor: 500kOhm -1..1000kOhm, same as LED1
/* Transmitter Stage */
uint8_t afe44xx_txRefVoltage = 0;     // Reference votlage for LEDs 1=0.5V, 0,2=0.75V, 3=1V, higher Voltage results in more current which increases brightness of LED
uint8_t afe44xx_range        = 0;     // Current range for LEDs 0=hig, 1=low, 2=high, 3=off // low is 50, 75, 100 mA, high 100, 150, 200 mA depending on voltage reference
uint8_t afe44xx_led1Power    = 24;    // LED1 power 0..255, this should be adjusted for each measurement subject so that readings are larger than 1,048,576 (half of max reading (2^21-1))
uint8_t afe44xx_led2Power    = 24;    // LED2 power 0..255
/* Digitial IO mode */
bool    afe44xx_tristate     = false; // Output logic tri state should be enabled if more than one SPI device is attached to the SPI bus, when the device is inactive to reduce power consumption
bool    afe44xx_hbridge      = true;  // use h-bridge for LED driver, this should be the default default
uint8_t afe44xx_alarmpins    = 0b101; // Photodiode and LED alarm pins can indicate activity, e.g. show that LED is sampled or LED is converted, see options below
/* 
Alarmpins
  PD Alarm pin                LED Alarm pin
  0 Sample LED2 pulse         Sample LED1 pulse
  1 LED2 LED pulse            LED1 LED pulse
  2 Sample LED2 ambient pulse Sample LED1 ambient pulse
  3 LED2 convert              LED1 convert
  4 LED2 ambient convert      LED1 ambient convert
  5 None                      None
  6 None                      None
  7 None                      None 
*/
float   afe44xx_factor       = 1.0; // Timing factor e.g. 0.5 or 8, 1 default, the default factor results in 500 samples per second.
/* 
Timing
  With a factor of 1.0 the default timing for 8000 clock cycles occurs. 
  At 4Mhz clock rate this will result in 500 measurements per second,
  on both red and ir channel including backgroudn subtraction.
  If averaging is set other than 1, the data rate will be reduced by the numer of averages.
  The default timing is taken from data sheet example. ADC reset pulse length is not changed by factor.
  If factor is <1 you will receive more samples per second, if its larger you will receive less samples per second.
  */
/* Hardware configuration*/
bool    afe44xx_bypassADC    = false; // use internal ADC, the default
bool    afe44xx_powerRX      = true;  // power the photodiode circuit, it will not work otherwise
bool    afe44xx_powerTX      = true;  // power the LED driver, it will not work otherwise
bool    afe44xx_powerAFE     = true;  // power the device, it will not work otherwise
uint8_t afe44xx_numAverage   = 4;     // number of averages caculated for LED1,2 and ambient measurements, this occurs inside the AFE
bool    afe44xx_running      = false; // keep track if system is running
bool    afe44xx_shortDiag    = true;  // short diagnostic 8ms or long diagnostics 16ms
bool    afe44xx_useXTAL      = true;  // use external crystal, otherwise provide clock signal, usual design is with crystal

// Program Globals
uint32_t   currentTime;                                               // Populated at beginning of main loop
uint32_t   lastTime;                                                  // to compute sampling frequency
float      samplingRate;                                              // computed in main loop
char       serialInputBuff[64];                                       // general text buffer
bool       serialCommandComplete = false;                             // keep track of serial input until we receive CR
const char waitmsg[] = {"Waiting 5 seconds, skip by sending enter"};  // Allows user to open serial terminal to observe the debug output before the loop starts
extern int currentLogLevel;                                           // log level when using the logger.cpp provided logging functions
char       tmpStr[256];                                               // general string buffer

// Data retrieved from the AFE
// ---------------------------
int32_t irRawValue = 0;      // raw value for IR LED data
int32_t redRawValue = 0;     // raw value for Red LED data
int32_t irAmbientValue = 0;  // Background reading for IR LED
int32_t redAmbientValue = 0; // Background reading for Red LED
int32_t irValue = 0;         // IR data, background subtracted by AFE
int32_t redValue = 0;        // Red data, background subtracted by AFE

// Instantiate the device driver
// -----------------------------
AFE44XX afe44xx(CS_PIN, PWDN_PIN, DRDY_PIN);

/******************************************************************************************************/
// Setup
/******************************************************************************************************/

void setup() {

  // Initialize globals
  // ------------------
  currentLogLevel = LOG_LEVEL;
  afe44xx_diag_status = 0;

  // Serial
  // ------
  Serial.begin(BAUD_RATE);      // setup serial
  Serial.setTimeout(1000);      // default is 1000 
  serialTrigger(waitmsg, 5000); // give user time to open serial terminal
  
  LOGI("Starting");

  // AFE
  // ------
  afe44xx.begin();              // setup AFE: power cycle, SPI.being, clear control register

  // Additional wire connections
  pinMode(RESET_PIN,   OUTPUT); // Reset pin, active low
  digitalWrite(RESET_PIN,HIGH); // Reset pin high, high-low transition creates reset
  pinMode(PDALARM_PIN, INPUT);  // Photodiode Alarm, active high
  pinMode(LEDALARM_PIN,INPUT);  // LED Alarm, active high
  pinMode(DIAGEND_PIN, INPUT);  // Diag End, active high
  LOGI("AFE: Pins for RESET, PD&LED ALARM, DIAGEND set");

  // Hard reset (do a software reset instead)
  // LOGD("AFE: hard reset")
  // digitalWrite(RESET_PIN,LOW);
  // delay(100);
  // digitalWrite(RESET_PIN,HIGH);
  // delay(500);

  // Initialize the AFE to default values (instead do all individual steps as outline below)
  // afe44xx.initialze();

  // 0) Reset the AFE
  afe44xx.reset();                                          // software reset

  // 1) Configure Timer
  afe44xx.setTimers(afe44xx_factor);                        // set timing

  afe44xx.clearCNTRL0();                                    // clear CONTROL0 register

  // 2) Configure AFE
  afe44xx.bypassADC(afe44xx_bypassADC);                     // use internal/external ADC
  afe44xx.setOscillator(afe44xx_useXTAL);                   // use crystal or clock signal
  afe44xx.setShortDiagnostic(afe44xx_shortDiag);            // short or long diagnostic sequence
  afe44xx.setTristate(afe44xx_tristate);                    // tri state or push pull output
  afe44xx.setHbridge(afe44xx_hbridge);                      // h bridge or push pull output
  afe44xx.setAlarmpins(afe44xx_alarmpins);                  // monitor activity on PD and LED alarm pins  
  afe44xx.setPowerAFE(afe44xx_powerAFE);                    // power up/down
  afe44xx.setPowerRX(afe44xx_powerRX);                      // power up/down
  afe44xx.setPowerTX(afe44xx_powerTX);                      // power up/down

  // 3) Configure Receiver
  afe44xx.setSameGain(afe44xx_sameGain);                    // same or separate settings for LED1 and LED2
  afe44xx.setFirstStageFeedbackCapacitor(afe44xx_CF1,1);    // e.g. 5pF, LED1
  afe44xx.setFirstStageFeedbackCapacitor(afe44xx_CF2,2);    // e.g. 5pF,LED2
  afe44xx.setFirstStageFeedbackResistor(afe44xx_RF1,1);     // e.g. 500kOhm LED1
  afe44xx.setFirstStageFeedbackResistor(afe44xx_RF2,2);     // e.g. 500kOhm LED1
  afe44xx.setFirstStageCutOffFrequency(afe44xx_Fc );        // 500 or 100 Hz low pass
  afe44xx.setSecondStageGain(afe44xx_stage2Gain1, true, 1); // gain enabled, on LED1
  afe44xx.setSecondStageGain(afe44xx_stage2Gain2, true, 2); // gain enabled on LED2
  afe44xx.setCancellationCurrent(afe44xx_CAmbient);         // e.g. 0 microAmp

  // 4) Configure Transmitter
  afe44xx.setLEDdriver(afe44xx_txRefVoltage,afe44xx_range); // e.g. 0.75V, 150mA
  afe44xx.setLEDpower(afe44xx_led1Power, 1);                // e.g. 0..255 on both LEDs
  afe44xx.setLEDpower(afe44xx_led2Power, 2);                // e.g. 0..255 on both LEDs

  // 5) Start the continous measurement process
  afe44xx_running = afe44xx.start(afe44xx_numAverage);      // start system and engage averaging

  // Diagnostics
  // -----------
  LOGD("AFE: Register Dump");
  afe44xx.dumpRegisters();                                  // Display all readable registers
  /* Run Diagnostics and Print */  
  LOGD("AFE: Starting Diagnostics");
  afe44xx.readDiag(afe44xx_diag_status);                    // Run diagnostics
  afe44xx.printDiag(afe44xx_diag_status);                   // Display in readable format
  if (afe44xx.selfCheck()) { LOGD("AFE: Self Check Passed"); } else { LOGD("AFE: Self Check Failed") ; } // Run self check, system should be running
  LOGD("AFE: Read Timers");
  afe44xx.readTimers();
  // Read and display the status of the diagnostic pins
  bool pdAlarmStatus  = digitalRead(PDALARM_PIN);           // check pins
  bool ledAlarmStatus = digitalRead(LEDALARM_PIN);
  bool diagEndStatus  = digitalRead(DIAGEND_PIN);
  LOGI("AFE: Alarm Pins:");
  LOGI("  PD:        %s",  pdAlarmStatus ? "HIGH" : "LOW");
  LOGI("  LED:       %s", ledAlarmStatus ? "HIGH" : "LOW");
  LOGI("  Diag Ends: %s",  diagEndStatus ? "HIGH" : "LOW");

  // Clear the serial input buffer
  while (Serial.available()) { Serial.read(); }

  LOGI("Setup complete, AFE started"); 

  lastTime = micros();
  samplingRate = 500.*afe44xx_factor/float(afe44xx_numAverage);

}

/******************************************************************************************************/
// Loop
/******************************************************************************************************/

void loop() {

  currentTime = micros();                                   // keep track of loop time

  // Obtain new data with ISR
  if (afe44xx.afe44xx_data_ready) {
    afe44xx.getData(irRawValue, redRawValue, irAmbientValue, redAmbientValue, irValue, redValue);
    samplingRate = 0.9*samplingRate + 100000./float(currentTime-lastTime); // low pass filtered sampling rate
    lastTime = currentTime;
    if (afe44xx_reporting) {
      snprintf(tmpStr, sizeof(tmpStr), "IR: %9i\tRed: %9i\tIR_a: %i\tRed_a: %9i\t S\\s: %.1f", irValue, redValue, irAmbientValue, redAmbientValue, samplingRate); Serial.println(tmpStr);
    }
  }

  // Check for user serial input
  static size_t index = 0;
  while (Serial.available() && !serialCommandComplete) {
    char c = Serial.read();
    if (c == '\n') {  // End of line character received
      serialInputBuff[index] = '\0'; // Null-terminate the string
      serialCommandComplete = true;
      index = 0; // Reset index for next message
    } else if (c != '\r') {
      // Only store the character if it's not a carriage return
      if (index < sizeof(serialInputBuff) - 1) {
        serialInputBuff[index++] = c;
      } else {
        // Prevent buffer overflow by not incrementing index
        // This will overwrite the last character repeatedly
        serialInputBuff[index] = c;
      }
    }
  }

  // Process the serial input
  if (serialCommandComplete) {
      inputHandle();
      serialCommandComplete = false;
  }

  // delay per sample in milli seconds: 1000ms/sec * 1sec/(500.*factor/numAverage)
  delay(1); // give some free processor time, if you increase timing to above 1000 samples per second, this delay should be 0
}

/******************************************************************************************************/
// Support functions
//
// These functions are used to allow runtime configuration of the AFE44XX through user input
// Single letters are used for commands
// Upper case commands indicate the command category and are followed by the function letter and optional number
// Lower case commands are single letter commands that can be followed by a number
/******************************************************************************************************/

// Boot helper
// Prints message and wait until timeout or user sends a character on the serial terminal
void serialTrigger(const char* mess, int timeout) {
  uint32_t startTime = millis();
  Serial.println(); Serial.println(mess);
  while ( !Serial.available() && ( (millis() - startTime) < timeout ) ) {
    delay(500);
  }
  // Clear the serial input buffer
  while (Serial.available()) { Serial.read(); }
}

// Present this text to the users if they type '?'
void helpMenu() {
  Serial.println("================================================================================");
  Serial.println("| AFE4490 Texas Instruments Analog Front End for Clinical 2CH Pulse Oximeter   |");  
  Serial.println("| 2024 Urs Utzinger                                                            |");
  Serial.println("================================================================================");
  Serial.println("| ?: help screen                        | r: dump registers                    |");
  Serial.println("| z: print sensor data                  | t: timing factor t1.0                |");
  Serial.println("| d: run diagnostics                    | p: alarm pins p0                     |");
  Serial.println("| a: apply settings                     | s: show settings                     |");
  Serial.println("| .: toggle start/stop DAQ              | e: show timers                       |");
  Serial.println("| m: set averages               m4      |                                      |");
  Serial.println("==Receiver==============================|==Transmitter==========================");
  Serial.println("| Rg1: stage2 gain LED1,        Rg1-1   | TV:  ref voltage level, TV0          |");
  Serial.println("| Rg2: stage2 gain LED2,        Rg23    | TC:  current range,     TC0          |");
  Serial.println("| RC1: feedback capacitor LED1, RC15    |======================================|");
  Serial.println("| RR1: feeback resistor LED1,   RR1200  | LP1: power level LED 1, LP1255       |");
  Serial.println("| RFC: filter edge frequency,   RFC0    | LP2: power level LED 2, LP2255       |");
  Serial.println("| RFB: ambient Current,         RFB1    |======================================|");
  Serial.println("| RC2: feedback capacitor LED2, RC25    |                                      |");
  Serial.println("| RR2: feeback resistor LED2,   RR2200  |                                      |");
  Serial.println("==Toggle================================|==Toggle===============================");
  Serial.println("| x: 1 sameGain LED1&2 on/off   x1      | x: 5 RX on/off          x5           |");
  Serial.println("| x: 2 tri state on/off         x2      | x: 6 TX on/off          x6           |");
  Serial.println("| x: 3 h bridge on/off          x3      | x: 7 AFE on/off         x7           |");
  Serial.println("| x: 4 bypass ADC on/off        x4      |                                      |");
  Serial.println("==Debug Level===========================|==Debug Level==========================");
  Serial.println("| l: 0 none                     l0      |                                      |");
  Serial.println("| l: 1 errors only              l1      |                                      |");
  Serial.println("| l: 2 warnings                 l2      |                                      |");
  Serial.println("| l: 3 info                     l3      |                                      |");
  Serial.println("| l: 4 debug                    l4      | l: 99 continous data reporting       |");
  Serial.println("================================================================================");
}

// Function to handle the input
//   - Lower case commands are single letter commands that can be followed by a number
//   - Upper case commands have sub commands and can be followed by a number
void inputHandle() {
  // Check if the input is empty
  if (serialInputBuff[0] == '\0') return;
  char command = serialInputBuff[0];
  if (isupper(command)) {
      handleUppercaseCommand(serialInputBuff);
  } else if (islower(command) || command == '?' || command == '.') {
      handleLowercaseCommand(serialInputBuff);
  } else {
      Serial.println("Unknown command");
  }
}

// Process Upper Case Commands with subcommands
void handleUppercaseCommand(const char* command) {
  long tmpI;                            // int
  int commandLen = strlen(command);     // length of command

  if (commandLen < 2) {
    snprintf(tmpStr, sizeof(tmpStr), "Command too short: %s chars. Check help with ?", command); Serial.println(tmpStr);
    return;
  }
  
  switch (command[0]) {

    // Check if first character is 'R' for Receiver Stage
    case 'R': {
      if (commandLen > 3) { // we have number
        tmpI = strtol(&command[3], NULL, 10); // string is R | X | LEADnum |value
      } else if (commandLen == 3) {
        tmpI = 0;
      } else {
        snprintf(tmpStr, sizeof(tmpStr), "Command too short: %s chars. Check help with ?", command); Serial.println(tmpStr);
        return;
      }
      switch (command[1]){
        case 'g': { // set gain
          if ( (tmpI < -1) || (tmpI>4)) {
            snprintf(tmpStr, sizeof(tmpStr), "Stage 2 gain ouside of rangem[-1..4]");
          } else {
            if (command[2] == '1') {
              afe44xx_stage2Gain1 = tmpI;
              if (afe44xx.setSecondStageGain(afe44xx_stage2Gain1, true, 1) == 0) {
                snprintf(tmpStr, sizeof(tmpStr), "Stage 2 gain LED1 set to: %d", tmpI);
              } else {
                snprintf(tmpStr, sizeof(tmpStr), "Error: Stage 2 gain LED1 could not be set to: %d", tmpI);              
              }
            } else if (command[2] == '2') {
              afe44xx_stage2Gain2 = tmpI;
              if (afe44xx.setSecondStageGain(afe44xx_stage2Gain2, true, 2) == 2) {
                snprintf(tmpStr, sizeof(tmpStr), "Stage 2 gain LED2 set to: %d", tmpI);
              } else {
                snprintf(tmpStr, sizeof(tmpStr), "Error: Stage 2 gain LED2 could not be set to: %d", tmpI);              
              
              }
            } else {
              strcpy(tmpStr, "Invalid LED number for command 'Rg'");
            }
          }
          break;
        }
        case 'C': { // feedback capacitor
          if ( (tmpI < 0) || (tmpI>275)) {
            snprintf(tmpStr, sizeof(tmpStr), "Feedback capacitor ouside of range [0..275]");
          } else {
            if (command[2] == '1') {
              afe44xx_CF1 = tmpI;
              if (afe44xx.setFirstStageFeedbackCapacitor(afe44xx_CF1,1) == 0) {
                snprintf(tmpStr, sizeof(tmpStr), "Feedback capacitor LED1 set to: %d", tmpI);
              } else {
                snprintf(tmpStr, sizeof(tmpStr), "Error: Feedback capacitor LED1 could not be set to: %d", tmpI); 
              }
            } else if (command[2] == '2') {
              afe44xx_CF2 = tmpI; 
              if (afe44xx.setFirstStageFeedbackCapacitor(afe44xx_CF2,2) == 0) {
                snprintf(tmpStr, sizeof(tmpStr), "Feedback capacitor LED2 set to: %d", tmpI);
              } else {
                snprintf(tmpStr, sizeof(tmpStr), "Error: Feedback capacitor LED2 could not be set to: %d", tmpI);
              } 
            } else {
              strcpy(tmpStr, "Invalid LED number for command 'C'");
            }
          }
          break;
        }
        case 'R': { // feedback resistor
          if ( (tmpI < -1) || (tmpI>1000)) {
            snprintf(tmpStr, sizeof(tmpStr), "Feedback resistor ouside of range [-1..1000]");
          } else {
            if (command[2] == '1') {
                afe44xx_RF1 = tmpI;
                if (afe44xx.setFirstStageFeedbackResistor(afe44xx_RF1,1)==0) {
                  snprintf(tmpStr, sizeof(tmpStr), "Feedback resistor LED1 set to: %d", tmpI);
                } else {
                  snprintf(tmpStr, sizeof(tmpStr), "Error: Feedback resistor LED1 could not be set to: %d", tmpI);
                }
            } else if (command[2] == '2') {
                afe44xx_RF2 = tmpI;
                if (afe44xx.setFirstStageFeedbackResistor(afe44xx_RF2,2)==0) {
                  snprintf(tmpStr, sizeof(tmpStr), "Feedback resistor LED2 set to: %d", tmpI);
                } else {
                  snprintf(tmpStr, sizeof(tmpStr), "Error: Feedback resistor LED2 could not be set to: %d", tmpI);
                } 
            } else {
              strcpy(tmpStr, "Invalid LED number for command 'R'");
            }
          }
          break;
        }
        case 'F': { 
          if (command[2] == 'C') { // edge frequency of low pass filter
            if ( (tmpI >= 0) && (tmpI<=1)) {
              afe44xx_Fc = tmpI;
              if (afe44xx.setFirstStageCutOffFrequency(afe44xx_Fc) == 0) {
                snprintf(tmpStr, sizeof(tmpStr), "Edge frequency set to: %d", tmpI);
              } else {
                snprintf(tmpStr, sizeof(tmpStr), "Error: Edge frequency could not be set to: %d", tmpI);
              }
            } else {
              snprintf(tmpStr, sizeof(tmpStr), "Edge frequency ouside of range [0,1]");
            }
          }
          else if (command[2] == 'B') { // ambient bias current for photodiode 
            if ( (tmpI >= 0) && (tmpI<=10)) {
              afe44xx_CAmbient=tmpI;
              if (afe44xx.setCancellationCurrent(afe44xx_CAmbient) == 0) {
                snprintf(tmpStr, sizeof(tmpStr), "Ambient background current set to: %d", tmpI);
              } else {
                snprintf(tmpStr, sizeof(tmpStr), "Error: Ambient background could not be set to: %d", tmpI);
              }
            } else {
              snprintf(tmpStr, sizeof(tmpStr), "Ambient background current ouside of range");
            }
          }
          break;
        }
        default: {
          strcpy(tmpStr, "Invalid sub-command for 'R'");
          break;
        }
      }
      break; 
    }

    // Check if first character is 'T' for Transmitter Stage
    case 'T': {
      if (commandLen > 2) {
        tmpI = strtol(&command[2], NULL, 10);
      } else if (commandLen == 2) {
        tmpI = 0;
      } else {
        snprintf(tmpStr, sizeof(tmpStr), "Command too short: %s chars. Check help with ?", command); Serial.println(tmpStr);
        return;
      }
      switch (command[1]){
        case 'V': { // reference voltage
          if ( (tmpI >= 0) && (tmpI<=3)) {
            afe44xx_txRefVoltage =  tmpI; 
            if (afe44xx.setLEDdriver(afe44xx_txRefVoltage,afe44xx_range) == 0) {
              snprintf(tmpStr, sizeof(tmpStr), "Reference voltage set to: %d", tmpI);
            } else {
              snprintf(tmpStr, sizeof(tmpStr), "Error: Reference voltage could not be set to: %d", tmpI);
            }
          } else {
            snprintf(tmpStr, sizeof(tmpStr), "Reference voltage outside of range [0..3]");
          }
          break;
        }
        case 'C':{ // current range
          if ( (tmpI >= 0) && (tmpI<=3)) {
            afe44xx_range = tmpI;
            if (afe44xx.setLEDdriver(afe44xx_txRefVoltage,afe44xx_range) == 0) {
              snprintf(tmpStr, sizeof(tmpStr), "Current range set to: %d", tmpI);
            } else {
              snprintf(tmpStr, sizeof(tmpStr), "Error: Current range could not be set to: %d", tmpI);
            }
          } else {
            snprintf(tmpStr, sizeof(tmpStr), "Current range outside of range [0..3]");
          }
          break;
        }
        default: {
          strcpy(tmpStr, "Invalid sub-command for 'T'");
          break;
        }
      }
      break;
    }

    // Check if first character is 'L' for LED power
    case 'L': {
      if (commandLen > 3) {
        tmpI = strtol(&command[3], NULL, 10);
      } else if (commandLen == 3) {
        tmpI = 0;
      } else {
        snprintf(tmpStr, sizeof(tmpStr), "Command too short: %s. Check help with ?", command); Serial.println(tmpStr);
        return;
      }
      if (command[1] == 'P') {
        if ( (tmpI < 0) || (tmpI>255)) {
          snprintf(tmpStr, sizeof(tmpStr), "LED power range outside of range [0..255]");
        } else {
          switch (command[2]) {
            case '1': { // LED1
              afe44xx_led1Power =  tmpI;
              afe44xx.setLEDpower(afe44xx_led1Power, 1);
              snprintf(tmpStr, sizeof(tmpStr), "LED1 power set to: %d", tmpI);
              break;
            }
            case '2': { // LED2
              afe44xx_led2Power =  tmpI;
              afe44xx.setLEDpower(afe44xx_led2Power, 2);
              snprintf(tmpStr, sizeof(tmpStr), "LED2 power set to: %d", tmpI);
              break;
            }
            default: {
              strcpy(tmpStr, "Invalid LED number");
              break;
            }
          }
        }
      } else {
        strcpy(tmpStr, "Invalid sub-command for 'L'");
      }
      break;
    }

    default: {
      snprintf(tmpStr, sizeof(tmpStr), "Unknown command: %s", command); Serial.println(tmpStr);
      break;
    }

  }
  Serial.println(tmpStr);

}

// Process Lower Case Commands
void handleLowercaseCommand(const char* command) {
  unsigned long tmpuI;                  // unsigned int
  float tmpF;                           // float
  bool wasRunning;
  int commandLen = strlen(command);

  if (commandLen < 1) {
      Serial.println("Command too short");
      return;
  }

  switch (command[0]) {

    case 'l': { // set debug level
      if (commandLen > 1) {
        tmpuI = strtoul(&command[1], NULL, 10);
        if (tmpuI >= 0 && tmpuI <= 4) {
          currentLogLevel = (unsigned int)tmpuI;
          afe44xx_reporting = false;
          snprintf(tmpStr, sizeof(tmpStr), "Debug level set to: %u", currentLogLevel);
        } else if (tmpuI==99) {
          currentLogLevel = 0;
          afe44xx_reporting = true;
          strcpy(tmpStr, "Reporting enabled");
        } else {
          strcpy(tmpStr, "Debug level out of valid range [0..4,99]");
        }
      } else {
        strcpy(tmpStr, "No log level provided. Check help with ?");
      }
      Serial.println(tmpStr);
      break;
    }

    case 't': { // set timing
      if (commandLen > 1) {
        tmpF = atof(&command[1]);
        if (tmpF >= 0.1 && tmpF <= 100.0) {
          afe44xx_factor = tmpF;
          if ((bool)afe44xx_running == true) {
            afe44xx_running = afe44xx.stop();
            afe44xx.setTimers(afe44xx_factor);
            afe44xx_running = afe44xx.start(afe44xx_numAverage); 
            snprintf(tmpStr, sizeof(tmpStr), "Re-started data aquisition with timing factor of %.2f and averaging of %4u", afe44xx_factor, afe44xx_numAverage);
          } else {
            afe44xx.setTimers(afe44xx_factor);
            snprintf(tmpStr, sizeof(tmpStr), "System not running, Timing factor set to: %.2f", afe44xx_factor);
          }
        } else {
          strcpy(tmpStr, "Timing factor out of valid range [0,1..100.0]");
        }
      } else {
        strcpy(tmpStr, "No timing factor provided. Check help with ?");
      }
      Serial.println(tmpStr);
      break;
    }

    case '?': // help
      helpMenu();
      break;

    case 'z': { // print sensor data
      afe44xx.getData(irRawValue, redRawValue, irAmbientValue, redAmbientValue, irValue, redValue);
      Serial.print("IR: ");
      Serial.print(irValue);
      Serial.print("\tRed: ");
      Serial.print(redRawValue);
      Serial.print("\tIR_r: ");
      Serial.print(irRawValue);
      Serial.print("\tRed_r: ");
      Serial.print(redValue);
      Serial.print("\tIR_a: ");
      Serial.print(irAmbientValue);
      Serial.print("\tRed_a: ");
      Serial.println(redAmbientValue);
      break;
    }

    case 'd': { // run diagnostics
      bool wasRunning = afe44xx_running;
      if ((bool)afe44xx_running == true) { afe44xx_running=afe44xx.stop(); }
      afe44xx.readDiag(afe44xx_diag_status);
      afe44xx.printDiag(afe44xx_diag_status);
      if (wasRunning == true) { afe44xx_running=afe44xx.start(afe44xx_numAverage); }
      break;
    }

    case 'x': { // toggle options
      if (commandLen > 1) {
        tmpuI = strtoul(&command[1], NULL, 10);
        if (tmpuI >= 0 && tmpuI <= 10) {
          switch (tmpuI){
            case 1: {
              afe44xx_sameGain = !afe44xx_sameGain;
              if (afe44xx.setSameGain(afe44xx_sameGain) == 0) {
                snprintf(tmpStr, sizeof(tmpStr), "Same gain: %s", (afe44xx_sameGain) ? "On" : "Off");
              } else {
                snprintf(tmpStr, sizeof(tmpStr), "Error: Same gain could not be set to: %s", (afe44xx_sameGain) ? "On" : "Off");
              }
              break;
            }
            case 2: {
              afe44xx_tristate = !afe44xx_tristate;
              afe44xx.setTristate(afe44xx_tristate);
              snprintf(tmpStr, sizeof(tmpStr), "Tri State: %s", (afe44xx_tristate) ? "On" : "Off");
              break;
            }
            case 3: {
              afe44xx_hbridge = !afe44xx_hbridge;
              afe44xx.setHbridge(afe44xx_hbridge);
              snprintf(tmpStr, sizeof(tmpStr), "H bridge: %s", (afe44xx_hbridge) ? "On" : "Off");
              break;
            }
            case 4: {
              afe44xx_bypassADC = !afe44xx_bypassADC;
              afe44xx.bypassADC(afe44xx_bypassADC);
              snprintf(tmpStr, sizeof(tmpStr), "Bypass ADC: %s", (afe44xx_bypassADC) ? "On" : "Off");
              break;
            }
            case 5:{
              afe44xx_powerRX = !afe44xx_powerRX;
              afe44xx.setPowerRX(afe44xx_powerRX);
              snprintf(tmpStr, sizeof(tmpStr), "Power RX: %s", (afe44xx_powerRX) ? "On" : "Off");
              break;
            }
            case 6: {
              afe44xx_powerTX = !afe44xx_powerTX;
              afe44xx.setPowerTX(afe44xx_powerTX);
              snprintf(tmpStr, sizeof(tmpStr), "Power TX: %s", (afe44xx_powerTX) ? "On" : "Off");
              break;
            }
            case 7: {
              afe44xx_powerAFE = !afe44xx_powerAFE;
              afe44xx.setPowerAFE(afe44xx_powerAFE);
              snprintf(tmpStr, sizeof(tmpStr), "Power AFE: %s", (afe44xx_powerAFE) ? "On" : "Off");
              break;
            }
            default: {
              snprintf(tmpStr, sizeof(tmpStr), "Device not available %u", tmpuI);
              break;
            }
          }
        } else {
          strcpy(tmpStr, "Toggeling option not known, need [1..7]");
        }
      } else {
        strcpy(tmpStr, "No option provided. Check help with ?");
      }
      Serial.println(tmpStr);
      break;
    }

    case 'r': { // dump registers
      afe44xx.dumpRegisters();
      break;
    }

    case 'a': { // apply settings
      bool wasRunning = afe44xx_running;
      if ((bool)afe44xx_running == true) { afe44xx_running=afe44xx.stop(); }
      afe44xx.setTimers(afe44xx_factor);                        // default timing
      afe44xx.clearCNTRL0();                                    // clear CONTROL0 register
      afe44xx.bypassADC(afe44xx_bypassADC);                     // use internal/external ADC
      afe44xx.setOscillator(afe44xx_useXTAL);                   // use external crystal or clock signal
      afe44xx.setShortDiagnostic(afe44xx_shortDiag);            // short or long diagnostic sequence
      afe44xx.setTristate(afe44xx_tristate);                    // tri state or push pull output
      afe44xx.setHbridge(afe44xx_hbridge);                      // h bridge or push pull output
      afe44xx.setAlarmpins(afe44xx_alarmpins);                  // monitor activity on PD and LED alarm pins  
      afe44xx.setPowerAFE(afe44xx_powerAFE);                    // power up/down
      afe44xx.setPowerRX(afe44xx_powerRX);                      // power up/down
      afe44xx.setPowerTX(afe44xx_powerTX);                      // power up/down
      afe44xx.setSameGain(afe44xx_sameGain);                    // same or separate settings for LED1 and LED2
      afe44xx.setFirstStageFeedbackCapacitor(afe44xx_CF1,1);    // e.g. 5pF, LED1
      afe44xx.setFirstStageFeedbackCapacitor(afe44xx_CF2,2);    // e.g. 5pF,LED2
      afe44xx.setFirstStageFeedbackResistor(afe44xx_RF1,1);     // e.g. 500kOhm LED1
      afe44xx.setFirstStageFeedbackResistor(afe44xx_RF2,2);     // e.g. 500kOhm LED1
      afe44xx.setFirstStageCutOffFrequency(afe44xx_Fc );        // 500 or 100 Hz low pass
      afe44xx.setSecondStageGain(afe44xx_stage2Gain1, true, 1); // gain enabled, on LED1
      afe44xx.setSecondStageGain(afe44xx_stage2Gain2, true, 2); // gain enabled on LED2
      afe44xx.setCancellationCurrent(afe44xx_CAmbient);         // e.g. 0 microAmp
      afe44xx.setLEDdriver(afe44xx_txRefVoltage,afe44xx_range); // e.g. 0.75V, 150mA
      afe44xx.setLEDpower(afe44xx_led1Power, 1);                // e.g. 24 of 255 on both LEDs
      afe44xx.setLEDpower(afe44xx_led2Power, 2);                // e.g. 24 of 255 on both LEDs
      if (wasRunning == true) { afe44xx_running=afe44xx.start(afe44xx_numAverage); } // number of averages and start of system
      Serial.println("Settings applied");
      break;
    }

    case 'e': { // show timers
      Serial.println("==Timer Settings================================================================");
      afe44xx.readTimers();
      Serial.println("================================================================================");
      break;
    }

    case 's': { // show settings
      Serial.println("==Settings======================================================================");
      Serial.println("===============General:");
      snprintf(tmpStr, sizeof(tmpStr), "Log Level: %u", currentLogLevel); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "Current Time: %u", currentTime); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "Timing factor: %f", afe44xx_factor); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "Number of averages: %u", afe44xx_numAverage); Serial.println(tmpStr);       
      Serial.println("===============Detection:");
      snprintf(tmpStr, sizeof(tmpStr), "Same Gain: %s", (afe44xx_sameGain) ? "On" : "Off"); Serial.println(tmpStr); 
      if (afe44xx_stage2Gain1 == -1)    { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED1: %s", "disabled"); } 
      else if(afe44xx_stage2Gain1 == 0) { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED1: %u", 1); }
      else if(afe44xx_stage2Gain1 == 1) { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED1: %f", 1.5); }
      else if(afe44xx_stage2Gain1 == 2) { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED1: %u", 2); }
      else if(afe44xx_stage2Gain1 == 3) { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED1: %u", 3); }
      else if(afe44xx_stage2Gain1 == 4) { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED1: %u", 4); }
      Serial.println(tmpStr);
      if (afe44xx_stage2Gain2 == -1)    { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED2: %s", "disabled"); } 
      else if(afe44xx_stage2Gain2 == 0) { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED2: %u", 1); }
      else if(afe44xx_stage2Gain2 == 1) { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED2: %f", 1.5); }
      else if(afe44xx_stage2Gain2 == 2) { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED2: %u", 2); }
      else if(afe44xx_stage2Gain2 == 3) { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED2: %u", 3); }
      else if(afe44xx_stage2Gain2 == 4) { snprintf(tmpStr, sizeof(tmpStr), "Stage 2 Gain LED2: %u", 4); }
      Serial.println(tmpStr);
      snprintf(tmpStr, sizeof(tmpStr), "LED1 Cf: %d pF", afe44xx_CF1); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "LED1 Rf: %d kOhm", afe44xx_RF1); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "LED2 Cf: %d pF", afe44xx_CF2); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "LED2 Rf: %d kOhm", afe44xx_RF2); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "Lowpass Edge frequency: %s Hz", (afe44xx_Fc==0) ? "500" : "1000"); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "Ambient background current: %d mu A", afe44xx_CAmbient); Serial.println(tmpStr); 
      Serial.println("===============Illumination:");
      switch (afe44xx_txRefVoltage) {
        case 0:  { snprintf(tmpStr, sizeof(tmpStr), "TX ref Voltage: %fV", 0.75); break;}
        case 1:  { snprintf(tmpStr, sizeof(tmpStr), "TX ref Voltage: %fV", 0.5);  break;}
        case 2:  { snprintf(tmpStr, sizeof(tmpStr), "TX ref Voltage: %fV", 0.75); break;}
        case 3:  { snprintf(tmpStr, sizeof(tmpStr), "TX ref Voltage: %fV", 1.0);  break;}
        default: {snprintf(tmpStr, sizeof(tmpStr), "TX ref Voltage: %fV", -1.0);  break;}
      }
      Serial.println(tmpStr); 
      if ((afe44xx_txRefVoltage == 0) || (afe44xx_txRefVoltage==2)) {
        if ((afe44xx_range == 0) || (afe44xx_range == 2) ) { snprintf(tmpStr, sizeof(tmpStr), "Current range: %d mA", 150); }
        else if (afe44xx_range == 1)                { snprintf(tmpStr, sizeof(tmpStr), "Current range: %d mA", 75); }
        else if (afe44xx_range == 3)                { snprintf(tmpStr, sizeof(tmpStr), "Current range: %s", "Off"); }
      } else if (afe44xx_txRefVoltage == 1) {
        if ((afe44xx_range == 0) || (afe44xx_range == 2) ) { snprintf(tmpStr, sizeof(tmpStr), "Current range: %d mA", 100); }
        else if (afe44xx_range == 1)                { snprintf(tmpStr, sizeof(tmpStr), "Current range: %d mA", 50); }
        else if (afe44xx_range == 3)                { snprintf(tmpStr, sizeof(tmpStr), "Current range: %s", "Off"); }
      } else if (afe44xx_txRefVoltage == 3) {
        if ((afe44xx_range == 0) || (afe44xx_range == 2) ) { snprintf(tmpStr, sizeof(tmpStr), "Current range: %d mA", 200); }
        else if (afe44xx_range == 1)                { snprintf(tmpStr, sizeof(tmpStr), "Current range: %d mA", 100); }
        else if (afe44xx_range == 3)                { snprintf(tmpStr, sizeof(tmpStr), "Current range: %s", "Off"); }
      }
      Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "LED1 power: %u", afe44xx_led1Power); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "LED1 power: %u", afe44xx_led2Power); Serial.println(tmpStr); 
      Serial.println("===============I/O:");
      snprintf(tmpStr, sizeof(tmpStr), "Tri State: %s", (afe44xx_tristate) ? "On" : "Off"); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "H Bridge: %s", (afe44xx_hbridge) ? "On" : "Off"); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "Bypass ADC: %s", (afe44xx_bypassADC) ? "On" : "Off"); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "Power RX: %s TX: %s AFE: %s", (afe44xx_powerRX) ? "On" : "Off", (afe44xx_powerTX) ? "On" : "Off", (afe44xx_powerAFE) ? "On" : "Off"); Serial.println(tmpStr); 
      snprintf(tmpStr, sizeof(tmpStr), "Alarm Pins: %u", afe44xx_alarmpins); Serial.println(tmpStr); 
      Serial.println("===============DiagStatus:");
      afe44xx.readDiag(afe44xx_diag_status);  // Run diagnostics
      afe44xx.printDiag(afe44xx_diag_status); // Print diagnostics
      Serial.println("================================================================================");
      break;
    }

    case 'p': { // alarm pins
      if (commandLen > 1) {
        tmpuI = strtoul(&command[1], NULL, 10);
        if (tmpuI >= 0 && tmpuI <= 7) {
          afe44xx_alarmpins = tmpuI;
          if (afe44xx.setAlarmpins(afe44xx_alarmpins) == 0) {
            snprintf(tmpStr, sizeof(tmpStr), "Alarm pins: %u", afe44xx_alarmpins);
          } else {
            snprintf(tmpStr, sizeof(tmpStr), "Error: Alarm pins could not be set to: %u", afe44xx_alarmpins);
          }
        } else {
          strcpy(tmpStr, "Alarm pins out of valid range [0..7]");
        }
      } else {
        strcpy(tmpStr, "No alarm pin provided. Check help with ?");
      }
      Serial.println(tmpStr);
      break;
    }

    case 'm': { // numberr of averages
      if (commandLen > 1) {
        tmpuI = strtoul(&command[1], NULL, 10);
        if (tmpuI >= 1 && tmpuI <= 16) {
          afe44xx_numAverage = tmpuI;
          if ((bool)afe44xx_running == true) {
            afe44xx_running=afe44xx.stop();
            delay(10);
            afe44xx_running=afe44xx.start(afe44xx_numAverage);
            snprintf(tmpStr, sizeof(tmpStr), "Re-started data aquistion with average of %u.", afe44xx_numAverage);  
          } else {
            snprintf(tmpStr, sizeof(tmpStr), "System not running, Number of averages set to: %u", afe44xx_numAverage);
          }
        } else {
          strcpy(tmpStr, "Number of averages out of range [1..16]");
        }
      } else {
        strcpy(tmpStr, "No number of avareages provided. Check help with ?");
      }
      Serial.println(tmpStr);
      break;
    }

    case '.': { // toggle running
      afe44xx_running = !(bool)afe44xx_running;
      if ((bool)afe44xx_running == false) {
        afe44xx_running=afe44xx.stop();
        strcpy(tmpStr, "Stopped data acquisition"); 
      } else if ((bool)afe44xx_running == true) {
        afe44xx_running=afe44xx.start(afe44xx_numAverage);
        snprintf(tmpStr, sizeof(tmpStr), "Started data aquistion with average of %u", afe44xx_numAverage);  
      }
      Serial.println(tmpStr);
      break;
    }

    default: { // unrecognized command
      snprintf(tmpStr, sizeof(tmpStr), "Command not available: %s", command); 
      Serial.println(tmpStr); 
      break;
    }
  }
}
