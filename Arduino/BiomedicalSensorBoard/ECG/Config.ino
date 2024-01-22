/******************************************************************************************************/
// Configuration stored in EEPROM
/******************************************************************************************************/
// This helps with storing setting in the flash memory
// You can use one program and re configure the ESP32 while it is runnig over the serial port
// 
#include "Settings.h"
#include "Config.h"
#include "Print.h"

const unsigned int   eepromAddress = 0;                      // EEPROM start address for saving the settings
Settings             mySettings;                             // the settings

extern bool          serialReceived;                         // keep track of serial input
extern unsigned long lastSerialInput;                        // keep track of serial flood
extern char          serialInputBuff[64];                    // serial input buffer

/**************************************************************************************/
// Terminal Input: Support Routines
/**************************************************************************************/
// This is long and has many if and {}. 
// When you modify this section and forget } the compiler complains elsewhere unfortunately

bool inputHandle() {
  unsigned long tmpuI;                  // handling unsinged int
  long          tmpI;                   // handling signed int
  float         tmpF;                   // handling float
  bool          processSerial = false;  // do we have work to do? 
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
    if ( serialInputLength > 1) { // do we have 2 or more characters
      // all commands proceeded by "ZZ"
      if (serialInputBuff[0] == 'Z' && serialInputBuff[1] == 'Z') {
        // init
        text[0] = '\0';
        textlen = 0;
        // extract single letter command
        if (serialInputLength > 2) { 
          command[0] = serialInputBuff[2]; 
          command[1] ='\0'; } // appended null char
        if (serialInputLength > 3) { 
          strlcpy(text, serialInputBuff+3, sizeof(text)); 
          textlen = strlen(text);
        }
      } else {
        R_printSerialln("Commands need to be preceeded by ZZ");
        helpMenu();
        return false; }
    } 
    
    else {
      helpMenu();
      return false;
    }

    // If debug is enable show the received text:
    // R_printSerial("Buffer: "); R_printSerialln(serialInputBuff);
    // R_printSerial("Command: "); R_printSerialln(command);
    // R_printSerial("Text: "); R_printSerialln(text);
    // snprintf(tmpStr, sizeof(tmpStr), "Text length : %u", strlen(text)); 
    // R_printSerialln(tmpStr);

    // command is first letter
    // text is the string following the command
    // value is the option provided for the command

    ///////////////////////////////////////////////////////////////////
    // General Commands
    ///////////////////////////////////////////////////////////////////

    if (command[0] == 'z') {                                                 // dump all sensor data
      printSensors();
    }

    else if (command[0] == '.') {                                            // print a clear execution times of subroutines
      tmpTime = millis();
      printProfile();
      deltaUpdate = millis() - tmpTime;
      if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
    }

    else if (command[0] =='?' || command[0] =='h') {                         // help requested
      helpMenu();
    }

    else if (command[0] == 'b') {                                            // baudrate
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
      R_printSerialln(tmpStr); yieldTime += yieldOS();
    }

    ///////////////////////////////////////////////////////////////////
    // ADC Commands
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'A') {                                        // ADC settings      
      if (textlen > 0) { // we have extended command, otherwise we just provide current settings

        if (text[0] == 's') {                                            // set sampling rate
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

        else if (text[0] == 'b') {                                            // set bit width
          if (textlen > 1) { strlcpy(value, text+1, sizeof(value)); } else { value[0] = '\0'; } // 
          if (strlen(value) > 0) { 
            tmpuI = strtoul(value, &endptr, 10);
            if ((endptr != value) && (*endptr == '\0')) {
              if ( (tmpuI>= SOC_ADC_DIGI_MIN_BITWIDTH) && (tmpuI <= SOC_ADC_DIGI_MAX_BITWIDTH) ) {
                mySettings.bitwidth = (uint8_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "Sampling bithwidth is: %u", mySettings.bitwidth);
              } else { snprintf(tmpStr, sizeof(tmpStr), "Sampling bithwidth needs to be between 9 and 12 but also depends on your ESP model."); }
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
                  snprintf(tmpStr, sizeof(tmpStr), "Sampling attenduation needs to be between %u and %u.", ADC_ATTEN_DB_0, ADC_ATTEN_DB_11 );
                  break;
              } // switch
            } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
          } else { strlcpy(tmpStr, "Command needs to be like Aa1", sizeof(tmpStr)); }
        } // a

        else if (text[0] == 'v') {                                            // set averaging length
          if (textlen > 1) { strlcpy(value, text+1, sizeof(value)); } else { value[0] = '\0'; } // 
          if (strlen(value) > 0) { 
            tmpuI = strtoul(value, &endptr, 10);
            if ((endptr != value) && (*endptr == '\0')) {
              if ( (tmpuI >=1) && (tmpuI <= MAX_AVG_LEN) ) {
                mySettings.avglen = (uint8_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "Sampling averaging length is: %u", mySettings.avglen);
              } else { snprintf(tmpStr, sizeof(tmpStr), "Sampling averaging needs to be between 1 and 128.", sizeof(tmpStr)); }
            } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
          } else { strlcpy(tmpStr, "Command needs to be like Av8", sizeof(tmpStr)); }
        } // v

        else if (text[0] == 'n') {                                            // set number of channels
          if (textlen > 1) { strlcpy(value, text+1, sizeof(value)); } else { value[0] = '\0'; } // 
          if (strlen(value) > 0) { 
            tmpuI = strtoul(value, &endptr, 10);
            if ((endptr != value) && (*endptr == '\0')) {
              if (tmpuI <= 6 ) {
                mySettings.channels = (uint8_t) tmpuI;
                snprintf(tmpStr, sizeof(tmpStr), "Number of sampling channels is: %u", mySettings.channels);
              } else { snprintf(tmpStr, sizeof(tmpStr), "Sampling channels needs to be between 1 and 6."); }
            } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
          } else { strlcpy(tmpStr, "Command needs to be like An1", sizeof(tmpStr)); }
        } // n

        else if (text[0] == 'c') {                                            // set channel pin
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
          } else { strlcpy(tmpStr, "Command needs to be like Ac112 to set channel 1 to 12", sizeof(tmpStr)); }
        } //c

        else { strlcpy(tmpStr, "Not a valid option given", sizeof(tmpStr)); }
      } 
      
      else { strlcpy(tmpStr, "Not an option after A", sizeof(tmpStr)); }

      R_printSerialln(tmpStr); yieldTime += yieldOS();

    } // end ADC

    ///////////////////////////////////////////////////////////////////
    // ESPNow Commands
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'W') {                                          // ESP settings      
      if (textlen > 0) { // we have extended command, otherwise we want to know WiFi status and intervals
        if (textlen >= 2) { 
          strlcpy(value, text+1, sizeof(value));
          
          if (text[0] == 'b') {                                            // set broadcast address
            if (textlen == 18) {                                           // make sure complete address given
              parseMACAddress(value, mySettings.broadcastAddress);
              snprintf(tmpStr, sizeof(tmpStr), "Boardcast address is: %X:%X:%X:%X:%X:%X", 
                          mySettings.broadcastAddress[0], mySettings.broadcastAddress[1], mySettings.broadcastAddress[2], 
                          mySettings.broadcastAddress[3], mySettings.broadcastAddress[4], mySettings.broadcastAddress[5]); 
            }
            else { strlcpy(tmpStr, "MAC # must look like E1.A2.F3.C4.D5.B6", sizeof(tmpStr)); }
          } 
        
          else if (text[0] == 'c') {                                      // set ESP channel
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

          else if (text[0] == 'e') {                                    // set encryption
            if (strlen(value) > 0) { 
              tmpuI = strtoul(value, &endptr, 10);
              if ((endptr != value) && (*endptr == '\0')) {
                if ((tmpuI >= 0) && (tmpuI <= 1)) {
                  mySettings.ESPNowEncrypt = (uint8_t)tmpuI;
                  snprintf(tmpStr, sizeof(tmpStr), "ESPNow encryption set to: %u", mySettings.ESPNowEncrypt); 
                } 
                else { strlcpy(tmpStr, "ESPNow ecnryption out of valid range 0 or 1", sizeof(tmpStr)); } 
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

      R_printSerialln(tmpStr); yieldTime += yieldOS(); 
    } // end ESPNow

    ///////////////////////////////////////////////////////////////////
    // OLED
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'L') {      
      if (textlen > 0) { // options was given
        tmpuI = strtoul(text, &endptr, 10);
        if ((endptr != value) && (*endptr == '\0')) {
          if ((tmpuI >= 0) && (tmpuI < 255)) {
            mySettings.OLEDDisplay=tmpuI;
            snprintf(tmpStr, sizeof(tmpStr), "OLED display type set to: %u", mySettings.OLEDDisplay); 
          } else { strlcpy(tmpStr, "OLED display type of valid range 0 .. 255", sizeof(tmpStr)); } 
        } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
        R_printSerialln(tmpStr); yieldTime += yieldOS();

      } else { helpMenu(); }

    } // end L

    ///////////////////////////////////////////////////////////////////
    // Enable / Disable Functions
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'x') {                                          // enable/disable equipment
      if (textlen > 0) { // options was given
        tmpuI = strtoul(text, &endptr, 10);
        if ((endptr != value) && (*endptr == '\0')) {
          if ((tmpuI > 0) && (tmpuI < 100)) {
            switch (tmpuI) {
              case 1:
                mySettings.useOLED = !bool(mySettings.useOLED);
                if (mySettings.useOLED)   {  R_printSerialln("OLED is used");    }     else { R_printSerialln("OLED is not used"); }
                break;
              case 2:
                mySettings.useESPNow = !bool(mySettings.useESPNow);
                if (mySettings.useESPNow)  { R_printSerialln("ESPNow is used"); }      else { R_printSerialln("ESPNow is not used"); }
                break;
              case 3:
                mySettings.useECG = !bool(mySettings.useECG);
                if (mySettings.useECG) {     R_printSerialln("ECG is used");  }        else { R_printSerialln("ECG is not used"); }
                break;
              case 4:
                mySettings.useBattery = !bool(mySettings.useBattery);
                if (mySettings.useBattery) { R_printSerialln("Battery is used");  }    else { R_printSerialln("Battery is not used"); }
                break;
              case 5:
                mySettings.useSerial = !bool(mySettings.useSerial);
                if (mySettings.useSerial) {  R_printSerialln("Serial is used");  }     else { R_printSerialln("Serial is not used"); }
                break;
              case 6:
                mySettings.useBinary = !bool(mySettings.useBinary);
                if (mySettings.useBinary) { R_printSerialln("Binary transmission is used"); } else { R_printSerialln("Text transmission is used"); }
                break;
              case 99:
                R_printSerialln("ESP is going to restart ...");
                Serial.flush();
                ESP.restart();
              default:
                R_printSerial("Invalid device");
                break;
            } // end switch
          } // valid option
        } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
      } else { R_printSerial("No valid device provided"); }
      yieldTime += yieldOS(); 
    } // end turning on/off sensors

    ///////////////////////////////////////////////////////////////////
    // Set Debug Levels
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'l') {                                            // set debug level
      if (textlen > 0) {
        tmpuI = strtoul(text, &endptr, 10);
        if ((endptr != value) && (*endptr == '\0')) {        
          if ((tmpuI >= 0) && (tmpuI <= 99)) {
            mySettings.debuglevel = (unsigned int)tmpuI;
            snprintf(tmpStr, sizeof(tmpStr), "Debug level set to: %u", mySettings.debuglevel);  
          } else { strcpy_P(tmpStr, "Debug level out of valid Range"); }
        } else { strlcpy(tmpStr, "No value found or no null char", sizeof(tmpStr)); }
      } else { strlcpy(tmpStr, "No debug level provided", sizeof(tmpStr)); }
      R_printSerialln(tmpStr);
      yieldTime += yieldOS(); 
    }

    ///////////////////////////////////////////////////////////////////
    // Settings
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 's') {                                            // print Settings
      printSettings();
    }

    else if (command[0] == 'd') {                                            // default Settings
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

        if      (text[0] == 's') {                                                // save
          tmpTime = millis();
          D_printSerial("D:S:EPRM..");
          EEPROM.begin(EEPROM_SIZE);
          EEPROM.put(0, mySettings);
          if (EEPROM.commit()) { snprintf(tmpStr, sizeof(tmpStr), "Settings saved to EEPROM in: %dms", millis() - tmpTime); } // takes 400ms
          else {strcpy(tmpStr, "EEPROM failed to commit");}
          R_printSerialln(tmpStr);
          yieldTime += yieldOS();
        } 

        else if (text[0] == 'r') {                                         // read
          tmpTime = millis();
          D_printSerial("D:R:EPRM..");
          EEPROM.begin(EEPROM_SIZE);
          EEPROM.get(eepromAddress, mySettings);
          snprintf(tmpStr, sizeof(tmpStr), "Settings read from eeprom in: %dms", millis() - tmpTime); 
          R_printSerialln(tmpStr);
          yieldTime += yieldOS(); 
          printSettings();

        } else { R_printSerialln("Sub-command not implemented"); }

      } 
    } // end EEPROM


    else {                                                                   // unrecognized command
      R_printSerial("Command not available: ");
      R_printSerialln(command);
      R_printSerialln("Use: ZZ? for help");
      yieldTime += yieldOS(); 
    } // end if
    
    processSerial  = false;
    return true;
  } // end process of input occured
  
  else { 
    processSerial  = false;
    return false; 
  } // end no process of input occured
  
} // end input handle

void parseMACAddress(const char* str, uint8_t* mac) {
  char* endptr;
  for (int i = 0; i < 6; i++) {
    mac[i] = (uint8_t) strtol(str, &endptr, 16);
    if (i < 5) {
      if (!((*endptr == '.') || (*endptr == ':'))) {
        if (mySettings.debuglevel > 0) { R_printSerialln("Invalid MAC address format"); }
        return;
      }
      str = endptr + 1;
    }
  }
}
