
#include <Arduino.h>
#include <Wire.h>

#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library

// Include Adafruit Graphics & OLED libraries
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <logger.h>

const int BUTTON_PIN = 9;
const int LED_PIN = 13;

// Button
//////////////////////////////////////////////////////////////////////////////////////////////
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long GOSLEEP_BUTTON_DURATION = 2000; // Press button for this amount to reset
volatile bool buttonPressed = false;
int buttonState = LOW;

// Interrupt function for different states of OLED when button is pressed 
void IRAM_ATTR handleButtonPress() {
  buttonPressed = true;
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

// LED
//////////////////////////////////////////////////////////////////////////////////////////////
const unsigned long BLINK_DELAY_ON = 200;
const unsigned long BLINK_DELAY_OFF = 800;
int ledState = false;

// Battery Indicator
//////////////////////////////////////////////////////////////////////////////////////////////
SFE_MAX1704X lipo(MAX1704X_MAX17048); // Create a MAX17048
const unsigned long BATTERY_DELAY = 2000;
float chargeRate=0;
float cellPerCent=100;

// Display
//////////////////////////////////////////////////////////////////////////////////////////////
const unsigned long DISPLAY_BAT_UPDATE_INT = 1000;
const unsigned long DISPLAY_DATA_UPDATE_INT = 100;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // 32 or 64 OLED display height, in pixels
#define OLED_RESET -1 // Not used
#define SCREEN_ADDRESS 0x3C

#define BAT_X 0
#define BAT_Y 16
#define BAT_W 64
#define BAT_H 16
#define BAT_FULL (BAT_W-9) // Define your constant width here
#define BAT_THICK 10 // Define your constant width here
#define CHRG_X 1
#define CHRG_Y 1

#define A1_X 1
#define A1_Y 1
#define A_W 126
#define A_H 14
#define A2_X 1
#define A2_Y 2 + A_H

const unsigned char Battery [] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000
  0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,  // 00111111, 11111111, 11111111, 11111111, 01111111, 11111111, 11111111, 11110000
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00010000
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00011000
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00011000
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00011110
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00011010
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00011010
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00011010
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1A,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00011010
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00011110
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00011000
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,  // 01000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00011000
  0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,  // 01000000, 00000000, 00000000, 00010000, 00000000, 00000000, 00000000, 00010000
  0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,  // 00111111, 11111111, 11111111, 11111111, 11111111, 11111111, 11111111, 11110000
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000, 00000000
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define different states for OLED
typedef enum {
  Bat,
  Data
} stateDisplayType;

stateDisplayType oledDisplay = Bat;

void drawBatteryIndicator(int x, int y, float cellPerCent) {
  display.drawBitmap(x, y, Battery, BAT_W, BAT_H, SSD1306_WHITE);
  int width = abs(cellPerCent/100 * BAT_FULL); // Calculate the width as a fraction of the constant width
  display.fillRect(x+3, y+3, width, BAT_THICK, SSD1306_WHITE); // Draw the filled rectangle
}

void drawChargingIndicator(int x, int y, int width, int height, float chargeRate) {
  // Draw the outline rectangles with one pixel separation
  display.drawRect(x, y, width, height, SSD1306_WHITE); // Left rectangle
  display.drawRect(x + width + 1, y, width, height, SSD1306_WHITE); // Right rectangle

  // Calculate the width of the inner bar
  int innerWidth = (width - 2) * abs(chargeRate) / 100;

  // Draw the inner filled rectangle based on charge rate
  if (chargeRate < 0) {
    // Discharging (left rectangle)
    display.fillRect(x + width - innerWidth + 1, y + 2, innerWidth, height - 4, SSD1306_WHITE);
  } else {
    // Charging (right rectangle)
    display.fillRect(x + width + 2, y + 2, innerWidth, height - 4, SSD1306_WHITE);
  }
}

void animatedBarIndicator(int x, int y, int width, int height, float perCent) {
  // clear previous bar and draws new one
  // there is no need to clear display
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  int bar_width = perCent / 100. * (width-4); // Calculate the width as a fraction of the constant width
  // Erase the previous bar
  display.fillRect(x + 3, y + 3, width-4, height-4, SSD1306_BLACK); 
  // Draw the new bar
  display.fillRect(x + 3, y + 3, bar_width, height-4, SSD1306_WHITE); 
}


// Simulated Data
//////////////////////////////////////////////////////////////////////////////////////////////
// Const
const float Mean_1 = 50.0;     // Mean value
const float Amp_1 = 50.0;      // Amplitude
const float Mean_2 = 65.0;     // Mean value
const float Amp_2 = 20.0;      // Amplitude
const float DEG_INCREMENT = 15.0;  // Degree increment
const float RAD_INCREMENT = DEG_INCREMENT * (PI / 180.0); // Radian increment
// Variables
float phase = 0.0;         // Phase in radians
const unsigned long DATA_DELAY = 100;
float data1 = 0;
float data2 = 0;

// LOOP EXECUTION
//////////////////////////////////////////////////////////////////////////////////////////////

unsigned long lastButtonTime = 0;
unsigned long lastButtonHighTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastDisplayTime = 0;
unsigned long lastBatteryTime = 0;
unsigned long lastDataTime = 0;
unsigned long blinkUpdateInterval = BLINK_DELAY_OFF;
unsigned long batteryUpdateInterval = BATTERY_DELAY;
unsigned long displayUpdateInterval = DISPLAY_BAT_UPDATE_INT; 
unsigned long dataUpdateInterval = DATA_DELAY;


void setup() {

  // GPIO
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure the LED is off initially
  pinMode(I2C_POWER, OUTPUT);
  digitalWrite(I2C_POWER, HIGH); // Turn on power to QWIIC connector

  // Start Serial
  currentLogLevel = LOG_LEVEL_WARN; // NONE, ERROR, WARN, INFO, DEBUG
  Serial.begin(57600);
  // delay(2000);
  delay(500);

  print_wakeup_reason();

  // Start Wire library for I2C
  Wire.begin();


  buttonState = digitalRead(BUTTON_PIN);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, CHANGE);
  // esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 1); // Wake up when button is pressed      
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_9,1); //wake   

  // LIPO Battery
  // -------------
  // lipo.enableDebugging();
  // Set up the MAX17048 LiPo fuel gauge:
  if (lipo.begin() == false)  {
    LOGE("MAX17048 not detected. Please check wiring. Freezing.");
    while (1) {delay(50);}      ;
  }
	// We can alert at anywhere between 1% and 32%:
	lipo.setThreshold(20); // Set alert threshold to 20%.
  // Set the low voltage threshold
  lipo.setVALRTMin((float)3.9); // Set low voltage threshold (Volts)
  // Set the high voltage threshold
  lipo.setVALRTMax((float)4.1); // Set high voltage threshold (Volts)
  // Enable the State Of Change alert
  lipo.enableSOCAlert();

  // OLED Display
  // -------------
   // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    LOGE("SSD1306 allocation failed");
  }

    // Clear the display
  display.clearDisplay();
  //Set the color - always use white despite actual display color
  display.setTextColor(WHITE);
  //Set the font size
  display.setTextSize(1.3);
  //Set the cursor coordinates
  display.setCursor(0,0);
  display.print("Display Test...");
  display.display();

  digitalWrite(LED_PIN, !digitalRead(LED_PIN)); 
  delay(1000);

}

void loop() {

  unsigned long currentTime = millis();

  // User Input with Button
  /////////////////////////

  if ((currentTime - lastBatteryTime) > batteryUpdateInterval) {
    lastBatteryTime = currentTime;
    chargeRate = lipo.getChangeRate();
    cellPerCent = lipo.getSOC();
  }

  // User Input with Button
  /////////////////////////

  if (buttonPressed) {
    buttonPressed = false; // Reset flag from ISR

    buttonState = digitalRead(BUTTON_PIN);
    //Serial.print("Button changed: ");
    //Serial.println(buttonState);
    
    if ((currentTime - lastButtonTime) > DEBOUNCE_DELAY) {
      lastButtonTime = currentTime;

      if (buttonState == HIGH) {
        // button pushed
        lastButtonHighTime = currentTime;
        digitalWrite(LED_PIN, HIGH); // Turn on the LED when the button is pressed
        //Serial.println("It was high, turning on LED ");
      } else {
        // button let go
        if ((currentTime - lastButtonHighTime) > GOSLEEP_BUTTON_DURATION) {
          Serial.println("Button is low after long duration going to sleep ");
          Serial.flush(); 
          digitalWrite(LED_PIN, LOW);
          digitalWrite(I2C_POWER, LOW); // power off on QWICC connector
          delay(1000);
          esp_deep_sleep_start();
        } else {
          // Switch display state
          Serial.println("Button has been pressed for short time, switching display");
          oledDisplay = (oledDisplay == Bat) ? Data : Bat;
          displayUpdateInterval = DISPLAY_DATA_UPDATE_INT;
          display.clearDisplay();
          display.display();
        }
        digitalWrite(LED_PIN, LOW); 
      }
    }
  }
  
  // On Boad LED Blinking
  /////////////////////////

  // Blink the LED when the button is not pressed
  if ((currentTime - lastBlinkTime) > blinkUpdateInterval) {
    lastBlinkTime = currentTime;
    //Serial.println("Blink");
    if (buttonState == LOW) { 
      ledState = digitalRead(LED_PIN);
      if (ledState==true) {
        blinkUpdateInterval= BLINK_DELAY_ON;
        } else {
          blinkUpdateInterval = BLINK_DELAY_OFF;
        }
      digitalWrite(LED_PIN, !ledState); 
    }
  }

  // Display Update
  /////////////////////////

  // Check if we need to update display 
  if (currentTime - lastDisplayTime >= displayUpdateInterval) {
    lastDisplayTime = currentTime;
    if (oledDisplay == Data) {
      animatedBarIndicator(A1_X, A1_Y, A_W, A_H, data1);
      animatedBarIndicator(A2_X, A2_Y, A_W, A_H, data2);
      display.display();
    } else {  
      // (oledDisplay == Bat)
      display.clearDisplay();
      drawBatteryIndicator(BAT_X, BAT_Y, cellPerCent);
      drawChargingIndicator(CHRG_X, CHRG_Y, BAT_W/2, BAT_H-2, chargeRate);
      display.setCursor(BAT_X+BAT_W+4,CHRG_X+4); 
      display.print(chargeRate,0); display.print("%/hr");
      display.setCursor(BAT_X+BAT_W+4,BAT_Y+4);
      display.print(cellPerCent,0); display.print("%");
      display.display();
      // Throttel OLED display update
      displayUpdateInterval = DISPLAY_BAT_UPDATE_INT;
    }
  } 

  // Update Data
  //////////////
  if ((currentTime - lastDataTime) >= dataUpdateInterval) {
    lastDataTime = currentTime;
       // Compute the oscillation
      data1 = Mean_1 + Amp_1 * sin(phase);
      data2 = Mean_2 + Amp_2 * sin(phase);
      phase += RAD_INCREMENT;
      if (phase >= 2 * PI) { phase -= 2 * PI; }
  }


}