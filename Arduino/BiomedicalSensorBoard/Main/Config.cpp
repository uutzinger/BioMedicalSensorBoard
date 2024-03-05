/******************************************************************************************************/
// Configurations stored in EEPROM
/******************************************************************************************************/
// This helps with storing setting in the flash memory
// You can use one program and re-configure the ESP32 while it is running
//
// All commands are preceeded by ZZ. This assures that the commands are not executed by accident.
/******************************************************************************************************/
#include "CommonDefs.h"
#include "Config.h"
#include "Print.h"
#include "ESPNow.h"

#include <AudioTools.h>                 // audio input    
#include <EEPROM.h>                     // storeing and reading settings


/**************************************************************************************/
// Global Variables
/**************************************************************************************/
// EEPROM
const unsigned int eepromAddress = 0;   // EEPROM start address for saving the settings
// Settings
Settings        mySettings;             // the settings

/**************************************************************************************/
// Terminal Input: Support Routines
/**************************************************************************************/
// This is long and has many if and {}. 
// When you modify this section and forget } the compiler complains elsewhere unfortunately.

bool inputHandle() {
  unsigned long tmpuI;                  // handling unsigned int
  long          tmpI;                   // handling signed int
  float         tmpF;                   // handling float
  unsigned long tmpTime;                // time consumption of updateSYS
  bool          processSerial = false;  // do we have work to do? 
  bool          noVal = false;
  char          command[2];             // one character command plus null termination
  char          value[64];              // text following command
  char          text[64];               // text variable
  int           textlen;                // length of text variable
  int           serialInputLength;      // number of characters in the input line 
  char* endptr;                         // ptr after strtou or strtoul

  serialInputLength = strlen(serialInputBuff);
  processSerial = (serialInputLength > 2) ? true : false; // did we receive more than cr or newline?

  if (processSerial) { 
    // extract command and value
    if ( serialInputLength > 1) {       // do we have 2 or more characters
      // all commands proceeded by "ZZ"
      if (serialInputBuff[0] == 'Z' && serialInputBuff[1] == 'Z') {
        // init
        text[0] = '\0';
        textlen = 0;
        // extract single letter command
        if (serialInputLength > 2) { 
          command[0] = serialInputBuff[2]; 
          command[1] ='\0'; }         // appended null char
        if (serialInputLength > 3) { 
          strlcpy(text, serialInputBuff+3, sizeof(text)); 
          textlen = strlen(text);
        }
      } else {
        LOG_Iln("Commands need to be preceeded by ZZ");
        helpMenu();
        return false; }
    } 
    
    else {
      helpMenu();
      return false;
    }

    // If debug is enable show the received text:
    snprintf(tmpStr, sizeof(tmpStr), "Buffer : %s", serialInputBuff); 
    LOG_Dln(tmpStr);
    snprintf(tmpStr, sizeof(tmpStr), "Command : %s", command); 
    LOG_Dln(tmpStr);
    snprintf(tmpStr, sizeof(tmpStr), "Text : %s, lenght: %u", text, strlen(text)); 
    LOG_Dln(tmpStr);

    // command is first letter
    // text is the string following the command
    // value is the option provided for the command

    /////////////////////////////////////////////////////////////////////////
    // General Commands
    /////////////////////////////////////////////////////////////////////////

    if (command[0] == 'z') {                                               // dump all sensor data
      printSensors();
    }

    else if (command[0] == 'Z') {                                          // Z command can not be used to avoid confusion with ZZ
       // do not use this command
    }

    else if (command[0] == '.') {                                          // print a clear execution times of subroutines
      tmpTime = millis();
      printProfile();
      deltaUpdate = millis() - tmpTime;
      if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
    }

    else if (command[0] =='?' || command[0] =='h') {                       // help requested
      helpMenu();
    }

    else if (command[0] == 'b') {                                          // baudrate
      if (textlen > 0) { strlcpy(value, text, sizeof(value)); } else { value[0] = '\0'; } // 
      if (strlen(value) > 0) { 
        tmpuI = strtoul(value, &endptr, 10);
        if ((endptr != value) && (*endptr == '\0')) {
          if ( (tmpuI <= 500000)) {
            mySettings.baudrate = (uint32_t) tmpuI;
            snprintf(tmpStr, sizeof(tmpStr), "Baudrate is: %u", mySettings.baudrate);
          } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
        } else { snprintf(tmpStr, sizeof(tmpStr), "Baud rate needs to be between %u and %u but also depends on your ESP model.", 300, 500000); }
      } else { strlcpy(tmpStr, "Command needs to be like b500000", sizeof(tmpStr)); }
      LOG_Iln(tmpStr); yieldTime += yieldOS();
    }

    /////////////////////////////////////////////////////////////////////////
    // ADC Commands
    /////////////////////////////////////////////////////////////////////////

    else if (command[0] == 'A') {                                          // ADC settings      
      if (textlen > 0) { // we have extended command, otherwise we just provide current settings

        if (text[0] == 's') {                                              // set sampling rate
          if (textlen > 1) { strlcpy(value, text+1, sizeof(value)); } else { value[0] = '\0'; } // 
          if (strlen(value) > 0) { 
            tmpuI = strtoul(value, &endptr, 10);
            if ((endptr != value) && (*endptr == '\0')) {
              if ( (tmpuI >= SOC_ADC_SAMPLE_FREQ_THRES_LOW) && (tmpuI<= SOC_ADC_SAMPLE_FREQ_THRES_HIGH)) {
                mySettings.samplerate = (uint32_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "Sampling rate is: %u", mySettings.samplerate);
              } else { snprintf(tmpStr, sizeof(tmpStr), "Sampling rate needs to be between %u and %u.", SOC_ADC_SAMPLE_FREQ_THRES_LOW, SOC_ADC_SAMPLE_FREQ_THRES_HIGH); }
            } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
          } else { strlcpy(tmpStr, "Command needs to be like As611", sizeof(tmpStr)); }
        } // s 

        else if (text[0] == 'b') {                                         // set bit width
          if (textlen > 1) { strlcpy(value, text+1, sizeof(value)); } else { value[0] = '\0'; } // 
          if (strlen(value) > 0) { 
            tmpuI = strtoul(value, &endptr, 10);
            if ((endptr != value) && (*endptr == '\0')) {
              if ( (tmpuI>= SOC_ADC_DIGI_MIN_BITWIDTH) && (tmpuI <= SOC_ADC_DIGI_MAX_BITWIDTH) ) {
                mySettings.bitwidth = (uint8_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "Sampling bit width is: %u", mySettings.bitwidth);
              } else { snprintf(tmpStr, sizeof(tmpStr), "Sampling bit width needs to be between 9 and 12 but also depends on your ESP model."); }
            } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
          } else { strlcpy(tmpStr, "Command needs to be like Ab12", sizeof(tmpStr)); }
        } // b

        else if (text[0] == 'a') {                                            // set attenuation
          if (textlen > 1) { strlcpy(value, text+1, sizeof(value)); } else { value[0] = '\0'; } // 
          if (strlen(value) > 0) { 
            tmpuI = strtoul(value, &endptr, 10);
            if ((endptr != value) && (*endptr == '\0')) {
              switch ((uint8_t) tmpuI) {
                case ADC_ATTEN_DB_0:
                  mySettings.atten = ADC_ATTEN_DB_0;                
                  snprintf(tmpStr, sizeof(tmpStr), "Sampling attenuation is: %u", ADC_ATTEN_DB_0);
                  break;
                case ADC_ATTEN_DB_2_5:
                  mySettings.atten = ADC_ATTEN_DB_2_5;                
                  snprintf(tmpStr, sizeof(tmpStr), "Sampling attenuation is: %u", ADC_ATTEN_DB_2_5);
                  break;
                case ADC_ATTEN_DB_6:
                  mySettings.atten = ADC_ATTEN_DB_6;                
                  snprintf(tmpStr, sizeof(tmpStr), "Sampling attenuation is: %u", ADC_ATTEN_DB_6);
                  break;
                case ADC_ATTEN_DB_11:
                  mySettings.atten = ADC_ATTEN_DB_11;                
                  snprintf(tmpStr, sizeof(tmpStr), "Sampling attenuation is: %u", ADC_ATTEN_DB_11);
                  break;
                default:
                  snprintf(tmpStr, sizeof(tmpStr), "Sampling attenuation needs to be between %u and %u.", ADC_ATTEN_DB_0, ADC_ATTEN_DB_11 );
                  break;
              } // switch
            } else { strlcpy(tmpStr, "No value found or no null character in string", sizeof(tmpStr)); }
          } else { strlcpy(tmpStr, "Command should be similar to Aa1", sizeof(tmpStr)); }
        } // a

        else if (text[0] == 'v') {                                         // set averaging length
          if (textlen > 1) { strlcpy(value, text+1, sizeof(value)); } else { value[0] = '\0'; } // 
          if (strlen(value) > 0) { 
            tmpuI = strtoul(value, &endptr, 10);
            if ((endptr != value) && (*endptr == '\0')) {
              if ( (tmpuI >=1) && (tmpuI <= MAX_AVG_LEN) ) {
                mySettings.avglen = (uint8_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "Sampling averaging length is: %u", mySettings.avglen);
              } else { snprintf(tmpStr, sizeof(tmpStr), "Sampling averaging needs to be between 1 and 128.", sizeof(tmpStr)); }
            } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
          } else { strlcpy(tmpStr, "Command should be similar to Av8", sizeof(tmpStr)); }
        } // v

        else if (text[0] == 'n') {                                         // set number of channels
          if (textlen > 1) { strlcpy(value, text+1, sizeof(value)); } else { value[0] = '\0'; } // 
          if (strlen(value) > 0) { 
            tmpuI = strtoul(value, &endptr, 10);
            if ((endptr != value) && (*endptr == '\0')) {
              if (tmpuI <= 6 ) {
                mySettings.channels = (uint8_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "Number of sampling channels is: %u", mySettings.channels);
              } else { snprintf(tmpStr, sizeof(tmpStr), "Sampling channels needs to be between 1 and 6."); }
            } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
          } else { strlcpy(tmpStr, "Command should be similar to An1", sizeof(tmpStr)); }
        } // n

        else if (text[0] == 'c') {                                         // set channel pin
          if (textlen > 2) { strlcpy(value, text+2, sizeof(value)); } else { value[0] = '\0'; } // 
          if (strlen(value) > 0) { 
            tmpuI = strtoul(value, &endptr, 10);
            if ((endptr != value) && (*endptr == '\0')) {
              if        ( text[1] == '0') {
                mySettings.channel0 = (adc_channel_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "ADC channel 0 is: %u", mySettings.channel0);
              } else if ( text[1] == '1' ) {
                mySettings.channel1 = (adc_channel_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "ADC channel 1 is: %u", mySettings.channel1);
              } else if ( text[1] == '2' ) {
                mySettings.channel2 = (adc_channel_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "ADC channel 2 is: %u", mySettings.channel2);
              } else if ( text[1] == '3' ) {
                mySettings.channel3 = (adc_channel_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "ADC channel 3 is: %u", mySettings.channel3);
              } else if ( text[1] == '4' ) {
                mySettings.channel4 = (adc_channel_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "ADC channel 4 is: %u", mySettings.channel4);
              } else if ( text[1] == '5' ) {
                mySettings.channel5 = (adc_channel_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "ADC channel 5 is: %u", mySettings.channel5);
              } else {
                strlcpy(tmpStr, "Selected ADC channel needs to be 0..5", sizeof(tmpStr));
              }
            } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
          } else { strlcpy(tmpStr, "Command should be similar Ac112 to set channel 1 to 12", sizeof(tmpStr)); }
        } //c

        else { strlcpy(tmpStr, "Not a valid option given", sizeof(tmpStr)); }
      } 
      
      else { strlcpy(tmpStr, "Not an option after A", sizeof(tmpStr)); }

      LOG_Iln(tmpStr); yieldTime += yieldOS();

    } // end ADC

    /////////////////////////////////////////////////////////////////////////
    // Thermistor Commands
    /////////////////////////////////////////////////////////////////////////

    else if (command[0] == 'R') {                                          // Thermistor settings      
      if (textlen > 0) { // options was given
        if (textlen > 1) { 
          strlcpy(value, text+1, sizeof(value)); 
          noVal = false;
          tmpuI = strtoul(value, &endptr, 10);
          if ((endptr == value) || (*endptr != '\0')) { noVal = true; }
          tmpF = strtof(value, &endptr);
          if ((endptr == value) || (*endptr != '\0')) { noVal = true;}
        } 
        else { value[0] = '\0'; } // value was given

        if (noVal) { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }

        else if (text[0] == '1') { 
          if ((tmpuI > 0) && (tmpuI < 20000)) {
            mySettings.R1 = tmpuI;
            snprintf(tmpStr, sizeof(tmpStr), "R1 set to: %u", mySettings.R1);
          }
          else { strlcpy(tmpStr, "Value for R1 out of range: 0..20,000", sizeof(tmpStr)); }
        }

        else if (text[0] == '2') { 
          if ((tmpuI > 0) && (tmpuI < 20000)) {
            mySettings.R2 = tmpuI;
            snprintf(tmpStr, sizeof(tmpStr), "R2 set to: %u", mySettings.R2);
          }
          else { strlcpy(tmpStr, "Value for R2 out of range: 0..20,000", sizeof(tmpStr)); }
        }

        else if (text[0] == '3') { 
          if ((tmpuI > 0) && (tmpuI < 20000)) {
            mySettings.R3 = tmpuI;
            snprintf(tmpStr, sizeof(tmpStr), "R3 set to: %u", mySettings.R3);
          }
          else { strlcpy(tmpStr, "Value for R3 out of range: 0..20,000", sizeof(tmpStr)); }
        }

        else if (text[0] == 'A') { 
          if ((tmpF > 0.0) && (tmpF < 1.0)) {
            mySettings.A = tmpF;
            snprintf(tmpStr, sizeof(tmpStr), "A set to: %f", mySettings.A);
          }
          else { strlcpy(tmpStr, "Value for A out of range: 0..1.0", sizeof(tmpStr)); }
        }

        else if (text[0] == 'B') { 
          if ((tmpF > 0.0) && (tmpF < 1.0)) {
            mySettings.B = tmpF;
            snprintf(tmpStr, sizeof(tmpStr), "B set to: %f", mySettings.B);
          }
          else { strlcpy(tmpStr, "Value for B out of range: 0..1.0", sizeof(tmpStr)); }
        }

        else if (text[0] == 'C') {
          if ((tmpF > 0.0) && (tmpF < 1.0)) {
            mySettings.C = tmpF;
            snprintf(tmpStr, sizeof(tmpStr), "C set to: %f", mySettings.C);
          }
          else { strlcpy(tmpStr, "Value for C out of range: 0..1.0", sizeof(tmpStr)); }
        }

        else if (text[0] == 'V') {
          if ((tmpI > 3000) && (tmpI < 3500)) {
            mySettings.Vin = tmpI;
            snprintf(tmpStr, sizeof(tmpStr), "Vin set to: %u", mySettings.Vin);
          }
          else { strlcpy(tmpStr, "Value for Vin out of range: 3000..3500", sizeof(tmpStr)); }
        }

        else if (text[0] == 'T') {
          if ((tmpI > 0) && (tmpI <= 64)) {
            mySettings.Tavglen = tmpI;
            snprintf(tmpStr, sizeof(tmpStr), "Averaging set to: %u", mySettings.Tavglen);
          }
          else { strlcpy(tmpStr, "Value for Temp averaging out of range: 1..64", sizeof(tmpStr)); }
        }

      }

      else { strlcpy(tmpStr, "No valid option provided", sizeof(tmpStr));} 

      LOG_I(tmpStr); yieldTime += yieldOS();       
    } // end Temperature

    /////////////////////////////////////////////////////////////////////////
    // ESPNow Commands
    /////////////////////////////////////////////////////////////////////////

    else if (command[0] == 'W') {                                          // ESP settings      
      if (textlen > 0) { // we have extended command, otherwise we want to know WiFi status and intervals
        if (textlen >= 2) { 
          strlcpy(value, text+1, sizeof(value));
          
          if (text[0] == 'b') {                                            // set broadcast address
            if (textlen == 18) {                                           // make sure complete address given
              parseMACAddress(value, mySettings.broadcastAddress);
              snprintf(tmpStr, sizeof(tmpStr), "Broadcast address is: %X:%X:%X:%X:%X:%X", 
                          mySettings.broadcastAddress[0], mySettings.broadcastAddress[1], mySettings.broadcastAddress[2], 
                          mySettings.broadcastAddress[3], mySettings.broadcastAddress[4], mySettings.broadcastAddress[5]); 
            }
            else { strlcpy(tmpStr, "MAC address must look like EF.A1.F9.C8.D5.B7", sizeof(tmpStr)); }
          } 
        
          else if (text[0] == 'c') {                                       // set ESP channel
            if (strlen(value) > 0) { 
              tmpuI = strtoul(value, &endptr, 10); 
              if ((endptr != value) && (*endptr == '\0')) {
                if ((tmpuI >= 0) && (tmpuI <= 99)) {
                  mySettings.ESPNowChannel = (uint8_t)tmpuI;
                  snprintf(tmpStr, sizeof(tmpStr), "ESPNow channel set to: %u", mySettings.ESPNowChannel); 
                } 
                else { strlcpy(tmpStr, "ESPNow channel out of valid range 0 - 99", sizeof(tmpStr)); } 
              } 
              else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
            } 
            else { strlcpy(tmpStr, "Command must look like Wc1", sizeof(tmpStr)); }
          }   

          else if (text[0] == 'e') {                                       // set encryption
            if (strlen(value) > 0) { 
              tmpuI = strtoul(value, &endptr, 10);
              if ((endptr != value) && (*endptr == '\0')) {
                if ((tmpuI >= 0) && (tmpuI <= 1)) {
                  mySettings.ESPNowEncrypt = (uint8_t)tmpuI;
                  snprintf(tmpStr, sizeof(tmpStr), "ESPNow encryption set to: %u", mySettings.ESPNowEncrypt); 
                } 
                else { strlcpy(tmpStr, "ESPNow encryption out of valid range 0 or 1", sizeof(tmpStr)); } 
              } 
              else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
            } 
            else { strlcpy(tmpStr, "Command must look like We1", sizeof(tmpStr));}
          }

          else { strlcpy(tmpStr, "No valid option", sizeof(tmpStr)); }

        } 

        else { strlcpy(tmpStr, "Need two or more characters after W", sizeof(tmpStr));  }
      }
      
      else { snprintf(tmpStr, sizeof(tmpStr), "Hostname %s", hostName); }

      LOG_Iln(tmpStr); yieldTime += yieldOS(); 
    } // end ESPNow

    /////////////////////////////////////////////////////////////////////////
    // OLED
    /////////////////////////////////////////////////////////////////////////

    else if (command[0] == 'L') {      
      if (textlen > 0) { // options was given
        tmpuI = strtoul(text, &endptr, 10);
        if ((endptr != value) && (*endptr == '\0')) {
          if ((tmpuI >= 0) && (tmpuI < 255)) {
            mySettings.OLEDDisplay=tmpuI;
            snprintf(tmpStr, sizeof(tmpStr), "OLED display type set to: %u", mySettings.OLEDDisplay); 
          } else { strlcpy(tmpStr, "OLED display type of valid range 0 .. 255", sizeof(tmpStr)); } 
        } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
        LOG_Iln(tmpStr); yieldTime += yieldOS();

      } else { helpMenu(); }

    } // end L

    /////////////////////////////////////////////////////////////////////////
    // Enable / Disable Functions
    /////////////////////////////////////////////////////////////////////////

    else if (command[0] == 'x') {                                          // enable/disable equipment
      if (textlen > 0) { // options was given
        tmpuI = strtoul(text, &endptr, 10);
        if ((endptr != value) && (*endptr == '\0')) {
          if ((tmpuI > 0) && (tmpuI < 100)) {
            switch (tmpuI) {
              case 1:
                mySettings.useOLED = !bool(mySettings.useOLED);
                if (mySettings.useOLED)   {  LOG_Iln("OLED is used");    }     else { LOG_Iln("OLED is not used"); }
                break;
              case 2:
                mySettings.useESPNow = !bool(mySettings.useESPNow);
                if (mySettings.useESPNow)  { LOG_Iln("ESPNow is used"); }      else { LOG_Iln("ESPNow is not used"); }
                break;
              case 3:
                mySettings.useECG = !bool(mySettings.useECG);
                if (mySettings.useECG) {     LOG_Iln("ECG is used");  }        else { LOG_Iln("ECG is not used"); }
                if (mySettings.useECG) {
                  if (mySettings.useSound) {
                    mySettings.useSound = false;
                    LOG_Iln("Turned Sound off. Cannot use ECG and Sound at the same time");
                  }
                  if (mySettings.useTemperature) {
                    mySettings.useTemperature = false;
                    LOG_Iln("Turned Temperature off. Cannot use ECG and Temperature at the same time");
                  }
                }
                break;
              case 4:
                mySettings.useBattery = !bool(mySettings.useBattery);
                if (mySettings.useBattery) { LOG_Iln("Battery is used");  }    else { LOG_Iln("Battery is not used"); }
                break;
              case 5:
                mySettings.useSerial = !bool(mySettings.useSerial);
                if (mySettings.useSerial) {  LOG_Iln("Serial is used");  }     else { LOG_Iln("Serial is not used"); }
                break;
              case 6:
                mySettings.useBinary = !bool(mySettings.useBinary);
                if (mySettings.useBinary) { LOG_Iln("Binary transmission is used"); } else { LOG_Iln("Text transmission is used"); }
                break;
              case 7:
                mySettings.useTemperature = !bool(mySettings.useTemperature);
                if (mySettings.useTemperature) { LOG_Iln("Thermistor is used"); } else { LOG_Iln("Thermistor is not used"); }
                if (mySettings.useTemperature) {
                  if (mySettings.useSound) {
                    mySettings.useSound = false;
                    LOG_Iln("Turned Sound off. Cannot use Thermistor and Sound at the same time");
                  }
                  if (mySettings.useECG) {
                    mySettings.useECG = false;
                    LOG_Iln("Turned ECG off. Cannot use ECG and Temperature at the same time");
                  }
                }
                break;
              case 8:
                mySettings.useSound = !bool(mySettings.useSound);
                if (mySettings.useSound) { LOG_Iln("Sound is used"); } else { LOG_Iln("Sound is not used"); }
                if (mySettings.useSound) {
                  if (mySettings.useECG) {
                    mySettings.useECG = false;
                    LOG_Iln("Turned ECG off. Cannot use ECG and Sound at the same time");
                  }
                  if (mySettings.useTemperature) {
                    mySettings.useTemperature = false;
                    LOG_Iln("Turned Temperature off. Cannot use Temperature and Sound at the same time");
                  }
                }
                break;
              case 9:
                mySettings.useImpedance = !bool(mySettings.useImpedance);
                if (mySettings.useImpedance) { LOG_Iln("Impedance is used"); } else { LOG_Iln("Impedance is not used"); }
                break;
              case 10:
                mySettings.useOx = !bool(mySettings.useOx);
                if (mySettings.useOx) { LOG_Iln("Pulseoximeter is used"); } else { LOG_Iln("Pulseoximeter is not used"); }
                break;
              case 11:
                mySettings.useBaseStation = !bool(mySettings.useBaseStation);
                if (mySettings.useBaseStation) { LOG_Iln("Base Station is used"); } else { LOG_Iln("Base Station is not used"); }
                if (mySettings.useBaseStation) {
                  if (mySettings.useECG) {
                    mySettings.useECG = false;
                    LOG_Iln("Turned ECG off. Cannot use ECG and Base Station at the same time");
                  }
                  if (mySettings.useTemperature) {
                    mySettings.useTemperature = false;
                    LOG_Iln("Turned Temperature off. Cannot use Temperature and Base Station at the same time");
                  }
                  if (mySettings.useSound) {
                    mySettings.useSound = false;
                    LOG_Iln("Turned Sound off. Cannot use Sound and Base Station at the same time");
                  }
                  if (mySettings.useOx) {
                    mySettings.useOx = false;
                    LOG_Iln("Turned Pulse Oximeter off. Cannot use Pulse Oximeter and Base Station at the same time");
                  }
                  if (mySettings.useImpedance) {
                    mySettings.useImpedance = false;
                    LOG_Iln("Turned Impedance off. Cannot use Impedance and Base Station at the same time");
                  }
                }
                break;
              case 99:
                LOG_Iln("ESP is going to restart ...");
                Serial.flush();
                ESP.restart();
              default:
                LOG_I("Invalid device");
                break;
            } // end switch
          } // valid option
        } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
      } else { LOG_I("No valid device provided"); }
      yieldTime += yieldOS(); 
    } // end turning on/off sensors

    /////////////////////////////////////////////////////////////////////////
    // Set Debug Levels
    /////////////////////////////////////////////////////////////////////////

    else if (command[0] == 'l') {                                          // set debug level
      if (textlen > 0) {
        tmpuI = strtoul(text, &endptr, 10);
        if ((endptr != value) && (*endptr == '\0')) {        
          if ((tmpuI >= 0) && (tmpuI <= 99)) {
            mySettings.debuglevel = (unsigned int)tmpuI;
            snprintf(tmpStr, sizeof(tmpStr), "Debug level set to: %u", mySettings.debuglevel);  
          } else { strcpy_P(tmpStr, "Debug level out of valid Range"); }
        } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
      } else { strlcpy(tmpStr, "No debug level provided", sizeof(tmpStr)); }
      LOG_Iln(tmpStr);
      yieldTime += yieldOS(); 
    }

    /////////////////////////////////////////////////////////////////////////
    // Settings
    /////////////////////////////////////////////////////////////////////////

    else if (command[0] == 's') {                                          // print Settings
      printSettings();
    }

    else if (command[0] == 'd') {                                          // default Settings
      if (textlen > 0) {
        if (text[0] == 'd') {
          defaultSettings();
          yieldTime += yieldOS(); 
          printSettings(); 
        }
      }
    }

    else if (command[0] == 'F') {
      if (textlen > 0) { 

        if      (text[0] == 's') {                                         // save
          tmpTime = millis();
          LOG_D("D:S:EEPRM..");
          EEPROM.begin(EEPROM_SIZE);
          EEPROM.put(0, mySettings);
          if (EEPROM.commit()) { snprintf(tmpStr, sizeof(tmpStr), "Settings saved to EEPROM in: %dms", millis() - tmpTime); } // takes 400ms
          else {strcpy(tmpStr, "EEPROM failed to commit");}
          LOG_Iln(tmpStr);
          yieldTime += yieldOS();
        } 

        else if (text[0] == 'r') {                                         // read
          tmpTime = millis();
          LOG_D("D:R:EEPRM..");
          EEPROM.begin(EEPROM_SIZE);
          EEPROM.get(eepromAddress, mySettings);
          snprintf(tmpStr, sizeof(tmpStr), "Settings read from eeprom in: %dms", millis() - tmpTime); 
          LOG_Iln(tmpStr);
          yieldTime += yieldOS(); 
          printSettings();

        } else { LOG_Iln("Sub-command not implemented"); }

      } 
    } // end EEPROM


    else {                                                                   // unrecognized command
      snprintf(tmpStr, sizeof(tmpStr), "Command not available: %s", command);
      LOG_Iln(tmpStr);
      LOG_Iln("Use: ZZ? for help");
      yieldTime += yieldOS(); 
    } // end if
    
    processSerial  = false;
    return true;
  } // end process of input occurred
  
  else { 
    processSerial  = false;
    return false; 
  } // end no process of input occurred
  
} // end input handle

void parseMACAddress(const char* str, uint8_t* mac) {
  char* endptr;
  for (int i = 0; i < 6; i++) {
    mac[i] = (uint8_t) strtol(str, &endptr, 16);
    if (i < 5) {
      if (!((*endptr == '.') || (*endptr == ':'))) {
        if (mySettings.debuglevel > 0) { LOG_Iln("Invalid MAC address format"); }
        return;
      }
      str = endptr + 1;
    }
  }
}


// Help Menu
// Expand if needed
void helpMenu() {  
  LOG_Dln("D:HELP:..");
  LOG_Iln(doubleSeparator); yieldTime += yieldOS(); 
  LOG_Iln(doubleSeparator);
  snprintf(tmpStr, sizeof(tmpStr), "ECG, 2023-2024, Team 24052                                                 %s",  VER); 
  LOG_Iln(tmpStr);
  LOG_Iln(doubleSeparator); 
  LOG_Iln("Supports........................................................................"); 
  LOG_Iln("....................................................................OLED SSD1306"); 
  LOG_Iln("........................................................................MAX1704X"); 
  LOG_Iln(".........................................................................ESP ADC"); 
  LOG_Iln("==General======================================================================="); 
  LOG_Iln("| All commands preceeded by ZZ                                                 |"); 
  LOG_Iln(singleSeparator);
  LOG_Iln("| ?:  help screen                       | .:  execution times                  |"); 
  LOG_Iln("| z:  print sensor data                 | b: set baudrate                      |"); 
  LOG_Iln("| s:  print current settings            | dd: default settings                 |");
  LOG_Iln("| Fs: save settings to EEPROM           | Fr: read settings from EEPROM        |"); yieldTime += yieldOS(); 
  LOG_Iln("==ADC===================================|=======================================");
  LOG_Iln("| As: sampling rate                     | Aa: attenuation                      |"); 
  LOG_Iln("| An: number of channels                | Av: average length                   |"); 
  LOG_Iln("| Ab: bitwidth                          |                                      |"); 
  LOG_Iln("| Ac0: channel 0  Ac012                 | Ac3: channel 3                       |"); 
  LOG_Iln("| Ac1: channel 1                        | Ac4: channel 4                       |"); 
  LOG_Iln("| Ac2: channel 2                        | Ac5: channel 5                       |"); yieldTime += yieldOS(); 
  LOG_Iln("==Temperature===========================|=======================================");
  LOG_Iln("| R1: R1 bridge                         | RA: A coeff                          |"); 
  LOG_Iln("| R2: R2 bridge                         | RB: B coeff                          |"); 
  LOG_Iln("| R3: R3 bridge                         | RC: C coeff                          |");  
  LOG_Iln("| RV: Vin                               | RT: Averaging                        |"); yieldTime += yieldOS();  
  LOG_Iln("==ESPNow================================|=======================================");
  LOG_Iln("| W:  print hostname                    | Wc: channel Wc0                      |"); 
  LOG_Iln("| Wb: set broadcast address             | We: encrypt We0 0/off 1/on           |"); 
  LOG_Iln("|     WbE1.A2.F3.C4.D5.B6               |                                      |"); 
  LOG_Iln("==OLED==================================|======================================="); 
  LOG_Iln("| L: display layout 1..2 L1               1:ECG 2:BATT                         |"); yieldTime += yieldOS(); 
  LOG_Iln("==Disable===============================|==Disable=============================="); 
  LOG_Iln("| x: 1  OLED on/off                     | x: 6 Binary on serial on/off         |"); 
  LOG_Iln("| x: 2  ESPNow on/off                   | x: 7 Thermistor on/off               |"); 
  LOG_Iln("| x: 3  ECG on/off                      | x: 8 Sound on/off                    |"); 
  LOG_Iln("| x: 4  Battery on/off                  | x: 9 Impedance on/off                |"); 
  LOG_Iln("| x: 5  Serial on/off                   | x: 10 Pulseoxi on/off                |");   
  LOG_Iln("| x: 11 Base Station on/off             | x: 99 Reboot                         |"); yieldTime += yieldOS();  
  LOG_Iln("==Debug Level===========================|==Debug Level=========================="); 
  LOG_Iln("| l: 0 ALL off                          | l: 3                                 |"); 
  LOG_Iln("| l: 1 Errors only                      | l: 4                                 |"); 
  LOG_Iln("| l: 2 Debug                            | l: 5                                 |"); 
  LOG_Iln("| l: 99 continuous                      |                                      |"); 
  LOG_Iln(doubleSeparator);
  LOG_Iln("| You might need to reboot with x99 to initialize the sensors                  |");
  LOG_Iln(doubleSeparator);
  yieldTime += yieldOS(); 
}

// create default settings
void defaultSettings() {
  LOG_Dln("D:SET:..");
  strlcpy(mySettings.identifier, "ECG", sizeof(mySettings.identifier)); 
  mySettings.runTime                      = 0;                      // cumulative runtime of software
  mySettings.debuglevel                   = 2;                      // set reporting level
  mySettings.useOLED                      = false;                  // enable/disable display
  mySettings.useESPNow                    = true;                   // enable/disable ESPNow
  mySettings.useSerial                    = true;                   // provide Serial interface on USB
  mySettings.useBattery                   = true;                   // battery
  mySettings.useECG                       = false;                   // ecg
  mySettings.useBinary                    = false;                  // binary for serial terminssion
  mySettings.useTemperature               = false;                  // temperature
  mySettings.useSound                     = false;                  // sound, stethoscope
  mySettings.useOx                        = false;                  // pulseoximeter
  mySettings.useImpedance                 = false;                  // bio impedance
  mySettings.useBaseStation               = false;                  // base station function
  mySettings.temp1                        = false;                  // ... not yet used
  mySettings.temp2                        = false;                  // ... not yet used
  mySettings.temp3                        = false;                  // ... not yet used
  mySettings.temp4                        = false;                  // ... not yet used
  mySettings.temp5                        = false;                  // ... not yet used
  mySettings.broadcastAddress[0]          = 0xdc;                   // MAC
  mySettings.broadcastAddress[1]          = 0x54;                   //
  mySettings.broadcastAddress[2]          = 0x75;                   //
  mySettings.broadcastAddress[3]          = 0xcf;                   //
  mySettings.broadcastAddress[4]          = 0x9f;                   //
  mySettings.broadcastAddress[5]          = 0x0c;                   //
  mySettings.ESPNowChannel                = 0;                      //
  mySettings.ESPNowEncrypt                = 0;                      //
  mySettings.baudrate                     = 500000;                 // serial communication rate
  mySettings.samplerate                   = 44100;                  // Analog to digital converter sampling rage
  mySettings.channels                     = 1;                      // Number of channels to sample 1..6
  mySettings.bitwidth                     = 12;                     // Number of bits for ADC
  mySettings.atten                        = ADC_ATTEN_DB_11;        // Range of input signal
  mySettings.avglen                       = 8;                      // Down sampling by averaging avglen measurements 
  mySettings.channel0                     = ADC_CHANNEL_3 ;         // Sparkfun Thing Plus CH3 A3, Adafruit S3 CH4 D5
  mySettings.channel1                     = ADC_CHANNEL_0;          // 
  mySettings.channel2                     = ADC_CHANNEL_1;          // 
  mySettings.channel3                     = ADC_CHANNEL_2;          // 
  mySettings.channel4                     = ADC_CHANNEL_4;          // 
  mySettings.channel5                     = ADC_CHANNEL_5;          //
  mySettings.OLEDDisplay                  = 1;                      // Display type 1
  mySettings.R1                           = 9820;                   // R1 of Wheatstone bridge
  mySettings.R2                           = 9897;                   //
  mySettings.R3                           = 9860;                   //
  mySettings.Vin                          = 3000;                   // Source voltage on the bridge
  mySettings.A                            = 0.001111285528;         // Thermistor constant A 0.001111285538
  mySettings.B                            = 0.000237121596;         // B 0.0002371215953 
  mySettings.C                            = 7.520677e-08;           // C 0.00000007520676806 
  mySettings.Tavglen                      = 32;                     // Further averaging for better accuracy
} 

// Print settings
void printSettings() {
  LOG_Iln();
  LOG_Iln(doubleSeparator);
  snprintf(tmpStr, sizeof(tmpStr), "Identifier: %s Version: %s",   mySettings.identifier, VER ); 
  LOG_Iln(tmpStr);
  LOG_Iln("-System-----------------------------------------"); yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "Debug level: .................. %u",   mySettings.debuglevel ); 
  LOG_Iln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Runtime [min]: ................ %lu",  mySettings.runTime / 60); 
  LOG_Iln(tmpStr); 
  LOG_Iln("-Network----------------------------------------"); yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "ESPNow: ....................... %s",  (mySettings.useESPNow) ? mON : mOFF); 
  LOG_Iln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Broadcast Address: ............ %X:%X:%X:%X:%X:%X", mySettings.broadcastAddress[0],mySettings.broadcastAddress[1],
                                                                                        mySettings.broadcastAddress[2],mySettings.broadcastAddress[3],
                                                                                        mySettings.broadcastAddress[4],mySettings.broadcastAddress[5]);  
  LOG_Iln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Encryption: ................... %u",  mySettings.ESPNowEncrypt); 
  LOG_Iln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Channel: ...................... %u",  mySettings.ESPNowChannel); 
  LOG_Iln(tmpStr); yieldTime += yieldOS(); 
  
  LOG_Iln("-Serial-----------------------------------------");
  snprintf(tmpStr, sizeof(tmpStr), "Serial: ....................... %s",  mySettings.useSerial   ? mON : mOFF); 
  LOG_Iln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Serial Baudtrate: ............. %u",  mySettings.baudrate); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Binary: ....................... %s",  mySettings.useBinary   ? mON : mOFF); 
  LOG_Iln(tmpStr);
  LOG_Iln("-Battery----------------------------------------");
  snprintf(tmpStr, sizeof(tmpStr), "Battery: ...................... %s",  mySettings.useBattery  ? mON : mOFF); 
  LOG_Iln(tmpStr);
  LOG_Iln("-ADC--------------------------------------------");  yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "ECG: .......................... %s",  mySettings.useECG      ? mON : mOFF); 
  snprintf(tmpStr, sizeof(tmpStr), "Samplerate: ................... %u",  mySettings.samplerate); 
  LOG_Iln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "Channels: ..................... %u",  mySettings.channels); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Bitwidth: ..................... %u",  mySettings.bitwidth); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Attenuation: .................. %u",  mySettings.atten); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Averages: ..................... %u",  mySettings.avglen); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 0: .................... %u",  mySettings.channel0); 
  LOG_Iln(tmpStr);  yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 1: .................... %u",  mySettings.channel1); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 2: .................... %u",  mySettings.channel2); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 3: .................... %u",  mySettings.channel3); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 4: .................... %u",  mySettings.channel4); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Channel 5: .................... %u",  mySettings.channel5); 
  LOG_Iln(tmpStr); 
  LOG_Iln("-ON/OFF------------------------------------------");  yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "ECG: .......................... %s",  mySettings.useECG         ? mON : mOFF); 
  snprintf(tmpStr, sizeof(tmpStr), "Temperature: .................. %s",  mySettings.useTemperature ? mON : mOFF); 
  snprintf(tmpStr, sizeof(tmpStr), "Sound: ........................ %s",  mySettings.useSound       ? mON : mOFF); 
  snprintf(tmpStr, sizeof(tmpStr), "Pulseoximeter: ................ %s",  mySettings.useOx          ? mON : mOFF); 
  snprintf(tmpStr, sizeof(tmpStr), "Impedance: .................... %s",  mySettings.useImpedance   ? mON : mOFF); 
  snprintf(tmpStr, sizeof(tmpStr), "Base Station: ................. %s",  mySettings.useBaseStation ? mON : mOFF); 
  LOG_Iln("-Temperature-------------------------------------");  yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "R1: ........................... %u",  mySettings.R1); 
  LOG_Iln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "R2: ........................... %u",  mySettings.R2); 
  LOG_Iln(tmpStr);
  snprintf(tmpStr, sizeof(tmpStr), "R3: ........................... %u",  mySettings.R3); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "A: ............................ %f",  mySettings.A); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "B: ............................ %f",  mySettings.B); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "C: ............................ %f",  mySettings.C); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Vin: .......................... %u",  mySettings.Vin); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Temp averaging: ............... %u",  mySettings.Tavglen); 
  LOG_Iln(tmpStr); 
  LOG_Iln("-OLED-------------------------------------------"); yieldTime += yieldOS(); 
  snprintf(tmpStr, sizeof(tmpStr), "OLED: ......................... %s",  mySettings.useOLED     ? mON : mOFF); 
  LOG_Iln(tmpStr); 
  snprintf(tmpStr, sizeof(tmpStr), "Display type: ................. %u",  mySettings.OLEDDisplay);  
  LOG_Iln(tmpStr); 
  LOG_Iln(doubleSeparator); 
  yieldTime += yieldOS(); 
}
