/*  
  Gavin Epperson
  UCA Honors Capstone, Fall 2021
  
  WasherWatcher LaundryReceiver code
  "main.cpp"
  
  Code to be flashed onto each laundry room microcontroller (ESP32).
  Data is received from multiple microcontroller senders, which is then used to update website files.
  The receiver microcontroller hosts these website files for anyone locally to view.
*/

#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"

const char* SSID = "UCAWIRELESS";
const char* PASSWORD = "";

AsyncWebServer server(80);
AsyncEventSource events("/events");

// Structure to receive data. Must match the sender structure
typedef struct {
  char id[32];
  bool machineOn;
} sensor_message;

// Create a sensor_message called receivedMessage
sensor_message receivedMessage;

// Callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {

  // Copy received data to global struct
  memcpy(&receivedMessage, incomingData, sizeof(receivedMessage));

  Serial.print("Sensor Name: ");
  Serial.println(receivedMessage.id);
  Serial.print("Machine On? : ");
  Serial.println(receivedMessage.machineOn);
  Serial.println();

  // Format the received data in JSON and send it as an AsyncEvent (handled by JavaScript)
  DynamicJsonDocument board(1024);
  board["id"] = receivedMessage.id;
  board["status"] = receivedMessage.machineOn;

  String jsonStr;
  serializeJson(board, jsonStr);
  events.send(jsonStr.c_str(), "machine_status", millis());
}

// Helper function to initialize SPIFFS (SPI Flash File System), a way of storing files on the controller.
bool initSPIFFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
    return false;
  }
  Serial.println("SPIFFS mounted successfully");
  return true;
}

// Helper function to connect the microcontroller to the WiFi network. Will hang indefinitely until it connects.
void initWiFi() {
  // Set device as a Wi-Fi station (able to serve the website to all users connected to the WiFi network)
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);


  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  // Print information about the controller's WiFi connection
  Serial.print("\nStation IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
  Serial.print("WiFi MAC Address: ");
  Serial.println(WiFi.macAddress());
}

/******************* Arduino Setup() Function ****************************
 * Runs Arduino's built-in setup() function.
 * Here, setup() initializes the WiFi connection, file system, and ESP-NOW
 * and additionally prepares and begins hosting the WasherWatcher website.
 **************************************************************************/
void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  initWiFi();

  // Initialize ESP-NOW
  if (initSPIFFS() != true) {
    Serial.println("Error initializing SPIFFS. Returning from setup");
    return;
  }
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW. Returning from setup");
    return;
  }
  
  // Once ESP-NOW is successfully initialized, we will register our receiver handler
  esp_now_register_recv_cb(OnDataRecv);

  // Handle Web Server (direct users to the correct html when connecting)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  // Serve the website files stored in the "data" folder with SPIFFS
  server.serveStatic("/", SPIFFS, "/");
  
  // Add AsyncEvent for when users connect
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID received is: %u\n", client->lastId());
    }
    // Send event with message "hello!", whose id is current millis and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });

  // Add the AsyncEvents to the WebServer and begin hosting the website 
  server.addHandler(&events);
  server.begin();
}
 
 /******************* Arduino Loop() Function ****************************
 * Runs Arduino's built-in loop() function, which repeats indefinitely while the microcontroller is powered.
 * Here, loop() is empty, as our AsyncEvent handlers run automatically when data
 * is received, meaning we don't have to constantly poll for new data in the loop,
 *************************************************************************/
void loop() {}