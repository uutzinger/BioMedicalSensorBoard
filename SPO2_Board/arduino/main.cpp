#include <Arduino.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>
#include "protocentral_afe44xx.h"
// Include Wire Library for I2C
#include <Wire.h>
// Include Adafruit Graphics & OLED libraries
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_MAX1704X.h"
// MAC Address of responder - edit as required
uint8_t broadcastAddress[] = {0xDC, 0x54, 0x75, 0xC3, 0xBE, 0xFC};
// Must match the receiver structure
typedef struct OXI {
  uint16_t IR;  // Use uint16_t for IR to save space
  uint16_t RED; // Use uint16_t for RED to save space
  uint8_t BPM;
  uint8_t SPO2;
} OXI;

// Create a structured object
OXI MediData;
esp_now_peer_info_t peerInfo;
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
const unsigned char Heart_Pulse [] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xF8, 0x1F, 0xE0,
0x1F, 0xFC, 0x3F, 0xF8, 0x3F, 0xFE, 0x7F, 0xFC, 0x3F, 0xFF, 0xFF, 0xFC, 0x7F, 0xFF, 0xFF, 0xFE,
0x7F, 0xFF, 0xFF, 0xFE, 0xFF, 0xC7, 0xFF, 0xFF, 0xFF, 0xC7, 0xFF, 0xFF, 0xFF, 0x83, 0xE7, 0xFF,
0xFF, 0x83, 0xC3, 0xFF, 0xFF, 0x83, 0xC3, 0xFF, 0xFF, 0x01, 0x81, 0xFF, 0x00, 0x11, 0x80, 0x00,
0x00, 0x18, 0x00, 0x00, 0x00, 0x78, 0x18, 0x00, 0x1F, 0xF8, 0x1F, 0xF8, 0x0F, 0xFC, 0x3F, 0xF0,
0x07, 0xFC, 0x3F, 0xE0, 0x03, 0xFE, 0x7F, 0xC0, 0x01, 0xFF, 0xFF, 0x80, 0x00, 0xFF, 0xFF, 0x00,
0x00, 0x7F, 0xFE, 0x00, 0x00, 0x3F, 0xFC, 0x00, 0x00, 0x1F, 0xF8, 0x00, 0x00, 0x0F, 0xF0, 0x00,
0x00, 0x07, 0xE0, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


#define AFE44XX_CS_PIN   6
#define AFE44XX_DRDY_PIN 13
#define AFE44XX_PWDN_PIN 5

// Reset pin not used but needed for library
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_RESET);
 

Adafruit_MAX17048 maxlipo;
bool shouldEnterDeepSleep = false;
const int wakeupInterval = 15 * 1000000;

AFE44XX afe44xx(AFE44XX_CS_PIN, AFE44XX_PWDN_PIN);

afe44xx_data afe44xx_raw_data;
int32_t heart_rate_prev=0;
int32_t spo2_prev=0;

//debounce of button
// Constants for debounce
const int BUTTON_PIN = 10;
const int Deepsleeppin=9;
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long RESET_DURATION = 5000; // 5 seconds
volatile int buttonState = HIGH;
volatile int lastButtonState = HIGH;
volatile unsigned long lastDebounceTime = 0;
volatile unsigned long buttonPressStartTime = 0;
volatile bool resetTriggered = false;
unsigned long lastAfeReadTime = 0;
const unsigned long afeReadInterval = 8;  // Adjust the interval as needed
unsigned long lastDisplayUpdateTime = 0;
const unsigned long displayUpdateInterval = 1000; //# of iterations before oled update
int voltageState;

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


// Callback function called when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  //Serial.print("\r\nLast Packet Send Status:\t");
 // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
void updateDisplay() {
  // switch case for oled state
     switch (oledDisplay)
  {
    //display battery precent and charge discharge rate
  case Bat:
     display.clearDisplay();
  display.drawBitmap(0,4,Battery_100,32,32,1);
  display.setCursor(42,0); 
  display.print("ChgR:");display.print(maxlipo.chargeRate()); display.print(" %/hr");
  display.setCursor(40,15);
  display.print("Bat:");display.print(maxlipo.cellPercent(), 1); display.print(" %");
  display.display();
    break;
  //display spo2 and heart rate 
  case Data:
  display.clearDisplay();
  display.drawBitmap(0,4,Heart_Pulse,32,32,1);
  display.setCursor(42,0);
  if (afe44xx_raw_data.spo2 == -999){
  display.print("Probe error!!!");

  }
  else{
  if(afe44xx_raw_data.spo2 <= 80){
    display.print("SPO2:");display.print("Calc...");
  }
  else{
  display.print("SPO2:");display.print(afe44xx_raw_data.spo2);display.print(" %");
  }
  display.setCursor(40,15);
  display.print("BPM:");display.print(afe44xx_raw_data.heart_rate);display.print(" bpm");
  display.display();
    break;
  }
  }  
      } 


void setup()
{
 /* pinMode(Deepsleeppin,INPUT_PULLUP);
  voltageState = digitalRead(Deepsleeppin); 
  if (voltageState == HIGH) {
    Serial.println("Entering deep sleep");
   // Configure deep sleep
    esp_sleep_enable_timer_wakeup(wakeupInterval); // Wake up every 60 seconds
    // Enter deep sleep mode
    esp_deep_sleep_start();
  } 
*/
  Serial.begin(57600);
  // Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  // Initilize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
   // Register the send callback
  esp_now_register_send_cb(OnDataSent);
    // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

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
  display.setTextSize(1.3);
  //Set the cursor coordinates
  display.setCursor(0,0);
  display.print("Pulse Oxi Config...");
  display.display();
  SPI.begin();
  //Serial.println("Intilaziting AFE44xx.. ");
  delay(2000) ;   // pause for a moment
  afe44xx.afe44xx_init();
   display.clearDisplay();
  display.setCursor(0,0);
  display.print("Inited...");
  display.display();
  //Serial.println("Inited...");
  delay(1000);
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Calculating...");
  //Serial.println("Calculating...");
  display.display();
  
  //check if max module is present if not send error
  if (!maxlipo.begin()) {
    Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    while (1) delay(10);
  }

}


void loop()
{ 

 /*voltageState = digitalRead(Deepsleeppin); // Read digital voltage
 switch (shouldEnterDeepSleep)
{
case true:
    Serial.println("Entering deep sleep");
   // Configure deep sleep
    esp_sleep_enable_timer_wakeup(wakeupInterval); // Wake up every 60 seconds
    // Enter deep sleep mode
    esp_deep_sleep_start();
    Serial.println("this will never be printed");
  break;

default:
  break;
}
  if (voltageState == HIGH) {
    shouldEnterDeepSleep = true;
  } */
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
    delay(8);
    afe44xx.get_AFE44XX_Data(&afe44xx_raw_data);
  //displaying raw Red Data
  //Serial.println(afe44xx_raw_data.RED_data);
  //Serial.println(afe44xx_raw_data.IR_data);  
  MediData.RED = afe44xx_raw_data.RED_data;
  MediData.IR = afe44xx_raw_data.IR_data;
  //buffer for spo2 and heart rate
 if (afe44xx_raw_data.buffer_count_overflow)
    {
      if(afe44xx_raw_data.spo2 == -999)
      {
        Serial.println("Probe error!!!!");
      }
      //displays new spo2 and Bpm if theres change
      else if ((heart_rate_prev != afe44xx_raw_data.heart_rate) || (spo2_prev != afe44xx_raw_data.spo2))
      {
        heart_rate_prev = afe44xx_raw_data.heart_rate;
        spo2_prev = afe44xx_raw_data.spo2;

       // Serial.print("calculating sp02...");
        //Serial.print(" Sp02 : ");
        Serial.print(afe44xx_raw_data.spo2);
        MediData.SPO2 = afe44xx_raw_data.spo2;
       // Serial.print("% ,");
       // Serial.print("Pulse rate:");
       Serial.print(afe44xx_raw_data.heart_rate);
       MediData.BPM =afe44xx_raw_data.heart_rate;
     //  Serial.println(" bpm");



        
      }
   

 
}
// Check for display update based on time
  if (currentMillis - lastDisplayUpdateTime >= displayUpdateInterval) {
    updateDisplay();
    lastDisplayUpdateTime = currentMillis;
  }
// Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &MediData, sizeof(MediData));
}