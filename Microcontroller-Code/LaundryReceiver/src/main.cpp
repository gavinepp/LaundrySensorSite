/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp-now-esp32-arduino-ide/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"

const char* SSID = "The High Cs";
const char* PASSWORD = "curse of black pearl";

AsyncWebServer server(80);
AsyncEventSource events("/events");

// Structure example to receive data
// Must match the sender structure
typedef struct {
  char id[32];
  bool machineOn;
} sensor_message;

// Create a struct_message called myData
sensor_message receivedMessage;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&receivedMessage, incomingData, sizeof(receivedMessage));

  Serial.print("Sensor Name: ");
  Serial.println(receivedMessage.id);
  Serial.print("Machine On? : ");
  Serial.println(receivedMessage.machineOn);
  Serial.println();

  DynamicJsonDocument board(1024);
  board["id"] = receivedMessage.id;
  board["status"] = receivedMessage.machineOn;
  String jsonStr;
  serializeJson(board, jsonStr);
  events.send(jsonStr.c_str(), "machine_status", millis());
}

void initSPIFFS() {
  if (!SPIFFS.begin()) {  Serial.println("An error has occurred while mounting SPIFFS"); }
  else {  Serial.println("SPIFFS mounted successfully"); }
}

void initWiFi() {
  // Set device as a Wi-Fi Station AND access point
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(SSID, PASSWORD);


  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nStation IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  initWiFi();
  initSPIFFS();
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);

  // Handle Web Server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.serveStatic("/", SPIFFS, "/");
   
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID received is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();
}
 
void loop() {
  // Optional?
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 5000;
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
    events.send("ping",NULL,millis());
    lastEventTime = millis();
  }
}