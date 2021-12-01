/*  
  Gavin Epperson
  UCA Honors Capstone, Fall 2021
  
  WasherWatcher LaundrySender code
  "main.cpp"
  
  Code to be flashed onto each machine sensor microcontroller (ESP32).
  Each microcontroller is connected to an MPU-6050 accelerometer, which it uses to determine the status of the machine.
  Data is sent to LaundryReceiver microcontrollers (given in receiverMacAddress variable).
*/

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Adafruit_MPU6050.h>
#include <Arduino_JSON.h>

constexpr char WIFI_SSID[] = "UCAWIRELESS"; // String name of the WiFi network to connect to
char BOARD_ID[] = "FARRIS_WASHER_2";        // String name of this board (aka the machine it is attached to)

const unsigned long MEASUREDELAY = 400;     // Time between each sensor reading
const unsigned long EVALDELAY = 2000;       // Time between each determination of whether the machine is on or off

/*
  Receiver microcontroller MAC Address. Notice that for ESP32 units, it can simply be the WiFi.macAddress() value 
  but for ESP8266 units, it needs to be the WiFi.softAPmacAddress() value. ESP32s can use either.
  This is probably due to the receiver being both an STA and soft AP unit, but I'm not sure.
*/
uint8_t receiverMacAddress[] = {0x94, 0xB9, 0x7E, 0xFA, 0x5A, 0x3D};


// Callback when data is sent over ESP-NOW
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Used to ensure the sender and receiver are on the same WiFi channel
int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
    for (uint8_t i=0; i<n; i++) {
      if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
        return WiFi.channel(i);
      }
    }
  }
  return 0;
}

// Helper function used to establish an ESP-NOW connection with the receiver safely
bool prepareEspNow() {

  // Set ESP32 as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // The following code ensures that the ESP-NOW connection doesn't have issues caused by channel conflicts
  int32_t channel = getWiFiChannel(WIFI_SSID);

  WiFi.printDiag(Serial); // Print to verify channel number before
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  WiFi.printDiag(Serial); // Print to verify channel change after


  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return false;
  }

  // Once ESP-NOW is successfully initialized, register the onSend callback 
  esp_now_register_send_cb(onDataSent);
  
  // Register peer (the receiver microcontroller) to communicate with
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return false;
  }

  return true;
}

// Structure to send data. Must match the receiver structure
typedef struct {
  char id[32];
  bool machineOn;
} sensor_message;


/************************** AccReadings Class ****************************
 * A class used to group the readings from the MPU 6050 together
 * Also allows us to average the 3-axes into a single reading
 *************************************************************************/
class AccReadings {
  private:
    float accX;
    float accY;
    float accZ;

  public:
    AccReadings(float, float, float);
    float getTotalAcc();
};

// Constructor, given the 3-axes as floats
AccReadings::AccReadings(float accX, float accY, float accZ) {
  this->accX = accX;
  this->accY = accY;
  this->accZ = accZ;
}

// A method that returns a single average reading given the three axes data points
float AccReadings::getTotalAcc() {
  return sqrt(sq(accX) + sq(accY) + sq(accZ));
}


/***************** SensorUnit Class Definition *************************
 * This class defines all helpful methods and variables for the accelerometer,
 * keeping track of the running reading totals to calculate whatever average is needed
 ************************************************************************/
class SensorUnit {
  private:
    const float STATUS_THRESHOLD_PERCENT = 0.01;
    Adafruit_MPU6050 mpu;
    sensors_event_t a, g, temp;
    AccReadings getAccReadings();
    float runningTotal = 0.0;
    int totalReadings = 0;
    float calibrationAccAvg = 0.0;
    sensor_message currentMessageToSend;

  public:
    bool isCalibrated = false;
    bool initMPU();
    void addReading();
    void calibrate();
    bool determineStatus();
    float getTemperature();
    void setMessage(char[32]);
    sensor_message getMsg();
};

// Initialize the MPU6050 accelerometer. If unable to find it return false.
bool SensorUnit::initMPU() {
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    return false;
  }
  Serial.println("MPU6050 Found!");
  return true;
}

// Helper method to add an accelerometer reading to the running total
void SensorUnit::addReading() {
  this->runningTotal += this->getAccReadings().getTotalAcc();
  this->totalReadings++;
}

// "Calibrates" the accelerometer, assuming it isn't moving. Results used to evaluate machine state.
void SensorUnit::calibrate() {
  if (totalReadings != 0)  { this->calibrationAccAvg = runningTotal / totalReadings; }
  else { this->calibrationAccAvg = 0; }

  Serial.print("Calibrated to ");
  Serial.println(this->calibrationAccAvg);

  this->runningTotal = 0.0;
  this->totalReadings = 0;
  this->isCalibrated = true;
}

// Determines the state of the machine (on/off) by comparing it to the calibration baseline
bool SensorUnit::determineStatus() {
  // Safely determine the current average
  float currentAvg = 0;
  if (this->totalReadings != 0) { 
    currentAvg = this->runningTotal / this->totalReadings; 
  }
  
  // Reset for next cycle
  this->runningTotal = 0;
  this->totalReadings = 0;

  // Determine the percent difference between currentAvg and calibrationAvg
  // Uses the equation %diff = |a - b| / ((a+b)/2) , where a and b are 2 numbers
  double calcDifference = abs(currentAvg - this->calibrationAccAvg);
  double calcAverage = (currentAvg + this->calibrationAccAvg) / 2.0;

  if ((calcDifference / calcAverage) >= this->STATUS_THRESHOLD_PERCENT) {
    this->currentMessageToSend.machineOn = true;
    return true;
  } else {
    this->currentMessageToSend.machineOn = false;
    return false;
  }
}

// Sets the string message to send over ESP-NOW. Likely the machine's name.
void SensorUnit::setMessage(char msg[]) {
  strcpy(currentMessageToSend.id, msg);
}

// Returns the message to send over ESP-NOW. Likely the machine's name.
sensor_message SensorUnit::getMsg() {
  return this->currentMessageToSend;
}

// Returns the current accelerometer readings in the form of the custom AccReadings object
AccReadings SensorUnit::getAccReadings() {
  mpu.getEvent(&a, &g, &temp);
  // Save current acceleration values
  return AccReadings(a.acceleration.x, a.acceleration.y, a.acceleration.z);
}

// Returns the current temperature provided by the sensor. Currently unused.
float SensorUnit::getTemperature() {
  mpu.getEvent(&a, &g, &temp);
  return temp.temperature;
}


/******************* Arduino Setup() Function ****************************
 * Defines the global SensorUnit variable then runs Arduino's built-in setup() function.
 * Here, setup() prepares the ESP-NOW connection and initializes the machineUnit variable. 
 **************************************************************************/

SensorUnit machineUnit = SensorUnit();

void setup() {
  Serial.begin(115200);

  if (prepareEspNow() == false) { 
    Serial.println("ESP-Now failed to initialize, exiting setup now.");
    return;
  }

  if (machineUnit.initMPU() == false) {
    Serial.println("Error initializing hardware, exiting setup now.");
    return;
  }

  machineUnit.setMessage(BOARD_ID);
}

/******************* Arduino Loop() Function ****************************
 * Runs Arduino's built-in loop() function, which repeats indefinitely while the microcontroller is powered.
 * Here, loop() takes measurement every MEASUREDELAY milliseconds and evaluates the state
 * of the machine based off these measurements every EVALDELAY milliseconds.
 * After determining the machine's status, it sends these results through ESP-NOW and resets.
 *************************************************************************/

// Timer variables for use in the loop (determine when to read from the sensor and evaluate state)
unsigned long lastMeasurementTime = 0;  
unsigned long lastEvaluationTime = 0;


void loop() {
  unsigned long startingTime = millis();

  // Take simple measurements and add to running average
  if ((startingTime - lastMeasurementTime) > MEASUREDELAY) {
    machineUnit.addReading();
    lastMeasurementTime = millis();
  }

  // Determine the machine's status and send the results via ESP-NOW
  if ((startingTime - lastEvaluationTime) > EVALDELAY) {

    // Ensure machine is calibrated before performing the first evaluation
    if (machineUnit.isCalibrated) {

      machineUnit.determineStatus();

      // Send message via ESP-NOW
      sensor_message currentMsg = machineUnit.getMsg();
      Serial.println(currentMsg.machineOn);
      esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t *) &currentMsg, sizeof(currentMsg));

      if (result == ESP_OK) {
        Serial.println("Sent with success");
      }
      else {
        Serial.println("Error sending the data");
      }

    } else {
      machineUnit.calibrate();
    }
    
    // Reset timer for next evaluation
    lastEvaluationTime = millis();
  }
}
