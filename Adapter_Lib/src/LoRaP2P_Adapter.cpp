#include <LoRaP2P_Adapter.h>
#include "mbedtls/aes.h"

// Shared key for AES encryption/decryption (must be the same on both Tx and Rx devices)
const unsigned char AES_SECRET_KEY[16] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};

void encryptPayload(const LoRaPayload &inputData, unsigned char *outputBuffer16)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, AES_SECRET_KEY, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, (const unsigned char *)&inputData, outputBuffer16);
    mbedtls_aes_free(&aes);
}

bool decryptPayload(const unsigned char *inputBuffer16, LoRaPayload &outputData)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, AES_SECRET_KEY, 128);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, inputBuffer16, (unsigned char *)&outputData);
    mbedtls_aes_free(&aes);

    return true;
}

String bytesToHex(const unsigned char* data, int length) {
    String hexStr = "";
    for(int i = 0; i < length; i++) {
        if(data[i] < 0x10) hexStr += "0";
        hexStr += String(data[i], HEX);
    }
    hexStr.toUpperCase();
    return hexStr;
}

void hexToBytes(const String& hex, unsigned char* bytes) {
    for (int i = 0; i < hex.length(); i += 2) {
        String byteString = hex.substring(i, i + 2);
        bytes[i / 2] = (unsigned char) strtol(byteString.c_str(), NULL, 16);
    }
}

LoraP2P::LoraP2P(uint8_t rst_pin, uint8_t rx_pin, uint8_t tx_pin, HardwareSerial &serial)
    : _rst_pin(rst_pin), _rx_pin(rx_pin), _tx_pin(tx_pin), _loraSerial(serial) {}
/************************
 * Private Functions
*************************/
// Used for initializing the LoRa module and sending commands
bool LoraP2P::sendCmd(const char* cmd, const char* expected) {
    _loraSerial.println(cmd);
    String response = _loraSerial.readStringUntil('\n');
    response.trim();

    bool ok;
    if (expected == nullptr) {
        ok = (response.length() > 0 && !response.startsWith("invalid"));
    } else {
        ok = response.equals(expected);
    }

    Serial.printf("[LoRa] CMD: %-30s | RSP: \"%s\" | %s\n",
                  cmd, response.c_str(), ok ? "OK" : "FAIL");
    return ok;
}
/************************
 * Public Functions
*************************/
// Lora Module Initialization
bool LoraP2P::moduleInit() {
    Serial.printf("[LoRa] Initializing on RX=%d TX=%d RST=%d\n", _rx_pin, _tx_pin, _rst_pin);
    _loraSerial.begin(57600, SERIAL_8N1, _rx_pin, _tx_pin);
    _loraSerial.setTimeout(1000);

    Serial.println("[LoRa] Resetting module...");
    pinMode(_rst_pin, OUTPUT);
    digitalWrite(_rst_pin, LOW);
    delay(200);
    digitalWrite(_rst_pin, HIGH);
    delay(500);

    int flushed = 0;
    while (_loraSerial.available()) { _loraSerial.read(); flushed++; }
    if (flushed) Serial.printf("[LoRa] Flushed %d stale bytes from buffer\n", flushed);

    Serial.println("[LoRa] Sending init commands:");
    if (!sendCmd("sys get ver",          nullptr))          { Serial.println("[LoRa] FAILED at: sys get ver (no response — check wiring/baud)"); return false; }
    if (!sendCmd("mac pause",            nullptr))          { Serial.println("[LoRa] FAILED at: mac pause"); return false; }
    if (!sendCmd("radio set mod lora",   "ok"))             { Serial.println("[LoRa] FAILED at: radio set mod lora"); return false; }
    if (!sendCmd("radio set freq 866100000", "ok"))         { Serial.println("[LoRa] FAILED at: radio set freq"); return false; }
    if (!sendCmd("radio set pwr 14",     "ok"))             { Serial.println("[LoRa] FAILED at: radio set pwr"); return false; }
    if (!sendCmd("radio set sf sf7",     "ok"))             { Serial.println("[LoRa] FAILED at: radio set sf"); return false; }
    if (!sendCmd("radio set afcbw 41.7", "ok"))             { Serial.println("[LoRa] FAILED at: radio set afcbw"); return false; }
    if (!sendCmd("radio set rxbw 125",   "ok"))             { Serial.println("[LoRa] FAILED at: radio set rxbw"); return false; }
    if (!sendCmd("radio set prlen 8",    "ok"))             { Serial.println("[LoRa] FAILED at: radio set prlen"); return false; }
    if (!sendCmd("radio set crc on",     "ok"))             { Serial.println("[LoRa] FAILED at: radio set crc"); return false; }
    if (!sendCmd("radio set iqi off",    "ok"))             { Serial.println("[LoRa] FAILED at: radio set iqi"); return false; }
    if (!sendCmd("radio set cr 4/5",     "ok"))             { Serial.println("[LoRa] FAILED at: radio set cr"); return false; }
    if (!sendCmd("radio set wdt 60000",  "ok"))             { Serial.println("[LoRa] FAILED at: radio set wdt"); return false; }
    if (!sendCmd("radio set sync 12",    "ok"))             { Serial.println("[LoRa] FAILED at: radio set sync"); return false; }
    if (!sendCmd("radio set bw 125",     "ok"))             { Serial.println("[LoRa] FAILED at: radio set bw"); return false; }

    Serial.println("[LoRa] Module initialized successfully.");
    return true;
}

// Transmit method
bool LoraP2P::transmitHex(const String &hexData) {
    stopRx(); // abort any active RX before transmitting

    String cmd = "radio tx " + hexData;
    if (!sendCmd(cmd.c_str(), "ok")) {
        // "busy" means the module is still in RX mode (state tracking drifted).
        // Force it back to idle so the next call is not also stuck.
        _radioInRx = true;
        stopRx();
        return false;
    }

    // When "ok" is received, the module will transmit, and then return "radio_tx_ok"
    String txStatus = _loraSerial.readStringUntil('\n');
    txStatus.trim();
    return txStatus.equals("radio_tx_ok");
}

// Abort an active RX window so the radio is free to transmit or re-arm for LBT.
void LoraP2P::stopRx() {
    if (!_radioInRx) return;
    _loraSerial.println("radio rxstop");
    _loraSerial.readStringUntil('\n'); // consume "ok"
    // Module emits "radio_err" shortly after; drain it.
    delay(20);
    while (_loraSerial.available()) _loraSerial.read();
    _radioInRx = false;
}

// Receive method
String LoraP2P::receive() {
    // Replay any packet that was swallowed during a prior LBT probe
    if (_bufferedRx.length() > 0) {
        String cached = _bufferedRx;
        _bufferedRx = "";
        return cached;
    }

    // Only start reception if the module buffer is empty
    if (!_loraSerial.available()) {
        _loraSerial.println("radio rx 0");
    }
    // Read received response 
    String response = _loraSerial.readStringUntil('\n');
    response.trim();

    // Handle different responses with empty returns
    if (response.length() == 0) return "";
    if (response.equals("busy")) return "";            // Already listening
    if (response.equals("ok")) { _radioInRx = true; return ""; }   // Just armed
    if (response.equals("radio_err")) { _radioInRx = false; return ""; }

    // Data received successfully
    if (response.startsWith("radio_rx  ")) {
        _radioInRx = false;
        return response.substring(10);
    }

    return "";
}

//Encrypted Payload Transmission
bool LoraP2P::transmitPayload(const LoRaPayload &payload) {
    unsigned char encryptedBytes[16];
    
    // Encrypt the struct directly into a 16 byte array
    encryptPayload(payload, encryptedBytes);
    //Byte to Hex String conversion for transmission
    String hexStr = bytesToHex(encryptedBytes, 16);
    // Transmit method call with the hex string, returns true if successful, false otherwise
    return transmitHex(hexStr);
}

// Encrypted Payload Reception
bool LoraP2P::receivePayload(LoRaPayload &receivedData) {
    // Call Receive method to get the hex string
    String receivedHex = receive();


    // Check if we got a 32 character hex string (16 bytes)
    if (receivedHex.length() != 32) {
        return false;
    }

    // Convert the Hex into a 16-byte array
    unsigned char encryptedBytes[16];
    hexToBytes(receivedHex, encryptedBytes);

    // Decrypt the bytes directly into the provided struct
    return decryptPayload(encryptedBytes, receivedData);
}

// ----------------------------------------------------------------
// Collision-avoidance / reliability layer
// ----------------------------------------------------------------

// Listen-before-talk: arms the radio for listenMs, returns true if a frame
// was heard during the window. Any received hex is buffered so the caller's
// next receive() call still sees it.
bool LoraP2P::channelBusy(uint16_t listenMs) {
    if (listenMs < 30) listenMs = 30;

    // Abort any active RX so we can reconfigure the WDT and arm a fresh LBT window.
    stopRx();

    // Lower the receive watchdog to bound the listen window, then restore.
    char wdtCmd[32];
    snprintf(wdtCmd, sizeof(wdtCmd), "radio set wdt %u", (unsigned)listenMs);
    if (!sendCmd(wdtCmd, "ok")) {
        sendCmd("radio set wdt 60000", "ok");
        return false;
    }

    // Drain any stale UART bytes before arming RX.
    while (_loraSerial.available()) _loraSerial.read();

    _loraSerial.println("radio rx 0");
    String armed = _loraSerial.readStringUntil('\n');
    armed.trim();
    if (!armed.equals("ok")) {
        sendCmd("radio set wdt 60000", "ok");
        return false;
    }
    _radioInRx = true;

    // Wait up to listenMs (+ small slack) for either radio_rx or radio_err.
    _loraSerial.setTimeout((uint32_t)listenMs + 50);
    String response = _loraSerial.readStringUntil('\n');
    _loraSerial.setTimeout(1000);
    response.trim();
    _radioInRx = false;

    bool busy = false;
    if (response.startsWith("radio_rx  ")) {
        // Buffer so the application doesn't lose the packet.
        _bufferedRx = response.substring(10);
        busy = true;
    }

    sendCmd("radio set wdt 60000", "ok");
    return busy;
}

bool LoraP2P::transmitWithLBT(const LoRaPayload &payload, uint8_t maxAttempts) {
    if (maxAttempts == 0) maxAttempts = 1;
    for (uint8_t attempt = 0; attempt < maxAttempts; attempt++) {
        if (!channelBusy(120)) {
            return transmitPayload(payload);
        }
        // Exponential backoff with jitter: 50–800 ms, growing per attempt.
        uint32_t cap = (uint32_t)50 << attempt; // 50, 100, 200, 400, 800...
        if (cap > 800) cap = 800;
        uint32_t backoff = 50 + (uint32_t)random(0, cap);
        Serial.printf("[LoRa] LBT busy on attempt %u, backing off %lums\n",
                      attempt + 1, (unsigned long)backoff);
        delay(backoff);
    }
    Serial.println("[LoRa] LBT failed: channel busy after max attempts");
    return false;
}

bool LoraP2P::transmitWithAck(const LoRaPayload &payload,
                              uint16_t ackTimeoutMs,
                              uint8_t  maxRetries)
{
    for (uint8_t attempt = 0; attempt <= maxRetries; attempt++) {
        if (!transmitWithLBT(payload)) {
            // Channel could not be acquired; brief pause then retry.
            delay(50 + random(0, 200));
            continue;
        }

        uint32_t expectedCounter = payload.data.sensorData.messageCounter;
        uint32_t deadline = millis() + ackTimeoutMs;
        while ((int32_t)(deadline - millis()) > 0) {
            LoRaPayload rx;
            if (receivePayload(rx)
                && rx.msgType == LORA_MSG_ACK
                && rx.nodeId == payload.nodeId
                && rx.data.ackData.ackedMessageCounter == expectedCounter)
            {
                return true;
            }
            delay(10);
        }

        Serial.printf("[LoRa] ACK miss for counter %lu (attempt %u/%u)\n",
                      (unsigned long)expectedCounter,
                      attempt + 1, maxRetries + 1);
        // Backoff before retransmit so a colliding peer can clear.
        delay(100 + random(0, 300));
    }
    return false;
}

bool LoraP2P::sendAck(uint8_t targetNodeId, uint32_t ackedCounter) {
    LoRaPayload ack;
    ack.nodeId  = targetNodeId;
    ack.msgType = LORA_MSG_ACK;
    ack.data.ackData.ackedMessageCounter = ackedCounter;
    // Skip LBT: the channel is clear — the node just finished TX and switched to RX
    // waiting for this ACK. Adding a 120ms LBT probe here delays the ACK past the
    // node's timeout, causing it to retransmit and collide with our own ACK attempt.
    return transmitPayload(ack);
}

