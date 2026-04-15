#ifndef MQTT_ADAPTER_H
#define MQTT_ADAPTER_H

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "LoRaP2P_Adapter.h"

class MqttAdapter {
private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    
    const char* _ssid;
    const char* _password;
    const char* _mqttServer;
    uint16_t _mqttPort;
    const char* _clientId;
    
    LoRaPayload _pendingCommand; 
    bool _hasPendingCommand;

    void reconnect();

public:
    MqttAdapter(const char* ssid, const char* password, const char* mqttServer, uint16_t mqttPort, const char* clientId);

    // Initialize WiFi and MQTT connection
    void init(MQTT_CALLBACK_SIGNATURE);

   // Maintaining connection 
    void alive_loop();

    // Publish methods tailored to your payload types
    bool publishNodeTelemetry(const LoRaPayload& payload);
    bool publishGatewayTelemetry(uint8_t gatewayBatteryLevel);
    bool publishAlarm(const LoRaPayload& payload);
    bool publishAuthentication(const LoRaPayload& payload);
    bool publishNodeOffline(uint8_t nodeId);
    void processIncomingMessage(char* topic, byte* payload, unsigned int length);
    bool getPendingCommand(uint8_t requestingNodeId, LoRaPayload &outPayload);
};

#endif