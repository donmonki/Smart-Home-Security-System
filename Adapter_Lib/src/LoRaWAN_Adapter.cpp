#include "LoRaWAN_Adapter.h"

// Constructor
LoRaWAN::LoRaWAN(uint8_t rst_pin, uint8_t rx_pin, uint8_t tx_pin, HardwareSerial &serial,
                 const char *devEUI, const char *appEUI, const char *appKey)
    : _loraSerial(serial), _rst_pin(rst_pin), _rx_pin(rx_pin), _tx_pin(tx_pin),
      _devEUI(devEUI), _appEUI(appEUI), _appKey(appKey), _isJoined(false) {}

/************************
 * Private Helper Methods
 *************************/

bool LoRaWAN::sendCmd(const char *cmd, const char *expected, uint32_t timeout)
{
    _loraSerial.println(cmd);

    unsigned long startTime = millis();
    String response = "";

    while (millis() - startTime < timeout)
    {
        if (_loraSerial.available())
        {
            response = _loraSerial.readStringUntil('\n');
            response.trim();

            if (response.length() > 0)
            {
                Serial.print("Response: ");
                Serial.println(response);

                // If no specific response expected, just check it's not an error
                if (expected == nullptr)
                {
                    return !response.startsWith("invalid");
                }

                // Check for expected response
                if (response.equals(expected))
                {
                    return true;
                }
            }
        }
        delay(10);
    }

    Serial.println("Command timeout or unexpected response");
    return false;
}

bool LoRaWAN::waitForResponse(const char *expected, uint32_t timeout)
{
    unsigned long startTime = millis();

    while (millis() - startTime < timeout)
    {
        if (_loraSerial.available())
        {
            String response = _loraSerial.readStringUntil('\n');
            response.trim();

            if (response.length() > 0)
            {
                Serial.print("Async Response: ");
                Serial.println(response);

                if (response.equals(expected))
                {
                    return true;
                }
            }
        }
        delay(100);
    }

    return false;
}

String LoRaWAN::bytesToHex(const uint8_t *data, int length)
{
    String hexStr = "";
    for (int i = 0; i < length; i++)
    {
        if (data[i] < 0x10)
            hexStr += "0";
        hexStr += String(data[i], HEX);
    }
    hexStr.toUpperCase();
    return hexStr;
}

/************************
 * Public Methods
 *************************/

void LoRaWAN::reset()
{
    // Hardware reset
    pinMode(_rst_pin, OUTPUT);
    digitalWrite(_rst_pin, LOW);
    delay(200);
    digitalWrite(_rst_pin, HIGH);
    delay(500);

    // Clear serial buffer
    while (_loraSerial.available())
        _loraSerial.read();
}

bool LoRaWAN::init()
{
    Serial.println("Initializing LoRaWAN module...");

    // Initialize serial communication
    _loraSerial.begin(57600, SERIAL_8N1, _rx_pin, _tx_pin);
    _loraSerial.setTimeout(2000);

    // Hardware reset
    reset();

    // Check module version
    if (!sendCmd("sys get ver", nullptr))
    {
        Serial.println("Failed to get module version");
        return false;
    }

    // Reset MAC layer to factory defaults
    if (!sendCmd("mac reset 868"))
    {
        Serial.println("Failed to reset MAC");
        return false;
    }

    // Configure LoRaWAN parameters for TTN (EU868)
    // Set DevEUI
    String devEUICmd = "mac set deveui " + _devEUI;
    if (!sendCmd(devEUICmd.c_str()))
    {
        Serial.println("Failed to set DevEUI");
        return false;
    }

    // Set AppEUI
    String appEUICmd = "mac set appeui " + _appEUI;
    if (!sendCmd(appEUICmd.c_str()))
    {
        Serial.println("Failed to set AppEUI");
        return false;
    }

    // Set AppKey
    String appKeyCmd = "mac set appkey " + _appKey;
    if (!sendCmd(appKeyCmd.c_str()))
    {
        Serial.println("Failed to set AppKey");
        return false;
    }

    // Configure for TTN EU868
    sendCmd("mac set adr on");          // Enable Adaptive Data Rate
    sendCmd("mac set rx2 3 869525000"); // RX2 frequency for TTN EU

    Serial.println("LoRaWAN module initialized successfully");
    return true;
}

bool LoRaWAN::join()
{
    Serial.println("Attempting OTAA join...");

    // Clear any pending data
    while (_loraSerial.available())
        _loraSerial.read();

    // Start OTAA join
    if (!sendCmd("mac join otaa", "ok"))
    {
        Serial.println("Failed to start join procedure");
        return false;
    }

    // Wait for join acceptance (can take 5-10 seconds)
    Serial.println("Waiting for network acceptance...");
    if (waitForResponse("accepted", 30000))
    {
        Serial.println("Successfully joined TTN!");
        _isJoined = true;
        return true;
    }
    else if (waitForResponse("denied", 1000))
    {
        Serial.println("Join denied by network");
        _isJoined = false;
        return false;
    }

    Serial.println("Join timeout - no response from network");
    _isJoined = false;
    return false;
}

String LoRaWAN::getDevEUI()
{
    _loraSerial.println("sys get hweui");
    String response = _loraSerial.readStringUntil('\n');
    response.trim();
    return response;
}

bool LoRaWAN::sendHex(const String &hexData, uint8_t port, bool confirmed)
{
    if (!_isJoined)
    {
        Serial.println("Error: Not joined to network");
        return false;
    }

    // Build transmission command
    String txType = confirmed ? "cnf" : "uncnf"; // confirmed or unconfirmed
    String cmd = "mac tx " + txType + " " + String(port) + " " + hexData;

    Serial.print("Sending: ");
    Serial.println(cmd);

    // Send command
    if (!sendCmd(cmd.c_str(), "ok"))
    {
        Serial.println("Failed to initiate transmission");
        return false;
    }

    // Wait for transmission result
    if (waitForResponse("mac_tx_ok", 15000))
    {
        Serial.println("Transmission successful");
        return true;
    }
    else if (waitForResponse("mac_err", 1000))
    {
        Serial.println("Transmission error");
        return false;
    }

    Serial.println("Transmission timeout");
    return false;
}

bool LoRaWAN::sendRaw(const uint8_t *data, uint8_t length, uint8_t port, bool confirmed)
{
    String hexData = bytesToHex(data, length);
    return sendHex(hexData, port, confirmed);
}

bool LoRaWAN::sendPayload(const LoRaWANPayload &payload, uint8_t port, bool confirmed)
{
    return sendRaw((const uint8_t *)&payload, sizeof(LoRaWANPayload), port, confirmed);
}

bool LoRaWAN::sendCriticalPayload(const LoRaWANPayload &payload, uint8_t maxRetries, uint32_t retryDelay)
{
    for (uint8_t attempt = 1; attempt <= maxRetries; attempt++)
    {
        Serial.printf("Critical transmission attempt %d/%d...\n", attempt, maxRetries);

        // Use confirmed transmission for critical alerts
        if (sendPayload(payload, 1, true))
        {
            Serial.println("✓ Critical alert confirmed by network!");
            return true;
        }

        // If not last attempt, wait before retrying
        if (attempt < maxRetries)
        {
            Serial.printf("✗ Attempt %d failed, retrying in %lu seconds...\n",
                          attempt, retryDelay / 1000);
            delay(retryDelay);
        }
    }

    Serial.println("✗ All transmission attempts failed - check network coverage");
    return false;
}

// Predefined Messages

bool LoRaWAN::sendBlackoutAlert(uint8_t batteryLevel)
{
    Serial.println("========================================");
    Serial.println("CRITICAL: Sending blackout alert");
    Serial.println("========================================");

    LoRaWANPayload payload;
    payload.messageType = LORAWAN_MSG_BLACKOUT;
    payload.timestamp = millis() / 1000; // Simple timestamp (seconds since boot)
    payload.batteryLevel = batteryLevel;

    // Use critical send with retry logic (max 3 attempts, 10 sec between retries)
    return sendCriticalPayload(payload, 3, 10000);
}

bool LoRaWAN::sendPowerRestored(uint8_t batteryLevel)
{
    Serial.println("Sending power restored notification...");

    LoRaWANPayload payload;
    payload.messageType = LORAWAN_MSG_RESTORED;
    payload.timestamp = millis() / 1000;
    payload.batteryLevel = batteryLevel;

    // Unconfirmed transmission is fine for restoration notification
    return sendPayload(payload, 1, false);
}
