/******************************************************************************************************/
// OLED
/******************************************************************************************************/
// This is not fully implemented
// Make sure OLED is update infrequently

#include "OLED.h"
#include "ECG.h"
#include "Print.h"
#include "Config.h"

bool oled_avail = false;
uint8_t displayType;

extern float     batteryPercent;
extern float     batteryChargerate;
extern Settings  mySettings; 
extern Adafruit_SSD1306 display;

bool initializeOLED() {
  bool ok;

  D_printSerial("D:U:OLED:IN..");
  // create display voltage from internal 3.3V
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    if (mySettings.debuglevel > 0) { R_printSerialln("OLED: SSD1306 allocation failed"); }
    ok = false;
    oled_avail = false;
  } 
  else {
    // clear the display
    display.clearDisplay();
    //Set the color - always use white despite actual display color
    display.setTextColor(WHITE);
    //Set the font size
    display.setTextSize(1.3);
    //Set the cursor coordinates
    display.setCursor(0,0);
    display.print("ECG initialized...");
    display.display();
    if (mySettings.debuglevel > 1) { R_printSerialln("OLED: initialized"); }
    oled_avail = true;
    ok = true;
  }
  return ok;
} 

bool updateECGDisplay() {
  bool ok = false;
  // This needs to be developed
  if (mySettings.debuglevel > 1) { R_printSerialln("OLED: updated"); }
  return ok;
} 

bool updateBatteryDisplay() {
  bool ok = false;
  // This needs to be developed
  if (mySettings.debuglevel > 1) { R_printSerialln("OLED: updated"); }
  return ok;
} 

