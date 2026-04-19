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
    : _loraSerial(serial), _rst_pin(rst_pin), _rx_pin(rx_pin), _tx_pin(tx_pin) {}
/************************
 * Private Functions
*************************/
// Used for initializing the LoRa module and sending commands
bool LoraP2P::sendCmd(const char* cmd, const char* expected) {
    _loraSerial.println(cmd);
    String response = _loraSerial.readStringUntil('\n');
    response.trim();
    
    if (expected == nullptr) {
        return (response.length() > 0 && !response.startsWith("invalid"));
    }
    return response.equals(expected);
}
/************************
 * Public Functions
*************************/
// Lora Module Initialization
bool LoraP2P::moduleInit() {
    _loraSerial.begin(57600, SERIAL_8N1, _rx_pin, _tx_pin);
    _loraSerial.setTimeout(1000);

    // Hardware Reset
    pinMode(_rst_pin, OUTPUT);
    digitalWrite(_rst_pin, LOW);
    delay(200);
    digitalWrite(_rst_pin, HIGH);
    delay(500);

    // Clear buffer
    while (_loraSerial.available()) _loraSerial.read();

    // Initialize Radio (Returns false immediately if any fail)
    if (!sendCmd("sys get ver", nullptr)) return false; 
    if (!sendCmd("mac pause", nullptr)) return false;   
    if (!sendCmd("radio set mod lora")) return false;
    if (!sendCmd("radio set freq 866100000")) return false;
    if (!sendCmd("radio set pwr 14")) return false;
    if (!sendCmd("radio set sf sf7")) return false;
    if (!sendCmd("radio set afcbw 41.7")) return false;
    if (!sendCmd("radio set rxbw 125")) return false;
    if (!sendCmd("radio set prlen 8")) return false;
    if (!sendCmd("radio set crc on")) return false;
    if (!sendCmd("radio set iqi off")) return false;
    if (!sendCmd("radio set cr 4/5")) return false;
    if (!sendCmd("radio set wdt 60000")) return false;
    if (!sendCmd("radio set sync 12")) return false;
    if (!sendCmd("radio set bw 125")) return false;

    return true; 
}

// Transmit method
bool LoraP2P::transmitHex(const String &hexData) {
    // Transmit format: "radio tx <hex_data>"
    String cmd = "radio tx " + hexData;
    
    if (!sendCmd(cmd.c_str(), "ok")) return false;
    
    // When "ok" is received, the module will transmit, and then return "radio_tx_ok"
    String txStatus = _loraSerial.readStringUntil('\n');
    txStatus.trim();
    return txStatus.equals("radio_tx_ok");
}

// Receive method
String LoraP2P::receive() {
    // Only start reception if the module buffer is empty
    if (!_loraSerial.available()) {
        _loraSerial.println("radio rx 0");
    }
    // Read received response 
    String response = _loraSerial.readStringUntil('\n');
    response.trim();

    // Handle different responses with empty returns
    if (response.length() == 0) return ""; 
    if (response.equals("busy")) return "";      // Module is actively listening
    if (response.equals("ok")) return "";        // Module just started listening
    if (response.equals("radio_err")) return ""; // Module error, retry next loop

    // Data received succesfully
    if (response.startsWith("radio_rx  ")) {
        // Return just the hex data payload (without "radio_rx  ")
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

