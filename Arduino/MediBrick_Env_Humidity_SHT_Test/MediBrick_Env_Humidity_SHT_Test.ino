#include <Arduino.h>
#include <Wire.h>
#include <logger.h>
#include "SHTSensor.h"

SHTSensor sht;

// Timings
unsigned long currentTime;
unsigned long lastSHTTime;
const unsigned long shtInterval = 1000;
#define SHT_I2CFrequency 400000

void setup() {

  Serial.begin(115200);
  while (!Serial){}

  Wire.begin();
  Wire.setClock(SHT_I2CFrequency);
  if (sht.init(Wire)){
    LOGI("SHT Initialized")
  } else {
    LOGE("Error: SHT initialization failed")
    while(1){delay(50);}
  }

  if (sht.setAccuracy(SHTSensor::SHT_ACCURACY_HIGH)){
    LOGI("SHT accuracy set to: %s", "high");
  } else {
    LOGE("Error: SHT settign accuracy failed");
    while(1){delay(50);}
  }
}

void loop() {

  currentTime = millis();

  if ((currentTime - lastSHTTime) >= shtInterval) {
    lastSHTTime = currentTime;
    if (sht.readSample()) {
        Serial.print(" rH: ");
        Serial.print(sht.getHumidity(), 2);
        Serial.print("\t");
        Serial.print("T: ");
        Serial.println(sht.getTemperature(), 2);
    } else {
        LOGE("Error reading sample")
    }

  }

  // low power mode
  // sht.cleanup();
}
