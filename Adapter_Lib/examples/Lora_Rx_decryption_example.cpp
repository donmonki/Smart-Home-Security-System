#include <Arduino.h>
#include <LoRaP2P_Adapter.h>
#include <HardwareSerial.h>

// Define LoRa module pins and serial
#define RST_PIN 23
#define RX_PIN 18
#define TX_PIN 19

HardwareSerial loraserial(2);
// Creating Lora object with specified pins and serial
LoraP2P myLora(RST_PIN, RX_PIN, TX_PIN, loraserial);

void setup()
{
  Serial.begin(115200);

  if (myLora.moduleInit())
  {
    Serial.println("LoRa Receiver Initialized.");
    Serial.println("Listening for encrypted packets...");
  }
  else
  {
    Serial.println("LoRa Initialization Failed!");
  }
}

void loop()
{

  /**********************
  Remove this comment block to test encrypted reception without decryption
  **********************/
  /*
  // Get raw hex string from the LoRa module
  String rawHex = myLora.receive();

  // Ensure we received a full 16 byte (32 hex characters) payload
  if (rawHex.length() == 32) {
      Serial.println("\n ***Received Packet*** ");
      Serial.println("Raw Hex: " + rawHex);

      // Convert the 32 character Hex String back into a 16 byte array
      unsigned char encryptedBytes[16];
      hexToBytes(rawHex, encryptedBytes);

      // Force received data into our struct to demonstrate how it looks without decryption (will be garbage)
      LoRaPayload garbageData;
      memcpy(&garbageData, encryptedBytes, 16);

      // Print out the received data
      Serial.println("\n--- Struct Values (WITHOUT DECRYPTION) ---");
      Serial.printf("Node ID: %d\n", garbageData.nodeId);
      Serial.printf("Message Type: %d\n", garbageData.msgType);
      Serial.printf("Motion Detected: %d\n", garbageData.motionDetected);
      Serial.printf("Light Level: %d\n", garbageData.lightLevel);
      Serial.printf("Sound Level: %d\n", garbageData.soundLevel);
      Serial.printf("RFID UID: %u\n", garbageData.rfidUid);
      Serial.printf("Battery Level: %d\n", garbageData.batteryLevel);
      Serial.printf("Message Counter: %u\n", garbageData.messageCounter);
      Serial.println("------------------------------------------");
  }
  delay(10);
  */

  LoRaPayload decryptedData;
  // receivePayload returns true if a valid packet was received and successfully decrypted
  if (myLora.receivePayload(decryptedData))
  {

    Serial.println("\n--- Struct Values (USING DECRYPTION) ---");
    Serial.printf("Node ID: %d\n", decryptedData.nodeId);
    Serial.printf("Message Type: %d\n", decryptedData.msgType);
    Serial.printf("Motion Detected: %d\n", decryptedData.data.sensorData.motionDetected);
    Serial.printf("RFID UID: %u\n", decryptedData.data.sensorData.rfidUid);
    Serial.printf("Message Counter: %u\n", decryptedData.data.sensorData.messageCounter);
    Serial.println("------------------------------");
  }

  delay(10);
}