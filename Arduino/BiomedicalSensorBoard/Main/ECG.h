/******************************************************************************************************/
// ECG
/******************************************************************************************************/
#ifndef ECG_H_
#define ECG_H_
#include <Arduino.h>

/******************************************************************************************************/
// ECG Functions
/******************************************************************************************************/
bool initializeECG();
bool updateECG();

/******************************************************************************************************/
// ECG Globals
/******************************************************************************************************/
extern bool ecg_avail;                           // ECG is working
extern int stateLOMinus;                         // are the electrodes attached
extern int stateLOPlus;                          // are the electrodes attached

#endif