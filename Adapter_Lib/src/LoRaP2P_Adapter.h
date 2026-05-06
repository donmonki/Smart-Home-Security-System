#ifndef LORA_P2P_ADAPTER_H
#define LORA_P2P_ADAPTER_H

#include <Arduino.h>
#include <stdint.h>
#include <string.h>

enum MessageType : uint8_t
{
    // Uplink (Node -> Gateway)
    LORA_MSG_HEARTBEAT = 0x01,     // Sent by all nodes
    LORA_MSG_MOTION_ALARM = 0x02,  // Sent by Motion sensor node
    LORA_MSG_RFID_SCANNED = 0x03,  // Sent by RFID node

    // Downlink (Gateway -> Node)
    LORA_MSG_COMMAND = 0x04,
    LORA_MSG_ACK = 0x05            // Acknowledgement of an uplink message
};

enum ActionType : uint8_t
{
    CMD_ALARM_ON = 0x01,
    CMD_ALARM_OFF = 0x02,
};

enum AuthenticationResult : uint8_t
{
    AUTH_FAILURE = 0x01,
    AUTH_SUCCESS = 0x02,
};

// The 14-byte Payload Struct
struct __attribute__((packed)) LoRaPayload {
    // 2-Byte Header 
    uint8_t nodeId;
    uint8_t msgType;

    // 14-Byte Payload 
    union {
        // Used by Motion Node, RFID Node, and all Heartbeats
        struct {
            bool motionDetected;    // 1 byte
            uint32_t rfidUid;       // 4 bytes
            uint32_t messageCounter;// 4 bytes
            uint8_t padding[7];     // Pad to be 16 bytes 
        } sensorData;

        // Used by Gateway to control the LED/Buzzer Node
        struct {
            uint8_t actionId;       // Trigger Alarm node
            uint8_t authenticationResult; // Authentication result for RFID scanned events
            uint8_t parameter;      // Siren Duration time in seconds
            uint8_t padding[13];    // Pad to be 16 bytes
        } commandData;

        // Used by Gateway to acknowledge an uplink message
        struct {
            uint32_t ackedMessageCounter; // 4 bytes — matches sensorData.messageCounter
            uint8_t  padding[12];          // Pad to be 16 bytes
        } ackData;

    } data;
    // Zero initializing the struct to ensure no garbage values in the payload
    LoRaPayload() {
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
    String _bufferedRx; // packet swallowed during LBT, replayed by next receive()
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

    // Collision-avoidance / reliability layer
    bool channelBusy(uint16_t listenMs = 120);
    bool transmitWithLBT(const LoRaPayload &payload, uint8_t maxAttempts = 3);
    bool transmitWithAck(const LoRaPayload &payload,
                         uint16_t ackTimeoutMs = 600,
                         uint8_t  maxRetries   = 4);
    bool sendAck(uint8_t targetNodeId, uint32_t ackedCounter);
};

#endif // LORA_P2P_ADAPTER_H