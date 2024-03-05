/******************************************************************************************************/
// Battery
/******************************************************************************************************/
#include "Battery.h"
#include "Print.h"
#include "Config.h"

bool battery_avail = false;                          // do ww have battery
float batteryPercent;
float batteryChargerate;

extern Adafruit_MAX17048 maxlipo;
extern Settings mySettings;
extern TwoWire myWire;

bool initializeBattery() {
  bool ok = false;
  battery_avail = false;
  D_printSerial("D:U:BAT:IN..");
  if (!maxlipo.begin(&myWire)) {
    if (mySettings.debuglevel > 0) { R_printSerialln("Battery: Couldn't find Adafruit MAX17048"); }
  } else {
    // maxlipo.getChipID();
    // maxlipo.getResetVoltage();
    ok = true;
    battery_avail = true;
    if (mySettings.debuglevel > 1) { R_printSerialln("Battery: initialized"); }
  }
  return ok;
}

bool updateBattery() {
  batteryPercent    = maxlipo.cellPercent(); // percent charge
  batteryChargerate = maxlipo.chargeRate();  // percent charge per hour
  if (mySettings.debuglevel > 1) { R_printSerialln("Battery: updated"); }
  return true;
}

