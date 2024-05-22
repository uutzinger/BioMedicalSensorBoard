#include <Arduino.h>
#include <math.h>
#include <esp_now.h>
#include <WiFi.h>
// Include Wire Library for I2C
#include <Wire.h>
// Include Adafruit Graphics & OLED libraries
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_MAX1704X.h"
/////////////////////////////////

int ThermVb = A0;
int ThermVd = A1;
float Vb;
float Vd;
float Vin = 3290;
float Vdiff;
float R1 = 9820;
float R2 = 9897;
float R3 = 9860;
float R4;
float Temps;
float A = 0.001111285538;
float B = 0.0002371215953;
float C = 0.00000007520676806;

int i = 0;
float Temptot = 0;
float Tempave = 0;

/////////////////////////////////

uint8_t broadcastAddress[] = {0xDC, 0x54, 0x75, 0xC3, 0xBE, 0xFC};

typedef struct Temp{
  double Temp;
} Temp;
 Temp TempData;

esp_now_peer_info_t peerInfo;

//calibration input from master 
typedef struct TempR{
   unsigned int R1;
   unsigned int R2;
   unsigned int R3;
} TempR;

TempR TempCali;
//icons for oled
const unsigned char Battery_100 [] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xF0, 0x40, 0x00, 0x00, 0x10, 0x47, 0x3C, 0xE7, 0x18,
0x47, 0xBD, 0xE7, 0x98, 0x45, 0xA5, 0xA5, 0x9E, 0x45, 0xA5, 0xA5, 0x9A, 0x45, 0xA5, 0xA5, 0x9A,
0x45, 0xA5, 0xA5, 0x9A, 0x45, 0xA5, 0xA5, 0x9A, 0x45, 0xA5, 0xA5, 0x9E, 0x47, 0xBD, 0xE7, 0x98,
0x47, 0x3C, 0xE7, 0x18, 0x40, 0x00, 0x00, 0x10, 0x3F, 0xFF, 0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char temprature [] = {
0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x01, 0xF8, 0x00, 0x01, 0x08, 0x00, 0x03, 0x0C, 0x00, 0x03,
0x6D, 0xC0, 0x03, 0x6C, 0x00, 0x03, 0x6D, 0xE0, 0x03, 0x69, 0xE0, 0x03, 0x60, 0x00, 0x03, 0x61,
0xC0, 0x03, 0x60, 0x00, 0x03, 0x6D, 0xE0, 0x03, 0x6D, 0xE0, 0x03, 0x6C, 0x00, 0x06, 0xF6, 0x00,
0x05, 0x9A, 0x00, 0x05, 0x9A, 0x00, 0x05, 0x92, 0x00, 0x06, 0xF6, 0x00, 0x03, 0x0C, 0x00, 0x01,
0xF8, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 
};


// Reset pin not used but needed for library
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_RESET);
 

Adafruit_MAX17048 maxlipo;


//debounce of button
// Constants for debounce
const int BUTTON_PIN = 10;
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long RESET_DURATION = 5000; // 5 seconds
volatile int buttonState = HIGH;
volatile int lastButtonState = HIGH;
volatile unsigned long lastDebounceTime = 0;
volatile unsigned long buttonPressStartTime = 0;
volatile bool resetTriggered = false;
unsigned long lastDisplayUpdateTime = 0;
const unsigned long displayUpdateInterval = 1000; //# of iterations before oled update
unsigned long  lastTempCalcTime = 0 ;
const unsigned long TEMP_CALC_INTERVAL = 15;

//define diferent states for oled
typedef enum stateDsiplay {Bat, Data}
stateType;

volatile stateType oledDisplay = Bat;

//interupt func for different states of oled when button is pressed 
void handleButtonPress() {
  unsigned long currentMillis = millis();

  // Check if enough time has passed since the last button press
  if (currentMillis - lastDebounceTime > DEBOUNCE_DELAY) {
    int reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonState) {
      lastDebounceTime = currentMillis;
      lastButtonState = reading;

      if (lastButtonState == HIGH) {
        // Button pressed, start/reset the timer
        buttonPressStartTime = currentMillis;
        resetTriggered = false;
      } else {
        // Button released, reset timer and trigger reset if held for 5 seconds
        buttonPressStartTime = 0;
        resetTriggered = false;
      }

      // Move this part outside the inner if statement
      if (lastButtonState == HIGH) {
        // Button pressed, toggle OLED display state
        if (oledDisplay == Bat) {
          oledDisplay = Data;
        } else {
          oledDisplay = Bat;
        }
      }
    }
  }
}

float calctemp(float R4) {
  Temps = 1/(A + B * log(R4) + C * pow(log(R4), 3));
  Temps = (Temps-273.15);
  return Temps;
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Callback function executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&TempCali, incomingData, sizeof(TempCali));
 R1 = TempCali.R1;
 R2 = TempCali.R2;
 R3 = TempCali.R3;
}
/////////////////////////////////
void updateDisplay() {
 display.clearDisplay();
    // switch case for oled state
     switch (oledDisplay)
  {
    //display battery precent and charge discharge rate
  case Bat:
      
     display.setTextSize(1.3);
  display.drawBitmap(0,4,Battery_100,32,32,1);
  display.setCursor(42,0); 
  display.print("ChgR:");display.print(maxlipo.chargeRate()); display.print(" %/hr");
  display.setCursor(40,15);
  display.print("Bat:");display.print(maxlipo.cellPercent(), 1); display.print(" %");
  
    break;
  //display temp
  case Data:
  display.setTextSize(1.3);
  display.drawBitmap(0,4,temprature,24,24,1);
  display.setCursor(42,0); 
  display.print("Temp:");display.print(Tempave);display.print(" *C");
  display.setCursor(42,7);
  display.setTextSize(0.5);
  display.print("R1:");display.print(R1);
  display.setCursor(42,16);
  display.print("R2:");display.print(R2);
 display.setCursor(42,26);
  display.print("R3:");display.print(R3);
    break;
  }
  display.display();

}
/////////////////////////////////

void setup() {
  Serial.begin(9600);
  delay(500);

/////////////////////////////////
 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  // Register callback function
  esp_now_register_recv_cb(OnDataRecv);
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

/////////////////////////////////

//INIT  oled button 
 pinMode(10,INPUT);
attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, CHANGE);
esp_sleep_enable_ext0_wakeup(GPIO_NUM_10,1); //wake up pin if module is in deep sleep 
  // Start Wire library for I2C
  Wire.begin();
  // initialize OLED with I2C addr 0x3C
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    // Clear the display
  display.clearDisplay();
  //Set the color - always use white despite actual display color
  display.setTextColor(WHITE);
  //Set the font size
  display.setTextSize(1.5);
  //Set the cursor coordinates
  display.setCursor(0,0);
  display.print("Temp Sensor Config...");
  display.display();
 delay(1000);
  //check if max module is present if not send error
  if (!maxlipo.begin()) {
    Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    while (1) delay(10);
  }
}

void loop() {


//reset button
  unsigned long currentMillis = millis();

  // Check if the button is being held down for 5 seconds
  if (buttonPressStartTime != 0 && !resetTriggered && currentMillis - buttonPressStartTime >= RESET_DURATION) {
    // Button held for 5 seconds, trigger reset
    resetTriggered = true;
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Entering DeepSleep");
    display.println("GOOD NIGHT");
    display.display();
    delay(5000);
    display.clearDisplay();
    display.display();
    esp_deep_sleep_start();
  }
  
// Check if 10 milliseconds have passed since the last temperature calculation
  if (currentMillis - lastTempCalcTime >= TEMP_CALC_INTERVAL) {
    // Perform temperature calculation
    Vb = analogReadMilliVolts(ThermVb);
    Vd = analogReadMilliVolts(ThermVd);

    Vdiff = Vb - Vd;
    R4 = R3 / (R2 / (R1 + R2) - Vdiff / Vin) - R3;

    Temps = calctemp(R4);

    if (i == 100) {
      Tempave = Temptot / i;
      Tempave = Tempave * 100;
      Tempave = round(Tempave);
      Tempave = Tempave / 100;
      i = 0;
      Temptot = 0;
      Serial.println(Tempave);

      TempData.Temp = Tempave;
    }

    i = i + 1;
    Temptot = Temptot + Temps;

    // Update the last temperature calculation time
    lastTempCalcTime = currentMillis;
  }

    // Check for display update based on time
  if (currentMillis - lastDisplayUpdateTime >= displayUpdateInterval) {
    updateDisplay();
    lastDisplayUpdateTime = currentMillis;
  }
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &TempData, sizeof(TempData));
    if (result == ESP_OK) {
      Serial.println("Sent with success");
  }
    else {
      Serial.println("Error sending the data");
  }
  
}
