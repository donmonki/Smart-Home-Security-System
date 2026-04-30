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
MIIDpDCCAoygAwIBAgIUCT/sHptcfOxK/vEAT3lqNnoZ8kQwDQYJKoZIhvcNAQEL
BQAwajELMAkGA1UEBhMCVVMxDjAMBgNVBAgMBVN0YXRlMQ0wCwYDVQQHDARDaXR5
MRIwEAYDVQQKDAlTbWFydEhvbWUxETAPBgNVBAsMCFNlY3VyaXR5MRUwEwYDVQQD
DAxTbWFydEhvbWUtQ0EwHhcNMjYwNDMwMDg1NTQwWhcNMzYwNDI3MDg1NTQwWjBq
MQswCQYDVQQGEwJVUzEOMAwGA1UECAwFU3RhdGUxDTALBgNVBAcMBENpdHkxEjAQ
BgNVBAoMCVNtYXJ0SG9tZTERMA8GA1UECwwIU2VjdXJpdHkxFTATBgNVBAMMDFNt
YXJ0SG9tZS1DQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAPS6xaIK
n6RJAgDYpmpoGQwXV94FSpwF0Ts/AUapw1oHTj2tpidjAmCTJzehtu59weny2Aq7
ac5/gFf+BDO7vMF3scWWcaXzDOPp/1slUDkUNzGu557HXamFTwYdhFvv/0XT/h/V
jg+Vl4dTYlrB3ESJ9EMx2nBgAogw8X+c6d4MhJUcXriSC/D668VgW9KAMLVvTAJP
23SNDNaWnbtz2a8UlEds0TPscy1VSoh73Zpk7NYPn9z8qad6Bmjwl6lYE9T3X38p
yiBTcxGLTp2ORR/1/6+/yiKh6CU3q2HIRnpV09PwGV6iWfcD31aja4njdumOair+
2YGPdCg4xDJeTOsCAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8E
BAMCAYYwHQYDVR0OBBYEFEKO1X1O7saVq3sz3svAJT0ixNSiMA0GCSqGSIb3DQEB
CwUAA4IBAQCM0/cmj9mV09dkn7RCOr2TaHZV2+k3sasM1tcRotfQNm35J0p3PkBI
Wuw1Dwt3P7CBIiKGiBVXUviSN7YiJsSgHm+EJzOHupf/SHY0fZmKGkvwUxUws+ob
98hpg3gpkMPS7u+OdC9iVPfs1hKUIjOJ58Q2kEarmxi5vndVE7dFeLtq/oommqVK
O5yA4LyDCqL3vKSHZv/jpa3oqE9O5JkMKsrmzE9Nd6lwvtaXJp/LJsaDmIYocs5B
W3M/WOeOAQBhTA104hmcKdV+8wypeCRQEB05eDLHNt2s8twhTd65o2ItqFUdHaNq
3vQ6asvo19WcQC+wAEDY5vI5zUMrmWEH
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