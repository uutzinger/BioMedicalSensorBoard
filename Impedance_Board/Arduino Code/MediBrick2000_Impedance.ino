#include "Wire.h"
#include <math.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_MAX1704X.h"

// Configure the registers (refer to the AD5933 Datasheet)
#define SLAVE_ADDR 0x0D
#define ADDR_PTR 0xB0

#define START_FREQ_R1 0x82
#define START_FREQ_R2 0x83
#define START_FREQ_R3 0x84

#define FREG_INCRE_R1 0x85
#define FREG_INCRE_R2 0x86
#define FREG_INCRE_R3 0x87

#define NUM_INCRE_R1 0x88
#define NUM_INCRE_R2 0x89

#define NUM_SCYCLES_R1 0x8A 
#define NUM_SCYCLES_R2 0x8B 

#define RE_DATA_R1 0x94
#define RE_DATA_R2 0x95

#define IMG_DATA_R1 0x96
#define IMG_DATA_R2 0x97

#define TEMP_R1 0x92
#define TEMP_R2 0x93

#define CTRL_REG 0x80
#define CTRL_REG2 0x81

#define STATUS_REG 0x8F

const float MCLK = 16.776*pow(10,6); // AD5933 Internal Clock Speed 16.776 MHz
const float start_freq = 50*pow(10,3); // Set start freq, < 100Khz
const float incre_freq = 1*pow(10,2); // Set freq increment
const int incre_num = 49; // Set number of increments; < 511

// Initialized the receive data from the receiver
bool cal = 0;
bool measure = 1;

char state; 
double gainFactor = 0.0; 

// Receiver MAC address
uint8_t broadcastAddress[] = {0xDC, 0x54, 0x75, 0xC3, 0xBE, 0xFC};

typedef struct Imp{ //Send to receiver
  double Impavg;
  double Imps;
} Imp;
Imp ImpData;

esp_now_peer_info_t peerInfo;

//calibration input from master 
typedef struct ImpFlag{
   bool cal;
   bool measure;
} ImpFlag;
ImpFlag Flag;

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

const unsigned char Imp_screen [] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x07, 0xe0, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x3c, 0x3c, 0x00, 0x00, 0x73, 0xce, 0x00, 
0x00, 0x67, 0xe6, 0x00, 0x1e, 0xee, 0x77, 0x78, 0x3f, 0xcc, 0x33, 0xfc, 0x33, 0xcc, 0x33, 0xcc, 
0x33, 0xce, 0x73, 0xcc, 0x3f, 0xde, 0x7b, 0xfc, 0x1e, 0xfe, 0x7f, 0x78, 0x00, 0x60, 0x06, 0x00, 
0x00, 0x70, 0x0e, 0x00, 0x00, 0x3c, 0x3c, 0x00, 0x00, 0x1f, 0xf8, 0x00, 0x00, 0x07, 0xe0, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Reset pin not used but needed for library
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_RESET);
 
Adafruit_MAX17048 maxlipo;

//debounce of button
//Constants for debounce
const int BUTTON_PIN = 9;
const unsigned long DEBOUNCE_DELAY = 50;
const unsigned long RESET_DURATION = 5000; // 5 seconds
volatile int buttonState = HIGH;
volatile int lastButtonState = HIGH;
volatile unsigned long lastDebounceTime = 0;
volatile unsigned long buttonPressStartTime = 0;
volatile bool resetTriggered = false;
unsigned long lastDisplayUpdateTime = 0;
const unsigned long displayUpdateInterval = 1000; //# of iterations before oled update

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
      } 
      else {
        // Button released, reset timer and trigger reset if held for 5 seconds
        buttonPressStartTime = 0;
        resetTriggered = false;
      }

      // Move this part outside the inner if statement
      if (lastButtonState == LOW) {
        // Button pressed, toggle OLED display state
        if (oledDisplay == Bat) {
          oledDisplay = Data;
        } 
        else {
          oledDisplay = Bat;
        }
      }
    }
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Callback function executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&Flag, incomingData, sizeof(Flag));
  cal = Flag.cal;
  measure = Flag.measure;
}

/////////////////////////////////
void updateDisplay() {
  display.clearDisplay();

  // switch case for oled state
  switch (oledDisplay) {

  //display battery precent and charge discharge rate
  case Bat:
    display.setTextSize(1.3);
    display.drawBitmap(0,4, Battery_100 ,32,32,1);
    display.setCursor(42,0); 
    display.print("ChgR:"); 
    display.print(maxlipo.chargeRate()); 
    display.print(" %/hr");
    display.setCursor(40,15);
    display.print("Bat:"); 
    display.print(maxlipo.cellPercent(), 1); 
    display.print(" %");
    display.display();
  break;

  //display imp
  case Data:
    display.setTextSize(1.3);
    display.drawBitmap(0,4, Imp_screen ,32,32,1);
    display.setCursor(42,0); 
    display.print("Impedance:");
    display.setCursor(40,15);
    display.print(ImpData.Impavg);
    display.print(" ohm");
    display.display();
  break;
  }
}
/////////////////////////////
void setup() {
	Wire.begin();
	Serial.begin(115200);
  delay(500);
  pinMode(12, OUTPUT);
  /////////////////////////////////
 
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  
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


  //INIT  oled button 
  pinMode(BUTTON_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, CHANGE);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_9,1); //wake 
  // Start Wire library for I2C
  //Wire.begin();
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
  display.print("Imp Sensor Config...");
  display.display();
  delay(1000);
  //check if max module is present if not send error
  if (!maxlipo.begin()) {
    Serial.println(F("Couldnt find Adafruit MAX17048?\nMake sure a battery is plugged in!"));
    while (1) delay(10);
  }

	//nop - clear ctrl-reg
	writeData(CTRL_REG,0x0);

	//reset ctrl register
	writeData(CTRL_REG2,0x10);	

	programReg();
  calibrateGainFactor();
}

void loop(){
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
  if (cal == 1) {
    ESP.restart();
    cal = 0;
  }
  if (measure == 1) {
    runSweep();
    delay(1000);
    measure = 0;
  }  
  Serial.flush();

  if (currentMillis - lastDisplayUpdateTime >= displayUpdateInterval) {
    updateDisplay();
    lastDisplayUpdateTime = currentMillis;
  }
}


void calibrateGainFactor() {
	short re;
	short img;
	double freq;
	float mag;
  float magtot = 0;
  float magavg;
  int i=0;

	programReg();
  digitalWrite(12, HIGH);
  delay(100);

	// 1. Standby '10110000' Mask D8-10 of avoid tampering with gains
	writeData(CTRL_REG,(readData(CTRL_REG) & 0x07) | 0xB0);

	// 2. Initialize sweep
	writeData(CTRL_REG,(readData(CTRL_REG) & 0x07) | 0x10);

	// 3. Start sweep
	writeData(CTRL_REG,(readData(CTRL_REG) & 0x07) | 0x20);	


	while((readData(STATUS_REG) & 0x07) < 4 ) {  // Check that status reg != 4, sweep not complete
		delay(100); // delay between measurements

		int flag = readData(STATUS_REG)& 2;

		if (flag==2) {

			byte R1 = readData(RE_DATA_R1);
			byte R2 = readData(RE_DATA_R2);
			re = (R1 << 8) | R2;
      
			R1  = readData(IMG_DATA_R1);
			R2  = readData(IMG_DATA_R2);
			img = (R1 << 8) | R2;


			freq = start_freq + i*incre_freq;
			mag = sqrt(pow(double(re),2)+pow(double(img),2));
      magtot = magtot + mag;
      Serial.print("mag:");
      Serial.println(mag);
			if((readData(STATUS_REG) & 0x07) < 4 ){
				writeData(CTRL_REG,(readData(CTRL_REG) & 0x07) | 0x30);
				i++;
			}
    }
	}
  int count = 50;
  magavg = magtot / count;
  gainFactor = (1.0 /2000.0) / magavg;
  Serial.print("Mag Avg: ");
  Serial.println(magavg);
  Serial.print("GF: ");
  Serial.println(gainFactor, 16);
  digitalWrite(12, LOW);
  delay(100);
	writeData(CTRL_REG,(readData(CTRL_REG) & 0x07) | 0xA0);
}

void programReg(){

	// Set Range 1, PGA gain 1
	writeData(CTRL_REG,0x01);

	// Set settling cycles
	writeData(NUM_SCYCLES_R1, 0x07);
	writeData(NUM_SCYCLES_R2, 0xFF);

	// Start frequency of 1kHz
	writeData(START_FREQ_R1, getFrequency(start_freq,1));
	writeData(START_FREQ_R2, getFrequency(start_freq,2));
	writeData(START_FREQ_R3, getFrequency(start_freq,3));

	// Increment by 1 kHz
	writeData(FREG_INCRE_R1, getFrequency(incre_freq,1)); 
	writeData(FREG_INCRE_R2, getFrequency(incre_freq,2)); 
	writeData(FREG_INCRE_R3, getFrequency(incre_freq,3));

	// Points in frequency sweep (100), max 511
	writeData(NUM_INCRE_R1, (incre_num & 0x001F00)>>0x08 );
	writeData(NUM_INCRE_R2, (incre_num & 0x0000FF));
}

void runSweep() {
	short re;
	short img;
	double freq;
	double mag;
	double phase;
	double gain;
	double Impedance;
  double GF;
  double FFW;
  double wt;
  double ht;
  double BF;
  double tot = 0;
  double magcount = 0;
  double impcount = 0;
  double avgmag;
  double totimp = 0;
  double avgimp;
  double totreac = 0;
  double avgreac;
  double totre;
  double avgre;
	int i=0;
  int A;
  int G;

	programReg();
  delay(100);

	// 1. Standby '10110000' Mask D8-10 of avoid tampering with gains
	writeData(CTRL_REG,(readData(CTRL_REG) & 0x07) | 0xB0);

	// 2. Initialize sweep
	writeData(CTRL_REG,(readData(CTRL_REG) & 0x07) | 0x10);

	// 3. Start sweep
	writeData(CTRL_REG,(readData(CTRL_REG) & 0x07) | 0x20);	


	while((readData(STATUS_REG) & 0x07) < 4 ) {  // Check that status reg != 4, sweep not complete
		delay(100); // delay between measurements

		int flag = readData(STATUS_REG)& 2;


		if (flag==2) {

			byte R1 = readData(RE_DATA_R1);
			byte R2 = readData(RE_DATA_R2);
			re = (R1 << 8) | R2;
      totre = totre + re;

			R1  = readData(IMG_DATA_R1);
			R2  = readData(IMG_DATA_R2);
			img = (R1 << 8) | R2;
      totreac = totreac + img;

      // debug prints
      Serial.print("real:"); Serial.println(re);
      Serial.print("img:"); Serial.println(img);
      phase = atan(double(img)/double(re));
			phase = (180.0/3.1415926)*phase;  //convert phase angle to degrees

			freq = start_freq + i*incre_freq;
			mag = sqrt(pow(double(re),2)+pow(double(img),2));

      // debug print
      Serial.print("phase:"); Serial.println(phase);
      Serial.print("mag:"); Serial.println(mag);

      tot = tot+mag;
      Impedance = (1/(gainFactor*mag));
      ImpData.Imps = Impedance;
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &ImpData, sizeof(ImpData));
      if (result == ESP_OK) {
        Serial.println("Sent with success");
      }
      else {
        Serial.println("Error sending the data");
      }
    
      // Update impedance count and total impedance for all impedance values
      //impcount = 50;
      totimp = totimp + Impedance;
	    	
			Serial.print("Frequency: ");
			Serial.print(freq/1000);
			Serial.print(",kHz;");

      Serial.print(" Impedance Magnitude: ");
			Serial.print(Impedance);
			Serial.println(";");

			// break;  //TODO: for single run, remove after debugging
			
			//Increment frequency
			if((readData(STATUS_REG) & 0x07) < 4 ){
				writeData(CTRL_REG,(readData(CTRL_REG) & 0x07) | 0x30);
				i++;
			}             
		}
	}
  
  int count = 50;
  avgmag = tot/count;
  avgimp = totimp/count;
  avgreac = totreac/count;
  avgre = totre/count;

  // Uncomment below if want to measure body fat from the impedance sensor. Body fat calculations are made in the GUI. 
  // wt = 82; //lbsC
  // ht = 167; //cm, m

  //FFW = (0.396*((pow(ht,2))/(avgimp/200))+ (0.143 * wt) + 8.399)*1.37*2; //instruc
  // Serial.print ("FFM: ");
  // Serial.println (FFW);
  // BF = ((wt-FFW)/wt)*100;  


  Serial.print(" Avg Mag: ");
  Serial.print(avgmag);
  Serial.print(",");

  Serial.print(" Avg Impedance: ");
  Serial.print(avgimp);
  Serial.print(",");
  ImpData.Impavg = avgimp;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &ImpData, sizeof(ImpData));
  if (result == ESP_OK) {
      Serial.println("Sent with success");
  }
    else {
      Serial.println("Error sending the data");
  }
  // Serial.print(" % Body Fat: ");
  // Serial.print(BF);
  // Serial.print(",");

	//Power down
  writeData(CTRL_REG,0xA0);
  //digitalWrite(12, LOW);
  delay(100);
	writeData(CTRL_REG,(readData(CTRL_REG) & 0x07) | 0xA0);
}

void writeData(int addr, int data) {

 Wire.beginTransmission(SLAVE_ADDR);
 Wire.write(addr);
 Wire.write(data);
 Wire.endTransmission();
 delay(1);
}

int readData(int addr){
	int data;

	Wire.beginTransmission(SLAVE_ADDR);
	Wire.write(ADDR_PTR);
	Wire.write(addr);
	Wire.endTransmission();

	delay(1);

	Wire.requestFrom(SLAVE_ADDR,1);

	if (Wire.available() >= 1){
		data = Wire.read();
	}
	else {
		data = -1;
	}

	delay(1);
	return data;	
}

byte getFrequency(float freq, int n){
	long val = long((freq/(MCLK/4)) * pow(2,27));
	byte code;

	  switch (n) {
	    case 1:
	      code = (val & 0xFF0000) >> 0x10; 
	      break;
	    
	    case 2:
	      code = (val & 0x00FF00) >> 0x08;
	      break;

	    case 3:
	      code = (val & 0x0000FF);
	      break;

	    default: 
	      code = 0;
	  }

	return code;  
}