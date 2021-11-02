#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Adafruit_MPU6050.h>
#include <Arduino_JSON.h>

// Receiver MAC Address
constexpr char WIFI_SSID[] = "The High Cs";
char BOARD_ID[] = "FARRIS_WASHER_1";
uint8_t broadcastAddress[] = {0x94, 0xB9, 0x7E, 0xFA, 0x5A, 0x3C};

// Structure to send data. Must match the receiver structure
typedef struct {
  char id[32];
  bool machineOn;
} sensor_message;

// Class to group readings and necessary calculations
class AccReadings {
  private:
    float accX;
    float accY;
    float accZ;

  public:
    AccReadings(float, float, float);
    float getTotalAcc();
};

class SensorUnit {
  private:
    Adafruit_MPU6050 mpu;
    sensors_event_t a, g, temp;
    AccReadings getAccReadings();
    float runningTotal = 0.0;
    int totalReadings = 0;
    float calibrationAccAvg = 0.0;
    const float STATUS_THRESHOLD_PERCENT = 0.02;
    sensor_message currentMessageToSend;

  public:
    bool isCalibrated = false;
    void initMPU();
    void addReading();
    void calibrate();
    bool determineStatus();
    float getTemperature();
    void setMessage(char[32]);
    sensor_message getMsg();
};

// Timer variables
unsigned long lastMeasurementTime = 0;  
const unsigned long MEASUREDELAY = 400;   // Time between each sensor reading
unsigned long lastEvaluationTime = 0;
const unsigned long EVALDELAY = 2000;   // Time between each determination of whether the machine is on or off

SensorUnit machineUnit = SensorUnit();

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
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

void setup() {
  Serial.begin(115200);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  int32_t channel = getWiFiChannel(WIFI_SSID);

  WiFi.printDiag(Serial); // Uncomment to verify channel number before
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  WiFi.printDiag(Serial); // Uncomment to verify channel change after

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  machineUnit.initMPU();
  machineUnit.setMessage(BOARD_ID);
}

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
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &currentMsg, sizeof(currentMsg));

      if (result == ESP_OK) {
        Serial.println("Sent with success");
      }
      else {
        Serial.println("Error sending the data");
      }

      lastEvaluationTime = millis();
    } else {
      machineUnit.calibrate();
      lastEvaluationTime = millis();
    }
  }
}

AccReadings::AccReadings(float accX, float accY, float accZ) {
  this->accX = accX;
  this->accY = accY;
  this->accZ = accZ;
}

float AccReadings::getTotalAcc() {
  return sqrt(sq(accX) + sq(accY) + sq(accZ));
}

// Init MPU6050
void SensorUnit::initMPU() {
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");
}

void SensorUnit::addReading() {
  this->runningTotal += this->getAccReadings().getTotalAcc();
  this->totalReadings++;
}

void SensorUnit::calibrate() {
  if (totalReadings != 0)  { this->calibrationAccAvg = runningTotal / totalReadings; }
  else { this->calibrationAccAvg = 0; }

  Serial.print("Calibrated to ");
  Serial.println(this->calibrationAccAvg);

  this->runningTotal = 0.0;
  this->totalReadings = 0;
  this->isCalibrated = true;
}

bool SensorUnit::determineStatus() {
  float currentAvg = 0;
  if (this->totalReadings != 0) { currentAvg = this->runningTotal / this->totalReadings; }
  
  this->runningTotal = 0;
  this->totalReadings = 0;

  double difference = abs(currentAvg - this->calibrationAccAvg);
  double average = (currentAvg + this->calibrationAccAvg) / 2.0;

  if ((difference / average) >= this->STATUS_THRESHOLD_PERCENT) {
    this->currentMessageToSend.machineOn = true;
    return true;
  } else {
    this->currentMessageToSend.machineOn = false;
    return false;
  }
}

void SensorUnit::setMessage(char msg[]) {
  strcpy(currentMessageToSend.id, msg);
}

sensor_message SensorUnit::getMsg() {
  return this->currentMessageToSend;
}

AccReadings SensorUnit::getAccReadings() {
  mpu.getEvent(&a, &g, &temp);
  // Save current acceleration values
  return AccReadings(a.acceleration.x, a.acceleration.y, a.acceleration.z);
}

float SensorUnit::getTemperature() {
  mpu.getEvent(&a, &g, &temp);
  return temp.temperature;
}
