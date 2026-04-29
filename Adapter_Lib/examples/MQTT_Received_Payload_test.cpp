#include <Arduino.h>
#include "MQTT_Adapter.h" 
#include "LoRaP2P_Adapter.h"

// ==========================================
const char* WIFI_SSID     = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";

// MQTT Broker IP is the local machine running the docker
const char* MQTT_SERVER   = "MQTT_SERVER_IP"; 
const uint16_t MQTT_PORT  = 8883; // Default MQTT TLS port
const char* CLIENT_ID     = "Gateway_01";
// ==========================================
MqttAdapter mqtt(WIFI_SSID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, CLIENT_ID);



//============================================
// Define callback for processing incoming MQTT messages from the backend
//============================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.println("\nPayload received from backend!");
    Serial.printf("Topic: %s\n", topic);
    
    LoRaPayload loraPayload;
    // Process received JSON format
    mqtt.processIncomingMessage(topic, payload, length, loraPayload);
    Serial.println("Json parsed successfully into LoRaPayload struct:");
    Serial.printf("Target Node ID: %d\n", loraPayload.nodeId);
    Serial.printf("Action ID:      %d\n", loraPayload.data.commandData.actionId);
    Serial.printf("Parameter:      %d\n", loraPayload.data.commandData.parameter);
    Serial.println("-----------------------------------------");
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Connecting to WiFi and MQTT...");
    // Initialize MQTT and attach our callback
    mqtt.init(mqttCallback);
}

void loop() {
    // Keep the MQTT connection alive so it can receive the JSON
    mqtt.alive_loop();
}