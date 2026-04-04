#include <Arduino.h>
#include <LoRaP2P_Adapter.h>
#include <HardwareSerial.h>

//Define LoRa module pins and serial
#define RST_PIN 23
#define RX_PIN 18
#define TX_PIN 19

HardwareSerial loraserial(2);
// Instantiate the LoraP2P class with the defined pins and serial
LoraP2P myLora(RST_PIN, RX_PIN, TX_PIN, loraserial);

void setup() {
    Serial.begin(115200);
    
    // Call moduleInit() in setup to initialize the LoRa module
    if (myLora.moduleInit()) {
        Serial.println("LoRa Module Initialized Successfully!");
    } else {
        Serial.println("LoRa Initialization Failed!");
    }
}

void loop() {
  Serial.println("Waiting for LoRa data...");
  String incomingHex = myLora.receive();
  
  if (incomingHex.length() > 0)
  {
    Serial.println("\n Payload Received! ");
    Serial.println("Raw Hex: " + incomingHex);
  }
  delay(5000);
}

