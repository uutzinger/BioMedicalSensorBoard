/******************************************************************************************************/
// Temperature
/******************************************************************************************************/

#include "CommonDefs.h"
#include "Temperature.h"
#include "Print.h"
#include "Config.h"
#include "ESPNow.h"
#include "ADC.h"

bool updateTemp() {
  int16_t *sample_in;
  int16_t *sample_out;
  uint16_t R4;
  int32_t  tempSum;
  bool ok = false;
  LOG_Dln("D:U:TEMP:..");

  if (buffer_full) {                                                 // We have a new set of data
    buffer_full = false;                                             // reset the buffer flag
    uint16_t avg_temp_samples = avg_samples_per_channel/mySettings.Tavglen; // we want to further average the samples for better temperature accuracy

    // compute Voltage Difference between two channels
    sample_in  =  (int16_t*) buffer_in;                              // reset sample pointer (input)
    sample_out =  (int16_t*) buffer_out;                             // reset result pointer (output)
    for(uint16_t n=0; n<avg_samples_per_channel; n++){
      *sample_out++ = *sample_in++ - *sample_in++;                   // V diff is computed
    }

    // average Voltage Difference
    // the voltage difference is placed to the buffer_out
    // we will store the average in the same buffer.
    sample_in  = (int16_t*) buffer_out;                             // reset sample pointer (input)
    sample_out = (int16_t*) buffer_out;                             // reset result pointer (output)
    for (uint16_t n=1; n<avg_temp_samples ; n++){
      tempSum = *sample_in++;                                       // initialize the sum
      for (uint16_t o=1; o<mySettings.Tavglen ; o++){               // loop over number of average
          tempSum += *sample_in++;                                  // sum the values
      }
      *sample_out++ = (tempSum/int32_t(mySettings.Tavglen));        // average and increment output pointer
    }

    // convert Voltage Difference to Temperaturte
    // the average voltage difference is in the buffer_out
    // we will store the temperature in the same buffer
    sample_in  = (int16_t*) buffer_out;                             // reset sample pointer (input)
    sample_out = (int16_t*) buffer_out;                             // reset result pointer (output)
    for(uint16_t i=0; i<avg_temp_samples; i++){
      // this is optimized to use integer math
      int32_t   Vdiff = (int32_t) *sample_in++;
      // R_Thermistor = ( R3 * (Vin*R2 - Vdiff*(R1+R2)) ) / (Vin*R1 + Vdiff*(R1+R2)) // Wikipedia Wheatstone bridge formula
      // We need to use uint64_t because resistors are 10,000 and Vin is 3,300 resulting in numbers larger than 32 bits
      int32_t R4 = int32_t ( ( uint64_t(mySettings.R3) * uint64_t(mySettings.Vin*mySettings.R2 - Vdiff*(mySettings.R1+mySettings.R2)) ) / uint64_t(mySettings.Vin*mySettings.R1 + Vdiff*(mySettings.R1+mySettings.R2)) );
      int16_t Temp = calctemp(R4);
      *sample_out++ = Temp;
    }
    
    // populate the ESPNow payload
    myESPNowDataOut.sensorID = (uint8_t) SENSORID_TEMP;                      // ID for sensor, e.g. ECG, Sound, Temp etc.
    myESPNowDataOut.numData = (uint8_t) avg_temp_samples;                    // Number of samples per channel, is computed after every ADC read update
    myESPNowDataOut.numChannels = 1;                                         // Number of channels
    memcpy(myESPNowDataOut.dataArray, buffer_out, avg_temp_samples * 1 * BYTES_PER_SAMPLE); // copy the data from the ADC buffer to ESPNow structure
    myESPNowDataOut.checksum = 0;                                                  // We want to exclude the checksum field when caclulating the checksum 
    const byte* bytePointer = reinterpret_cast<const byte*>(&myESPNowDataOut);
    myESPNowDataOut.checksum = calculateChecksum(bytePointer, sizeof(myESPNowDataOut));
    if (esp_now_send(mySettings.broadcastAddress, (uint8_t *)&myESPNowDataOut, sizeof(myESPNowDataOut)) != ESP_OK) { // send the data
        if (mySettings.debuglevel > 0) { LOG_Iln("ESPNow: Error sending data"); }
    } 
    else {
        if (mySettings.debuglevel > 1) { LOG_Iln("ESPNow: data sent"); }
        ok = true;
    }
  }

  return ok;
} 

int16_t calctemp(int32_t R4) {
  LOG_Dln("D:U:TEMP:FLOAT:..");
  // Steinhart-Hart Equation
  float lnR = log(float(R4)); // natural logarithm
  float Temp = 1.0/(mySettings.A + mySettings.B * lnR + mySettings.C * (lnR*lnR*lnR));
  Temp = (Temp-273.15);       // Kelvin to Centigrade
  return int16_t(Temp*100.0); // convert temperature e.g. 29.12 to 2912
}