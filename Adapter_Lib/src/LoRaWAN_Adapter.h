#ifndef LORAWAN_ADAPTER_H
#define LORAWAN_ADAPTER_H

#include <Arduino.h>
#include <stdint.h>

enum LoRaWANMessageType : uint8_t
{
    LORAWAN_MSG_BLACKOUT = 0x01,
    LORAWAN_MSG_RESTORED = 0x02,
    LORAWAN_MSG_BACKEND_UNREACHABLE = 0x03,
    LORAWAN_MSG_CUSTOM = 0xFF
};

struct __attribute__((packed)) LoRaWANPayload
{
    uint8_t messageType;
    uint32_t timestamp;
    uint8_t batteryLevel;
    uint8_t reserved[2];

    LoRaWANPayload()
    {
        memset(this, 0, sizeof(LoRaWANPayload));
    }
};

class LoRaWAN
{
private:
    uint8_t _rst_pin;
    uint8_t _rx_pin;
    uint8_t _tx_pin;
    HardwareSerial &_loraSerial;
    Stream *_debugSerial;
    String _devEUI;
    String _appEUI;
    String _appKey;
    bool _isJoined;

    bool sendCmd(const char *cmd, const char *expected_response = "ok", uint32_t timeout = 5000);
    String bytesToHex(const uint8_t *data, int length);
    bool waitForResponse(const char *expected, uint32_t timeout);
    bool sendCriticalPayload(const LoRaWANPayload &payload, uint8_t maxRetries = 3, uint32_t retryDelay = 10000);
    void debugPrint(const char *msg);
    void debugPrint(const String &msg);

public:
    LoRaWAN(uint8_t rst_pin, uint8_t rx_pin, uint8_t tx_pin, HardwareSerial &serial,
            const char *devEUI, const char *appEUI, const char *appKey, Stream *debugSerial = nullptr);

    bool init();
    bool join();
    bool isJoined() const { return _isJoined; }

    bool sendPayload(const LoRaWANPayload &payload, uint8_t port = 1, bool confirmed = false);
    bool sendRaw(const uint8_t *data, uint8_t length, uint8_t port = 1, bool confirmed = false);
    bool sendHex(const String &hexData, uint8_t port = 1, bool confirmed = false);

    bool sendBlackoutAlert(uint8_t batteryLevel);
    bool sendPowerRestored(uint8_t batteryLevel);
    bool sendBackendUnreachableAlert(uint8_t batteryLevel);

    void reset();
    String getDevEUI();
    void shutdown();
    void wakeup();
};

#endif // LORAWAN_ADAPTER_H