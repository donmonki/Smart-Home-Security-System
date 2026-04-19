#include <Arduino.h>
#include "MQTT_Adapter.h" 

// ==========================================
const char* WIFI_SSID     = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";

// MQTT Broker IP is the local machine running the docker
const char* MQTT_SERVER   = "MQTT_SERVER_IP"; 
const uint16_t MQTT_PORT  = 1883;
const char* CLIENT_ID     = "Gateway_01";
// ==========================================

MqttAdapter mqtt(WIFI_SSID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, CLIENT_ID);

// Timer and state variables
unsigned long lastTestTime = 0;
const unsigned long testInterval = 5000; 
int currentTestStep = 0;

// ==========================================
// Dummy callback to comply with init 
// ==========================================
void dummyCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("Received message on: %s\n", topic);
}

// ==========================================
// MQTT initialization 
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n Starting MQTT Publish Test Script ");

    mqtt.init(dummyCallback);

}

// ==========================================
// Function iterations to test all publish methods with dummy data
// ==========================================
void loop() {
    // Keep connection alive
    mqtt.alive_loop();

    // Run a different publish test every 5 seconds
    if (millis() - lastTestTime >= testInterval) {
        lastTestTime = millis();
        
        Serial.println("\n-----------------------------------");
        
        switch (currentTestStep) {
            
            case 0: {
                Serial.println("TEST 1: publishGatewayTelemetry()");
                uint8_t dummyBattery = 98;
                mqtt.publishGatewayTelemetry(dummyBattery);
                break;
            }
            
            case 1: {
                Serial.println("TEST 2: publishEvent() -> HEARTBEAT");
                LoRaPayload p1;
                p1.nodeId = 2;
                p1.msgType = LORA_MSG_HEARTBEAT;
                mqtt.publishEvent(p1);
                break;
            }
            
            case 2: {
                Serial.println("TEST 3: publishEvent() -> RFID SCANNED");
                LoRaPayload p2;
                p2.nodeId = 3;
                p2.msgType = LORA_MSG_RFID_SCANNED;
                p2.data.sensorData.rfidUid = 0xDEADBEEF;
                
                mqtt.publishEvent(p2);
                break;
            }
            
            case 3: {
                Serial.println("TEST 4: publishEvent() -> MOTION ALARM");
                LoRaPayload p3;
                p3.nodeId = 4;
                p3.msgType = LORA_MSG_MOTION_ALARM;
                p3.data.sensorData.motionDetected = true;
                
                mqtt.publishEvent(p3);
                break;
            }
            
            case 4: {
                Serial.println("TEST 5: publishNodeOffline()");
                uint8_t deadNodeId = 5;
                
                mqtt.publishNodeOffline(deadNodeId);
                break;
            }
        }

        // Move to the next test step
        currentTestStep = (currentTestStep + 1) % 5; 
    }
}