#include <Arduino.h>
#include <Wire.h> 

#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library

// Include Adafruit Graphics & OLED libraries
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <logger.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // 32 or 64 OLED display height, in pixels
#define OLED_RESET -1 // Not used
#define SCREEN_ADDRESS 0x3C

#define LED_PIN 13 // Define the LED pin (built-in LED)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

SFE_MAX1704X lipo(MAX1704X_MAX17048); // Create a MAX17048

#define BAT_X 0
#define BAT_Y 16
#define BAT_W 64
#define BAT_H 16
#define BAT_FULL (BAT_W-9) // Define your constant width here
#define BAT_THICK 10 // Define your constant width here
#define CHRG_X 1
#define CHRG_Y 1

// const unsigned char Battery_halfsize [] = {
// 0x00, 0x00, 0x00, 0x00,   // 00000000, 00000000, 00000000, 00000000
// 0x3F, 0xFF, 0xFF, 0xF0,   // 00111111, 11111111, 11111111, 11110000
// 0x40, 0x00, 0x00, 0x10,   // 01000000, 00000000, 00000000, 00010000
// 0x40, 0x00, 0x00, 0x18,   // 01000000, 00000000, 00000000, 00011000
// 0x40, 0x00, 0x00, 0x18,   // 01000000, 00000000, 00000000, 00011000
// 0x40, 0x00, 0x00, 0x1E,   // 01000000, 00000000, 00000000, 00011110
// 0x40, 0x00, 0x00, 0x1A,   // 01000000, 00000000, 00000000, 00011010
// 0x40, 0x00, 0x00, 0x1A,   // 01000000, 00000000, 00000000, 00011010
// 0x40, 0x00, 0x00, 0x1A,   // 01000000, 00000000, 00000000, 00011010
// 0x40, 0x00, 0x00, 0x1A,   // 01000000, 00000000, 00000000, 00011010
// 0x40, 0x00, 0x00, 0x1E,   // 01000000, 00000000, 00000000, 00011110
// 0x40, 0x00, 0x00, 0x18,   // 01000000, 00000000, 00000000, 00011000
// 0x40, 0x00, 0x00, 0x18,   // 01000000, 00000000, 00000000, 00011000
// 0x40, 0x00, 0x00, 0x10,   // 01000000, 00000000, 00000000, 00010000
// 0x3F, 0xFF, 0xFF, 0xF0,   // 00111111, 11111111, 11111111, 11110000
// 0x00, 0x00, 0x00, 0x00,   // 00000000, 00000000, 00000000, 00000000
// };

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

unsigned long currentTime;
unsigned long lastTime;
const unsigned long displayUpdateInterval = 1000; 

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

int previousWidth = 0;
void animatedBarIndicator(int x, int y, int width, int height, float perCent) {
  // clear previous bar and draws new one
  // there is no need to clear display
  display.drawRect(x, y, width, height, SSD1306_WHITE);
  int bar_width = abs(perCent / 100. * (width-4)); // Calculate the width as a fraction of the constant width
  // Erase the previous bar
  if (previousWidth > 0){
    display.fillRect(x + 3, y + 3, previousWidth, height-4, SSD1306_BLACK); 
  }
  // Draw the new bar
  display.fillRect(x + 3, y + 3, bar_width, height-4, SSD1306_WHITE); 
  previousWidth = bar_width; // Store the width for the next iteration
}

void setup()
{

  pinMode(LED_PIN, OUTPUT); // Initialize the LED pin as an output
  digitalWrite(LED_PIN, LOW); // Ensure the LED is off initially
  
  pinMode(I2C_POWER, OUTPUT);
  digitalWrite(I2C_POWER, HIGH); // Turn on power to QWIIC connector


  currentLogLevel = LOG_LEVEL_WARN; // NONE, ERROR, WARN, INFO, DEBUG
  Serial.begin(115200);
  // while(!Serial);
  delay(100);

  //LOGI("OLED Bat Display Test");

  // Start Wire library for I2C
  Wire.begin();

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


   // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    LOGE("SSD1306 allocation failed");
    while (1) {delay(50);}      ;
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

  currentTime = millis();
  lastTime = millis();

}


void loop() { 
  
  currentTime = millis();

  // Check for display update based on time
  if (currentTime - lastTime >= displayUpdateInterval) {
    lastTime = currentTime;

    digitalWrite(LED_PIN, !digitalRead(LED_PIN)); 

    float chargeRate = lipo.getChangeRate();
    float cellPerCent = lipo.getSOC();

    display.clearDisplay();
    
    drawBatteryIndicator(BAT_X, BAT_Y, cellPerCent);
    drawChargingIndicator(CHRG_X, CHRG_Y, BAT_W/2, BAT_H-2, chargeRate);
    
    display.setCursor(BAT_X+BAT_W+4,CHRG_X+4); 
    display.print(chargeRate,0); display.print("%/hr");
    display.setCursor(BAT_X+BAT_W+4,BAT_Y+4);
    display.print(cellPerCent,0); display.print("%");
    display.display();
  } 

  delay(100);
}