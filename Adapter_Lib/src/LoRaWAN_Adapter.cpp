#include "LoRaWAN_Adapter.h"

LoRaWAN::LoRaWAN(uint8_t rst_pin, uint8_t rx_pin, uint8_t tx_pin, HardwareSerial &serial,
                 const char *devEUI, const char *appEUI, const char *appKey, Stream *debugSerial)
    : _loraSerial(serial), _rst_pin(rst_pin), _rx_pin(rx_pin), _tx_pin(tx_pin),
      _devEUI(devEUI), _appEUI(appEUI), _appKey(appKey), _debugSerial(debugSerial), _isJoined(false) {}

/************************
 * Private Functions
 *************************/

void LoRaWAN::debugPrint(const char *msg)
{
    if (_debugSerial)
    {
        _debugSerial->print("[LoRaWAN] ");
        _debugSerial->println(msg);
    }
}

void LoRaWAN::debugPrint(const String &msg)
{
    if (_debugSerial)
    {
        _debugSerial->print("[LoRaWAN] ");
        _debugSerial->println(msg);
    }
}

/************************
 * Private Functions
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

                // If we got a definite error response, fail immediately
                if (response.startsWith("invalid") || response.equals("busy") || response.equals("not_joined"))
                {
                    return false;
                }
            }
        }
        delay(10);
    }

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
                // Check if we got what we expected
                if (response.equals(expected))
                {
                    return true;
                }

                // Check for explicit failure responses
                if (response.equals("denied") || response.equals("mac_err"))
                {
                    return false;
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
 * Public Functions
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
    // Initialize serial communication
    _loraSerial.begin(57600, SERIAL_8N1, _rx_pin, _tx_pin);
    _loraSerial.setTimeout(2000);

    // Hardware reset
    reset();

    // Check module version
    if (!sendCmd("sys get ver", nullptr))
    {
        debugPrint("Failed to get module version");
        return false;
    }

    // Reset MAC layer to factory defaults
    if (!sendCmd("mac reset 868"))
    {
        debugPrint("Failed to reset MAC");
        return false;
    }

    delay(500);

    // Configure LoRaWAN parameters for TTN (EU868)
    String devEUICmd = "mac set deveui " + _devEUI;
    if (!sendCmd(devEUICmd.c_str()))
    {
        debugPrint("Failed to set DevEUI");
        return false;
    }

    String appEUICmd = "mac set appeui " + _appEUI;
    if (!sendCmd(appEUICmd.c_str()))
    {
        debugPrint("Failed to set AppEUI");
        return false;
    }

    String appKeyCmd = "mac set appkey " + _appKey;
    if (!sendCmd(appKeyCmd.c_str()))
    {
        debugPrint("Failed to set AppKey");
        return false;
    }

    // Configure for TTN EU868
    sendCmd("mac set adr off");
    sendCmd("mac set pwridx 1");
    sendCmd("mac set dr 5");
    sendCmd("mac set rx2 3 869525000");

    return true;
}

bool LoRaWAN::join()
{
    // Clear any pending data
    while (_loraSerial.available())
        _loraSerial.read();

    // Start OTAA join
    if (!sendCmd("mac join otaa", "ok"))
    {
        debugPrint("Failed to start join procedure");
        return false;
    }

    // Wait for join acceptance
    if (waitForResponse("accepted", 30000))
    {
        _isJoined = true;
        return true;
    }

    debugPrint("Join failed");
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
        debugPrint("Error: Not joined to network");
        return false;
    }

    // Build transmission command
    String txType = confirmed ? "cnf" : "uncnf";
    String cmd = "mac tx " + txType + " " + String(port) + " " + hexData;

    // Send command
    if (!sendCmd(cmd.c_str(), "ok"))
    {
        debugPrint("Failed to initiate transmission");
        return false;
    }

    // Wait for transmission result
    uint32_t timeout = confirmed ? 40000 : 15000;

    if (waitForResponse("mac_tx_ok", timeout))
    {
        return true;
    }

    debugPrint("Transmission failed");
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
        delay(100);
        while (_loraSerial.available())
            _loraSerial.read();

        if (!_isJoined)
        {
            debugPrint("Error: Not joined to network");
            return false;
        }

        // Build and send command
        String hexData = bytesToHex((const uint8_t *)&payload, sizeof(LoRaWANPayload));
        String cmd = "mac tx uncnf 1 " + hexData;

        if (sendCmd(cmd.c_str(), "ok", 2000))
        {
            // Wait for module to complete transmission
            unsigned long startTime = millis();
            while (millis() - startTime < 15000)
            {
                if (_loraSerial.available())
                {
                    _loraSerial.readStringUntil('\n');
                }
                delay(50);
            }

            delay(500);
            while (_loraSerial.available())
                _loraSerial.read();

            // Test module responsiveness
            _loraSerial.println("sys get ver");
            delay(300);

            bool moduleAlive = false;
            while (_loraSerial.available())
            {
                String resp = _loraSerial.readStringUntil('\n');
                resp.trim();
                if (resp.length() > 0)
                {
                    moduleAlive = true;
                }
            }

            if (!moduleAlive)
            {
                debugPrint("Module unresponsive - resetting");
                reset();
                delay(2000);

                if (!init() || !join())
                {
                    debugPrint("Failed to recover module");
                    return false;
                }
            }

            return true;
        }

        if (attempt < maxRetries)
        {
            delay(retryDelay);
        }
    }

    return false;
}

// Predefined Messages

bool LoRaWAN::sendBlackoutAlert(uint8_t batteryLevel)
{
    LoRaWANPayload payload;
    payload.messageType = LORAWAN_MSG_BLACKOUT;
    payload.timestamp = millis() / 1000;
    payload.batteryLevel = batteryLevel;

    return sendCriticalPayload(payload, 3, 15000);
}

bool LoRaWAN::sendBackendUnreachableAlert(uint8_t batteryLevel)
{
    LoRaWANPayload payload;
    payload.messageType = LORAWAN_MSG_BACKEND_UNREACHABLE;
    payload.timestamp = millis() / 1000;
    payload.batteryLevel = batteryLevel;

    return sendCriticalPayload(payload, 3, 15000);
}

bool LoRaWAN::sendPowerRestored(uint8_t batteryLevel)
{
    LoRaWANPayload payload;
    payload.messageType = LORAWAN_MSG_RESTORED;
    payload.timestamp = millis() / 1000;
    payload.batteryLevel = batteryLevel;

    return sendPayload(payload, 1, false);
}

// Power management
void LoRaWAN::shutdown()
{
    pinMode(_rst_pin, OUTPUT);
    digitalWrite(_rst_pin, LOW);
    _isJoined = false;
}

void LoRaWAN::wakeup()
{
    pinMode(_rst_pin, OUTPUT);
    digitalWrite(_rst_pin, HIGH);
    delay(500);

    while (_loraSerial.available())
        _loraSerial.read();
}
