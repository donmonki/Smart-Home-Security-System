#ifndef MQTT_ADAPTER_H
#define MQTT_ADAPTER_H

#include <WiFi.h>
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
class MqttAdapter
{
private:
    WiFiClient _wifiClient;
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