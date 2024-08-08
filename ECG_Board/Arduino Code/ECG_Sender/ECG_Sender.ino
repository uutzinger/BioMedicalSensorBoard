#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_MAX1704X.h"

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
const unsigned char ECG_Disp [] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 
0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 
0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0xc0, 0x00, 
0x00, 0x04, 0xc3, 0x00, 0x01, 0x86, 0xc7, 0x80, 0x03, 0xef, 0xec, 0xff, 0x7e, 0x39, 0x78, 0x00, 
0x00, 0x10, 0x60, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 
0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 
0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static float filteredValue = 0;
// Reset pin not used but needed for library
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_RESET);

Adafruit_MAX17048 maxlipo;

//debounce of button
// Constants for debounce
const int BUTTON_PIN = 9;
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long RESET_DURATION = 5000; // 5 seconds
volatile int buttonState = HIGH;
volatile int lastButtonState = HIGH;
volatile unsigned long lastDebounceTime = 0;
volatile unsigned long buttonPressStartTime = 0;
volatile bool resetTriggered = false;
unsigned long lastDisplayUpdateTime = 0;
const unsigned long displayUpdateInterval = 1000;

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
      if (lastButtonState == LOW) {
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

// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t broadcastAddress[] = {0xdc, 0x54, 0x75, 0xc3, 0xbe, 0xfc};
// Structure example to send data
// Must match the receiver structure
typedef struct  Ecg {
  int ECG;
} Ecg;

// Create a struct_message called myData
 Ecg ECGData;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void updateDisplay() {
  display.clearDisplay();
  // switch case for oled state
     switch (oledDisplay)
  {
   case Bat:
  display.setTextSize(1.3);
  display.drawBitmap(0, 4, Battery_100, 32, 32, 1);
  display.setCursor(42, 0);
  display.print("ChgR:"); display.print(maxlipo.chargeRate()); display.print(" %/hr");
  display.setCursor(40, 15);
  display.print("Bat:"); display.print(maxlipo.cellPercent(), 1); display.print(" %");
  display.display();

  break;

  //display ecg
  case Data:
  display.setTextSize(1.3);
  display.drawBitmap(0,4,ECG_Disp,32,32,1);
  display.setCursor(35,0); 
  display.print("ECG Val:");display.print(filteredValue);
  display.display();
  break;


}

}

void setup() {
  Wire.begin();
  Serial.begin(57600);
  delay(500);  
  
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
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  pinMode(BUTTON_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, CHANGE);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_9,1); //wake   


  pinMode(10, INPUT); // Setup for leads off detection LO +
  pinMode(11, INPUT); // Setup for leads off detection LO -


  // // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
  //   Serial.println(F("SSD1306 allocation failed"));
  //   for(;;); // Don't proceed, loop forever
  // }

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
  display.print("ECG Config...");
  display.display();
  delay(1000); // pause for a moment
  
  //check if max module is present if not send error
  if (!maxlipo.begin()) {
    Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    while (1) delay(10);
  }
}

void ECG() {
  if((digitalRead(10) == 1)||(digitalRead(11) == 1)) {
    Serial.println('!');
  }
  else {
    int signalValue = analogRead(A1);
    
    // Apply a low-pass filter
    float alpha = 0.92;  // Filter coefficients (0.9 and 0.1)
    filteredValue = 0;
    filteredValue = alpha * signalValue + (1 - alpha) * filteredValue;
    //myData.ecg = signalValue;
    ECGData.ECG = filteredValue;
     Serial.print("ECG:");
    Serial.println(filteredValue);
  
    // Send message via ESP-NOW
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &ECGData, sizeof(ECGData));
   
    if (result == ESP_OK) {
      //Serial.println("Sent with success");
    }
    else {
      //Serial.println("Error sending the data");
    }
  }
}

unsigned long lastECGTime = 0;
const unsigned long ecgInterval = 10;

void loop() {
  unsigned long currentMillis = millis();
  //esp_sleep_enable_ext0_wakeup(GPIO_NUM_9,1);
  // Check if the button is being held down for 5 seconds
  if (buttonPressStartTime != 0 && !resetTriggered && currentMillis - buttonPressStartTime >= RESET_DURATION) {
    // Button held for 5 seconds, trigger reset
    resetTriggered = true; //deep sleep instead of reset
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
  // Check for ECG update based on time
  if (currentMillis - lastECGTime >= ecgInterval) {
    ECG();
    lastECGTime = currentMillis;
  }

  // Check for display update based on time
  if (currentMillis - lastDisplayUpdateTime >= displayUpdateInterval) {
    updateDisplay();
    lastDisplayUpdateTime = currentMillis;
  }
}