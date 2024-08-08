
#include <Wire.h>

const int BUTTON_PIN = 9;
const int LED_PIN = 13;
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long BLINK_DELAY_ON = 200;
const unsigned long BLINK_DELAY_OFF = 800;
const unsigned long GOSLEEP_BUTTON_DURATION = 2000; // Press button for this amount to reset

volatile bool buttonPressed = false;

unsigned long lastButtonTime = 0;
unsigned long lastButtonHighTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long blink_delay = BLINK_DELAY_OFF;

#define displayUpdateInterval 1000;

// Define different states for OLED
typedef enum {
  Bat,
  Data
} stateDisplayType;

volatile stateDisplayType oledDisplay = Bat;

int reading = 0;

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

void setup() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Ensure the LED is off initially

  // Wire.begin();
  Serial.begin(57600);
  // delay(2000);
  delay(500);

  print_wakeup_reason();

  reading = digitalRead(BUTTON_PIN);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, CHANGE);
  // esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 1); // Wake up when button is pressed      
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_9,1); //wake   

}

void loop() {

  unsigned long currentTime = millis();
  if (buttonPressed) {
    buttonPressed = false; // Reset flag from ISR

    reading = digitalRead(BUTTON_PIN);
    //Serial.print("Button changed: ");
    //Serial.println(reading);
    
    if ((currentTime - lastButtonTime) > DEBOUNCE_DELAY) {
      lastButtonTime = currentTime;

      if (reading == HIGH) {
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
          digitalWrite(I2C_POWER, LOW);
          delay(1000);
          esp_deep_sleep_start();
        } else {
          // Switch display state
          Serial.println("Button has been pressed for short time, switching display");
          oledDisplay = (oledDisplay == Bat) ? Data : Bat;
        }
        digitalWrite(LED_PIN, LOW); 
      }
    }
  }
  

  // Blink the LED when the button is not pressed
  if ((currentTime - lastBlinkTime) > blink_delay) {
    lastBlinkTime = currentTime;
    //Serial.println("Blink");
    if (reading == LOW) { 
      reading = digitalRead(LED_PIN);
      if (reading==true) {
        blink_delay= BLINK_DELAY_ON;
        } else {
          blink_delay = BLINK_DELAY_OFF;
        }
      digitalWrite(LED_PIN, !reading); 
    }
  }

}