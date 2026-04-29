#ifndef MQTT_ADAPTER_H
#define MQTT_ADAPTER_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "LoRaP2P_Adapter.h"
#include <time.h>

enum MqttDeviceStatus : uint8_t
{
    MQTT_DEVICE_OFFLINE = 0x0,
    MQTT_DEVICE_ONLINE = 0x1,
};

enum MqttMessageType : uint8_t
{
    MQTT_MSG_UNKNOWN = 0x00,
    MQTT_MSG_GATEWAY_TELEMETRY = 0x01,
    MQTT_MSG_HEARTBEAT = 0x02,
    MQTT_MSG_AUTHENTICATION = 0x03,
    MQTT_MSG_MOTIONALARM = 0x04,
};

//==================================================================================
// ROOT CA CERTIFICATE - Required for TLS/SSL connection to Mosquitto broker
//==================================================================================
// INSTRUCTIONS:
// 1. Run: cd backend-server/mosquitto && ./generate-certs.sh
// 2. Extract CA certificate: cat certs/ca.crt
// 3. Copy entire certificate content (including BEGIN/END lines)
// 4. Paste between R"EOF( and )EOF" below
//
// IMPORTANT FORMATTING NOTES:
// - Include the entire "-----BEGIN CERTIFICATE-----" line
// - Include the entire "-----END CERTIFICATE-----" line
// - Preserve all line breaks exactly as they appear
// - No extra whitespace at the beginning or end
// - Any formatting issues will cause TLS handshake failures
//
// VERIFICATION:
// After updating, verify certificate is valid:
// - Certificate chain matches server cert: openssl verify -CAfile certs/ca.crt certs/server.crt
// - Certificate format is correct: openssl x509 -in certs/ca.crt -text -noout
// - For debugging steps, see: backend-server/mosquitto/ESP32_GATEWAY_SETUP.md
static const char ROOT_CA_CERT[] PROGMEM = R"EOF(-----BEGIN CERTIFICATE-----
MIIDpDCCAoygAwIBAgIUGxdSblONuHS1rhGZKq0cXKd3m/QwDQYJKoZIhvcNAQEL
BQAwajELMAkGA1UEBhMCVVMxDjAMBgNVBAgMBVN0YXRlMQ0wCwYDVQQHDARDaXR5
MRIwEAYDVQQKDAlTbWFydEhvbWUxETAPBgNVBAsMCFNlY3VyaXR5MRUwEwYDVQQD
DAxTbWFydEhvbWUtQ0EwHhcNMjYwNDI5MjA1OTM5WhcNMzYwNDI2MjA1OTM5WjBq
MQswCQYDVQQGEwJVUzEOMAwGA1UECAwFU3RhdGUxDTALBgNVBAcMBENpdHkxEjAQ
BgNVBAoMCVNtYXJ0SG9tZTERMA8GA1UECwwIU2VjdXJpdHkxFTATBgNVBAMMDFNt
YXJ0SG9tZS1DQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAJqcGCTc
ZCA9F2h7ulwcPJYhKA6ojMHO+fJMgKZexSb3A02cyQxYA//b9ETUcsUFVtfqRrW/
KNSTl2egvcX2Czip/mOXArGm8PG//AjV9qe+3KVxx3b8ztgFCx9qkMVGlPQCDc2Y
uNuOpUbxV9Y1ANbgYjI0CHlEVd1Ucm4rCsm9wMZHuvGtv6u+SK6NypCDp9YL8IG/
D7J/R7m5bu+Du1hdQu8fr/E7eRAuYSSQlr+WAHzkNOV6fpTgFVRpacCYeq+5Es8s
sj3H9ofUuJapWu+Zle4HmCcaTMka+a+T2Qq5dz1tEUgee06cmMupgiwMmitPRa1e
jBYPYlbFOmdjuqECAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8E
BAMCAYYwHQYDVR0OBBYEFOHPr0bWX3ZhtH1COJpvvcQAY07dMA0GCSqGSIb3DQEB
CwUAA4IBAQBtG/EFRueIoEWh586hdVn81F5JZ2AOEIjUEzuUjEsUNz+8aGK11d/k
IVLOMuwaI+GdIHo5iWQJMeCj5MHcQaetAFA4Oaf17e2HaYw5d0PnCy6h4J+KYEDG
4e5mDiDuMtysUzIeEI/Hj7AiPOsHZa3Y/Wmd0AccYqzvU2DW6aqUWniOgxRp13at
rXsZWVTKQNzSGBfN0v3hy7Iv/j7VowjeOkBvDK/e9Kp1SVOR2eYT0BZUW0sIpxaI
qUBbYey4s+0B5b+5rzxFptzwp63pzmshcjtdugIdKlaUIv6kJlmo1W+xWiUf6kOb
QivGwbmBhEwfdRgcsMa4zuT2a8CG3TyR
-----END CERTIFICATE-----
)EOF";
//=======================================================================================================================

class MqttAdapter
{
private:
    WiFiClientSecure _wifiClient;
    PubSubClient _mqttClient;

    const char *_ssid;
    const char *_password;
    const char *_mqttServer;
    uint16_t _mqttPort;
    const char *_clientId;

    void reconnect();

public:
    MqttAdapter(const char *ssid, const char *password, const char *mqttServer, uint16_t mqttPort, const char *clientId);

    // Initialize WiFi and MQTT connection
    void init(MQTT_CALLBACK_SIGNATURE);

    // Maintaining connection
    void alive_loop();

    // Publish methods tailored to your payload types
    bool publishGatewayTelemetry(uint8_t gatewayBatteryLevel);
    bool publishNodeOffline(uint8_t nodeId);
    bool publishEvent(const LoRaPayload &payload);
    void processIncomingMessage(char *topic, byte *payload, unsigned int length, LoRaPayload &outPayload);

};

#endif // MQTT_ADAPTER_H