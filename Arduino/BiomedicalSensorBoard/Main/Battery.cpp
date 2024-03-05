/******************************************************************************************************/
// Battery
/******************************************************************************************************/
#include "CommonDefs.h"
#include "Battery.h"
#include "Print.h"
#include "Config.h"

bool battery_avail;                             // we have battery electronics
float batteryPercent;
float batteryChargerate;
SFE_MAX1704X maxlipo(MAX1704X_MAX17048);        // Create a MAX17048

bool initializeBattery() {
  bool ok;
  LOG_Dln("D:I:BAT:..");

  if (!maxlipo.begin(myWire)) {
    if (mySettings.debuglevel > 0) { LOG_Iln("Battery: Couldn't find MAX17048"); }
    ok = false;
  } else {
    maxlipo.quickStart();                       // Initialize the MAX17048 registers
    maxlipo.setThreshold(20);                   // Set alert threshold to 20%.
    uint8_t maxlipoID = maxlipo.getID();        // get the ID of the chip
    ok = true;
    battery_avail = true;
    if (mySettings.debuglevel > 1) { LOG_Iln("Battery: initialized"); }
  }

  return ok;
}

bool updateBattery() {
  bool ok;
  LOG_Dln("D:U:BAT:..");

  batteryPercent    = maxlipo.getSOC();         // percent charge
  batteryChargerate = maxlipo.getChangeRate();  // percent charge per hour
  if ( maxlipo.isVoltageLow() > 0 ) {
    if (mySettings.debuglevel > 0) { LOG_Iln("Battery: Voltage LOW"); }
  }
  if (mySettings.debuglevel > 1) { LOG_Iln("Battery: updated"); }
  ok = true;

  return ok;
}

