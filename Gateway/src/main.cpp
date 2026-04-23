#include <Arduino.h>
#include <LoRaP2P_Adapter.h>
#include <MQTT_Adapter.h>

// ----------------------------------------------------------------
// Pin Definitions - adjust to match your gateway hardware wiring
// ----------------------------------------------------------------
#define LORA_RST_PIN 23
#define LORA_RX_PIN  19
#define LORA_TX_PIN  18

// ----------------------------------------------------------------
// WiFi / MQTT Credentials — update before deployment
// ----------------------------------------------------------------
#define WIFI_SSID      "your-ssid"
#define WIFI_PASSWORD  "your-password"
#define MQTT_SERVER    "your-broker-ip"
#define MQTT_PORT      1883
#define MQTT_CLIENT_ID "gateway-1"

// ----------------------------------------------------------------
// Node Health Tracking
// ----------------------------------------------------------------
#define MAX_NODES                8
#define HEARTBEAT_TIMEOUT_MS     30000  // 30s without heartbeat → node considered offline
#define HEALTH_CHECK_INTERVAL_MS 5000
#define TELEMETRY_INTERVAL_MS    60000  // publish gateway telemetry every 60s

#define GATEWAY_BATTERY_LEVEL    100    // TODO: replace with real ADC reading

struct NodeStatus
{
    uint8_t  nodeId;
    uint32_t lastSeenMs;
    bool     online;
};

NodeStatus nodeRegistry[MAX_NODES];
uint8_t    nodeCount = 0;

// ----------------------------------------------------------------
// Adapter Instances
// ----------------------------------------------------------------
HardwareSerial loraSerial(2);
LoraP2P     lora(LORA_RST_PIN, LORA_RX_PIN, LORA_TX_PIN, loraSerial);
MqttAdapter mqtt(WIFI_SSID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_CLIENT_ID);

// ----------------------------------------------------------------
// MQTT Callback — called by PubSubClient on incoming backend messages
// ----------------------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    LoRaPayload command;
    mqtt.processIncomingMessage(topic, payload, length, command);
    if (command.msgType == LORA_MSG_COMMAND)
    {
        Serial.printf("[Gateway] Forwarding command (action %d) to node %d\n",
                      command.data.commandData.actionId, command.nodeId);
        lora.transmitPayload(command);
    }
}

// ----------------------------------------------------------------
// Node Registry
// ----------------------------------------------------------------
void updateNodeLastSeen(uint8_t nodeId)
{
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodeRegistry[i].nodeId == nodeId)
        {
            nodeRegistry[i].lastSeenMs = millis();
            nodeRegistry[i].online     = true;
            return;
        }
    }

    if (nodeCount < MAX_NODES)
    {
        nodeRegistry[nodeCount++] = {nodeId, millis(), true};
        Serial.printf("[Gateway] New node registered: %d\n", nodeId);
    }
    else
    {
        Serial.println("[Gateway] WARNING: node registry full, cannot register new node");
    }
}

void checkNodeHealth()
{
    uint32_t now = millis();
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodeRegistry[i].online &&
            (now - nodeRegistry[i].lastSeenMs) > HEARTBEAT_TIMEOUT_MS)
        {
            nodeRegistry[i].online = false;
            mqtt.publishNodeOffline(nodeRegistry[i].nodeId);
        }
    }
}

// ----------------------------------------------------------------
// Message Routing — translates LoRa payloads to MQTT events
// ----------------------------------------------------------------
void routePayload(const LoRaPayload &payload)
{
    updateNodeLastSeen(payload.nodeId);
    Serial.printf("[Gateway] Received msg type 0x%02X from node %d\n", payload.msgType, payload.nodeId);
    mqtt.publishEvent(payload);
}

// ----------------------------------------------------------------
// Setup & Loop
// ----------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    Serial.println("[Gateway] Starting...");

    if (lora.moduleInit())
    {
        Serial.println("[Gateway] LoRa initialized — listening for nodes...");
    }
    else
    {
        Serial.println("[Gateway] ERROR: LoRa initialization failed!");
        // TODO: signal failure (e.g., blink onboard LED) and retry
    }

    mqtt.init(mqttCallback);
    Serial.println("[Gateway] MQTT adapter initialized.");
}

void loop()
{
    // Keep MQTT connection alive and process incoming backend messages
    mqtt.alive_loop();

    // Receive and route incoming LoRa messages from nodes
    LoRaPayload payload;
    if (lora.receivePayload(payload))
    {
        routePayload(payload);
    }

    // Periodically check whether any nodes have gone silent
    static uint32_t lastHealthCheck = 0;
    if (millis() - lastHealthCheck >= HEALTH_CHECK_INTERVAL_MS)
    {
        checkNodeHealth();
        lastHealthCheck = millis();
    }

    // Periodically publish gateway telemetry to the backend
    static uint32_t lastTelemetry = 0;
    if (millis() - lastTelemetry >= TELEMETRY_INTERVAL_MS)
    {
        mqtt.publishGatewayTelemetry(GATEWAY_BATTERY_LEVEL);
        lastTelemetry = millis();
    }

    delay(10);
}
