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
    // Transmit Hello in Hex every 5 seconds transmitHex() returns true if the transmission was successful, false otherwise
    if (myLora.transmitHex("48656C6C6F")) { 
        Serial.println("Message sent!");
    }
    delay(5000);
}
