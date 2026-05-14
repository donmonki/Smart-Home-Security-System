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
MIIDpDCCAoygAwIBAgIUTDn23twwz1MycshFdndD+HWU/uwwDQYJKoZIhvcNAQEL
BQAwajELMAkGA1UEBhMCVVMxDjAMBgNVBAgMBVN0YXRlMQ0wCwYDVQQHDARDaXR5
MRIwEAYDVQQKDAlTbWFydEhvbWUxETAPBgNVBAsMCFNlY3VyaXR5MRUwEwYDVQQD
DAxTbWFydEhvbWUtQ0EwHhcNMjYwNTE0MDg1NTM5WhcNMzYwNTExMDg1NTM5WjBq
MQswCQYDVQQGEwJVUzEOMAwGA1UECAwFU3RhdGUxDTALBgNVBAcMBENpdHkxEjAQ
BgNVBAoMCVNtYXJ0SG9tZTERMA8GA1UECwwIU2VjdXJpdHkxFTATBgNVBAMMDFNt
YXJ0SG9tZS1DQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANEdnwwV
/QX7BG0TcOrXT3W/jPanmcar9EPo+DJh7JQ9KOizJhfWfbu6+IHkhuh0/FuSFGbV
18SaeoNy3GvGvQoKhiBSwOitopil4Th9XMGiXTqDGUS2HbKF7gU4nI1/fTU2Inw0
vEQn9vMWN/iVO0yASEh2UGYOUJtcLuD/0bB5L1vC+aRMRXfoJI4m33a6LG0rCagZ
qShOat31zF2SBU2lyV2+cXyLlEheTDBBf5BBVonDOec89AztS41ZMAHepnwPseGK
2+UPOQWj3rljMqZ5rCKCHdcZxJ2mKQl4G/egCvy6huLIacHNXbU8Li8TR8dOswud
Nz1WPihDzGFTlZ8CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8E
BAMCAYYwHQYDVR0OBBYEFE9D9RFXzq1+5cj4afBOyEt36sjgMA0GCSqGSIb3DQEB
CwUAA4IBAQBU14nVBzlP7ttaz1eald7UWggo30u6wddpgKOlgnUhJuvrYUBCXqCG
+TEpj/+WSKotHFr77C68AM38kPmsuMpkdkavoWM7+uTWC8fQQOjctn92KsLj77Hi
vDywBHuiJpv+wUTxPkZImJtWJe1zLRLw76e6UjMnAHD6XpmIFPpblsexOiUc7yL8
YkqjPnkuFnjnno+LHVoJ1F5RL+RPgBnNA2Jk15GliNFBlj0gE1pjXgfYGKXqfcgV
gYfq1c8jdTovxxsW3fMl1X8EpRuXpG6mQz2uLsEbuhnRFiBuyPnD0FIxc8LeBO8l
blqO7Flr3mSCZNSOyw8v3fvqXCiaKjIx
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