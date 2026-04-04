#ifndef LORA_P2P_ADAPTER_H
#define LORA_P2P_ADAPTER_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

enum MessageType : uint8_t
{
    MSG_HEARTBEAT = 0x01,
    MSG_MOTION_ALARM = 0x02,
    MSG_RFID_SCANNED = 0x03,
    MSG_SENSOR_UPDATE = 0x04
};

// The 16-byte Payload Struct
struct __attribute__((packed)) LoRaPayload
{
    uint8_t nodeId;
    uint8_t msgType;
    bool motionDetected;
    uint16_t lightLevel;
    uint16_t soundLevel;
    uint32_t rfidUid;
    uint8_t batteryLevel;
    uint32_t messageCounter;

    LoRaPayload()
    {
        memset(this, 0, sizeof(LoRaPayload));
    }
};

// Function prototypes 
void encryptPayload(const LoRaPayload &inputData, unsigned char *outputBuffer16);
bool decryptPayload(const unsigned char *inputBuffer16, LoRaPayload &outputData);
String bytesToHex(const unsigned char *data, int length);
void hexToBytes(const String &hex, unsigned char *bytes);

class LoraP2P
{
private:
    uint8_t _rst_pin;
    uint8_t _rx_pin;
    uint8_t _tx_pin;
    HardwareSerial &_loraSerial;
    bool sendCmd(const char *cmd, const char *expected_response = "ok");

public:
    LoraP2P(uint8_t rst_pin, uint8_t rx_pin, uint8_t tx_pin, HardwareSerial &serial);

    // Initialization routine
    bool moduleInit();

    // TX and RX handling
    bool transmitHex(const String &hexData);
    String receive();
    bool transmitPayload(const LoRaPayload &payload);
    bool receivePayload(LoRaPayload &receivedData);
};

#endif // LORA_P2P_ADAPTER_H