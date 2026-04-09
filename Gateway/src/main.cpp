#include <Arduino.h>
#include <LoRaP2P_Adapter.h>

// ----------------------------------------------------------------
// Pin Definitions - adjust to match your gateway hardware wiring
// ----------------------------------------------------------------
#define LORA_RST_PIN 23
#define LORA_RX_PIN  18
#define LORA_TX_PIN  19

// ----------------------------------------------------------------
// Node Health Tracking
// ----------------------------------------------------------------
#define MAX_NODES            8
#define HEARTBEAT_TIMEOUT_MS 30000  // 30s without heartbeat → node considered offline
#define HEALTH_CHECK_INTERVAL_MS 5000

struct NodeStatus
{
    uint8_t  nodeId;
    uint32_t lastSeenMs;
    bool     online;
};

NodeStatus nodeRegistry[MAX_NODES];
uint8_t    nodeCount = 0;

// ----------------------------------------------------------------
// LoRa Adapter Instance
// ----------------------------------------------------------------
HardwareSerial loraSerial(2);
LoraP2P lora(LORA_RST_PIN, LORA_RX_PIN, LORA_TX_PIN, loraSerial);

// ----------------------------------------------------------------
// MQTT Forwarding Stubs
// These functions will be replaced once the MQTT adapter is ready.
// See Adapter_Lib docs for the planned MQTT adapter interface.
// ----------------------------------------------------------------
void mqttPublishMotionAlarm(const LoRaPayload &payload)
{
    // TODO: forward to backend topic home/security/alarm
    Serial.printf("[MQTT] Motion alarm from node %d\n", payload.nodeId);
}

void mqttPublishRfidScanned(const LoRaPayload &payload)
{
    // TODO: forward to backend topic home/security/rfid
    Serial.printf("[MQTT] RFID scan from node %d, UID: %u\n", payload.nodeId, payload.rfidUid);
}

void mqttPublishSensorUpdate(const LoRaPayload &payload)
{
    // TODO: forward to backend topic home/security/node/{id}/sensors
    Serial.printf("[MQTT] Sensor update from node %d — light: %d, sound: %d, battery: %d%%\n",
                  payload.nodeId, payload.lightLevel, payload.soundLevel, payload.batteryLevel);
}

void mqttPublishNodeOffline(uint8_t nodeId)
{
    // TODO: forward to backend topic home/security/node/{id}/status
    Serial.printf("[MQTT] Node %d went offline\n", nodeId);
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
            mqttPublishNodeOffline(nodeRegistry[i].nodeId);
        }
    }
}

// ----------------------------------------------------------------
// Message Routing — translates LoRa payloads to MQTT events
// ----------------------------------------------------------------
void routePayload(const LoRaPayload &payload)
{
    updateNodeLastSeen(payload.nodeId);

    switch (payload.msgType)
    {
        case MSG_HEARTBEAT:
            Serial.printf("[Gateway] Heartbeat from node %d (battery: %d%%)\n",
                          payload.nodeId, payload.batteryLevel);
            break;

        case MSG_MOTION_ALARM:
            Serial.printf("[Gateway] Motion alarm from node %d\n", payload.nodeId);
            mqttPublishMotionAlarm(payload);
            break;

        case MSG_RFID_SCANNED:
            Serial.printf("[Gateway] RFID scan from node %d, UID: %u\n",
                          payload.nodeId, payload.rfidUid);
            mqttPublishRfidScanned(payload);
            break;

        case MSG_SENSOR_UPDATE:
            mqttPublishSensorUpdate(payload);
            break;

        default:
            Serial.printf("[Gateway] Unknown message type 0x%02X from node %d\n",
                          payload.msgType, payload.nodeId);
            break;
    }
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

    // TODO: Initialize MQTT adapter when available
}

void loop()
{
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

    delay(10);
}
