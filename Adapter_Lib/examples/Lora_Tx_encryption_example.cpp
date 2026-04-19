#include <Arduino.h>
#include <LoRaP2P_Adapter.h>
#include <HardwareSerial.h>

//Define LoRa module pins and serial
#define RST_PIN 23
#define RX_PIN 18
#define TX_PIN 19
uint32_t msgCounter = 0;
HardwareSerial loraserial(2);

// Creating Lora object with specified pins and serial
LoraP2P myLora(RST_PIN, RX_PIN, TX_PIN, loraserial);

void setup() {
    Serial.begin(115200);
    
    // Call moduleInit() to initialize the LoRa module
    if (myLora.moduleInit()) {
        Serial.println("LoRa Module Initialized Successfully!");
    } else {
        Serial.println("LoRa Initialization Failed!");
    }
}

void loop() {
    LoRaPayload myData;
    myData.nodeId = 1;             // Node ID 1 for this device
    myData.msgType = 2;            // MSG_MOTION_ALARM
    myData.data.sensorData.motionDetected = true;  // Motion detection triggered
    myData.data.sensorData.rfidUid = 0;            // No RFID card scanned
    
    // Increment the counter so the encrypted output changes every time
    myData.data.sensorData.messageCounter = ++msgCounter; 

    Serial.printf("\nTransmitting Packet #%d...\n", msgCounter);

    // Transmit the encrypted payload and print the raw hex for debugging
    if (myLora.transmitPayload(myData)) {
        Serial.println("Sent Raw Hex:" + bytesToHex((unsigned char*)&myData, sizeof(LoRaPayload)));
        Serial.println("Encrypted packet sent!");
    } else {
        Serial.println("Failed to send packet.");
    }
    
    delay(5000);
}
