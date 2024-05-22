#include <esp_now.h>
#include <WiFi.h>
//replace with temp sensor mac address
uint8_t tempAddress[] = {0xDC, 0x54, 0x75, 0xC3, 0xBF, 0x18};
uint8_t impad[] = {0xDC,0x54,0x75,0xC3,0x07,0x98};

// Define peerInfo structures for each peer
esp_now_peer_info_t peer1Info;
esp_now_peer_info_t peer2Info;


typedef struct TempR{
   unsigned int R1 = 9820;
   unsigned int R2 = 9897;
   unsigned int R3 = 9860;
} TempR;
 TempR TempCali;

//calibration input from master 
typedef struct ImpFlag{
   bool cal;
   bool measure;
} ImpFlag;
ImpFlag Flag;
esp_now_peer_info_t peerInfo;



// Must match the receiver structure
typedef struct OXI {
  uint16_t IR;  // Use uint16_t for IR to save space
  uint16_t RED; // Use uint16_t for RED to save space
  uint8_t BPM;
  uint8_t SPO2;
} OXI;

typedef struct Ecg {
  int ECG;
} Ecg;

typedef struct Temp{
  double Temp;
} Temp;

typedef struct Imp{ //Send to receiver
  double Impavg;
  double Imps;
} Imp;




OXI MediData;
Ecg ECGData;
Temp TempData;
Imp ImpData;

// Union to hold different types of data
typedef union {
  OXI mediData;
  Ecg ecgData;
  Temp tempData;
} DataUnion;

DataUnion receivedData;
void OnDataRecv
(const uint8_t * mac, const uint8_t *incomingData, int len) {
  
  if (len == sizeof(OXI)) {
    memcpy(&MediData, incomingData, sizeof(MediData));
  Serial.print("Red Values:");
  Serial.println(MediData.RED);
  Serial.print("IR Values:");
  Serial.println(MediData.IR);
  Serial.print(" Sp02 : ");
  Serial.print(MediData.SPO2);
  Serial.print("% ,");
  Serial.print("Pulse rate:");
  Serial.println(MediData.BPM);
  Serial.println(" bpm");
  } 
  else if (len == sizeof(Ecg)) {
  memcpy(&ECGData, incomingData, sizeof(ECGData));
  Serial.print("ECG:");
  Serial.println(ECGData.ECG);
  }
  else if (len == sizeof(Temp)) {
  memcpy(&TempData, incomingData, sizeof(TempData));
  Serial.print("Temperature:");
  Serial.println(TempData.Temp);
  }
  else if (len == sizeof(Imp)) {
  memcpy(&ImpData, incomingData, sizeof(ImpData));
  Serial.print("ImpAvg:");
  Serial.println(ImpData.Impavg);
  Serial.print("Imp:");
  Serial.println(ImpData.Imps);
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");  
}

void handleResistorData(String R) {
  // display r values if inputed 
  if (R.startsWith("Resistor1:")) {
    // Extract and use the value, e.g., convert to integer
    int resistor1Value = R.substring(10).toInt();
    TempCali.R1 = resistor1Value;
    // Implement your logic with resistor1Value
  } 
  else if (R.startsWith("Resistor2:")) {
    int resistor2Value = R.substring(10).toInt();
    TempCali.R2 = resistor2Value;
    // Handle Resistor2 value
  } 
  else if (R.startsWith("Resistor3:")) {
    int resistor3Value = R.substring(10).toInt();
    TempCali.R3 = resistor3Value;
    // Handle Resistor3 value
  }
  esp_err_t result = esp_now_send(tempAddress, (uint8_t *) &TempCali, sizeof(TempCali));
}

void handleImpeadanceData(String IN){
  if (IN.startsWith("Cali:")) {
    // Extract the calibration flag
    bool cali = 1;
    Flag.cal = cali;
    esp_err_t result = esp_now_send(impad, (uint8_t *) &ImpData, sizeof(ImpData));
    Flag.cal = 0;
  }
  else if (IN.startsWith("measure:1")) {
    bool meas = 1;
     
    Serial.println("Performing impedance measurement...");
    Flag.measure = meas;
    esp_err_t result = esp_now_send(impad, (uint8_t *) &ImpData, sizeof(ImpData));
    Flag.measure= 0;  
  }
}

void setup() {
  // Set up Serial Monitor
  Serial.begin(500000);
  // Define the total number of data points
  pinMode(13,OUTPUT);
  // Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
 
  // Initilize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Register callback function
  esp_now_register_recv_cb(OnDataRecv);

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register and add peer1
  memcpy(peer1Info.peer_addr, tempAddress, 6);
  peer1Info.channel = 0;  
  peer1Info.encrypt = false;
  if (esp_now_add_peer(&peer1Info) != ESP_OK) {
    Serial.println("Failed to add peer1");
    return;
  }

  // Register and add peer2
  memcpy(peer2Info.peer_addr, impad, 6);
  peer2Info.channel = 0;  
  peer2Info.encrypt = false;
  if (esp_now_add_peer(&peer2Info) != ESP_OK) {
    Serial.println("Failed to add peer2");
    return;
  }

}
 
void loop() {
  if (Serial.available() > 0) {
    String receivedData = Serial.readStringUntil('\n'); // Read data until newline character
    //Serial.print("Received from Python: ");
    //Serial.println(receivedData);
    handleResistorData(receivedData);
    handleImpeadanceData(receivedData);
  }
}