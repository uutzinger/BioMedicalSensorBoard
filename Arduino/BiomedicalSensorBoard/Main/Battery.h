/******************************************************************************************************/
// Battery
/******************************************************************************************************/
#ifndef BATTERY_H_
#define BATTERY_H_
#include <Arduino.h>
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h> 
// Click here to get the library: http://librarymanager/All#SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library

/******************************************************************************************************/
// Battery functions
/******************************************************************************************************/
bool initializeBattery();
bool updateBattery();

/******************************************************************************************************/
// Battery globals
/******************************************************************************************************/
extern bool           battery_avail;                      // we have battery electronics
extern float          batteryPercent;
extern float          batteryChargerate;
extern SFE_MAX1704X   maxlipo;

#endif