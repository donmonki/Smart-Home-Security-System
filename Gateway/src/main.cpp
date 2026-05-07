#include <Arduino.h>
#include <LoRaP2P_Adapter.h>
#include <MQTT_Adapter.h>
#include <LoRaWAN_Adapter.h>

// ----------------------------------------------------------------
// Pin Definitions - P2P LoRa module (UART2)
// ----------------------------------------------------------------
#define LORA_RST_PIN 23
#define LORA_RX_PIN  18
#define LORA_TX_PIN  19

// ----------------------------------------------------------------
// Pin Definitions - LoRaWAN fallback module (UART1)
// ----------------------------------------------------------------
#define LORAWAN_RST_PIN 22
#define LORAWAN_RX_PIN  2
#define LORAWAN_TX_PIN  4

// HIGH = external power present, LOW = running on battery backup
#define POWER_SENSE_PIN 34

// ----------------------------------------------------------------
// WiFi / MQTT Credentials — update before deployment
// ----------------------------------------------------------------
#define WIFI_SSID      "Pixel 7 Pro"
#define WIFI_PASSWORD  "lookontherouter"
#define MQTT_SERVER    "10.128.202.9"
#define MQTT_PORT      8883
#define MQTT_CLIENT_ID "gateway-1"

// ----------------------------------------------------------------
// TTN Credentials — update before deployment
// ----------------------------------------------------------------
#define TTN_DEV_EUI "0004A30B0103EDFA"
#define TTN_APP_EUI "1234567890ABCDE0"
#define TTN_APP_KEY "0BB350EC15ED31F52F37E3892169818E"

// ----------------------------------------------------------------
// Node Health Tracking
// ----------------------------------------------------------------
#define MAX_NODES                8
#define HEARTBEAT_TIMEOUT_MS     30000  // 30s without heartbeat → node considered offline
#define HEALTH_CHECK_INTERVAL_MS 5000
#define TELEMETRY_INTERVAL_MS    60000  // publish gateway telemetry every 60s

#define GATEWAY_BATTERY_LEVEL    90    // TODO: replace with real ADC reading

// ----------------------------------------------------------------
// LoRaWAN Alert Thresholds
// ----------------------------------------------------------------
#define MQTT_FAIL_ALERT_THRESHOLD 5    // consecutive publish failures before LoRaWAN alert

// ----------------------------------------------------------------
// LoRa Airtime Budget — guards EU 868 MHz 1% duty-cycle limit
// ----------------------------------------------------------------
#define AIRTIME_PER_TX_MS    90        // approx. SF7 / 125 kHz / 16-byte airtime
#define AIRTIME_WINDOW_MS    3600000UL // 1 hour
#define AIRTIME_BUDGET_MS    28800UL   // 0.8% of an hour — leaves headroom under the 1% cap
#define AIRTIME_SLOT_COUNT   512

struct NodeStatus
{
    uint8_t  nodeId;
    uint32_t lastSeenMs;
    uint32_t lastHeartbeatMs;  // updated only on HEARTBEAT; drives offline detection
    uint32_t lastMsgCounter;   // last processed counter; used to suppress retransmissions
    bool     online;
    bool     seenCounter;      // false until first message counter is recorded
};

NodeStatus nodeRegistry[MAX_NODES];
uint8_t    nodeCount = 0;
bool       loraReady    = false;
bool       lorawanReady = false;

// LoRaWAN alert state — prevents repeated transmissions for the same event
static uint8_t mqttFailCount      = 0;
static bool    backendAlertSent   = false;
static bool    externalPower      = true;   // tracks last known power state
static bool    blackoutAlertSent  = false;

// Sliding 1 h ring buffer of LoRa TX timestamps for duty-cycle accounting.
static uint32_t airtimeLog[AIRTIME_SLOT_COUNT] = {0};
static uint16_t airtimeHead  = 0;
static uint16_t airtimeCount = 0;

static bool airtimeWouldExceed()
{
    uint32_t now  = millis();
    uint32_t used = 0;
    for (uint16_t i = 0; i < airtimeCount; i++)
    {
        uint16_t idx = (airtimeHead + AIRTIME_SLOT_COUNT - 1 - i) % AIRTIME_SLOT_COUNT;
        if (now - airtimeLog[idx] > AIRTIME_WINDOW_MS) break;
        used += AIRTIME_PER_TX_MS;
    }
    return (used + AIRTIME_PER_TX_MS) > AIRTIME_BUDGET_MS;
}

static void airtimeRecord()
{
    airtimeLog[airtimeHead] = millis();
    airtimeHead = (airtimeHead + 1) % AIRTIME_SLOT_COUNT;
    if (airtimeCount < AIRTIME_SLOT_COUNT) airtimeCount++;
}

// ----------------------------------------------------------------
// Adapter Instances
// ----------------------------------------------------------------
HardwareSerial loraSerial(2);
HardwareSerial lorawanSerial(1);
LoraP2P     lora(LORA_RST_PIN, LORA_RX_PIN, LORA_TX_PIN, loraSerial);
MqttAdapter mqtt(WIFI_SSID, WIFI_PASSWORD, MQTT_SERVER, MQTT_PORT, MQTT_CLIENT_ID);
LoRaWAN     lorawan(LORAWAN_RST_PIN, LORAWAN_RX_PIN, LORAWAN_TX_PIN, lorawanSerial,
                    TTN_DEV_EUI, TTN_APP_EUI, TTN_APP_KEY, &Serial);

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
        if (lora.transmitWithLBT(command))
        {
            airtimeRecord();
        }
        else
        {
            Serial.println("[Gateway] WARNING: command TX failed");
        }
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
        nodeRegistry[nodeCount++] = {nodeId, millis(), 0, 0, true, false};
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
        // Use the heartbeat timestamp when available; fall back to lastSeenMs for
        // nodes that have not yet sent a dedicated heartbeat packet.
        uint32_t ref = nodeRegistry[i].lastHeartbeatMs != 0
                       ? nodeRegistry[i].lastHeartbeatMs
                       : nodeRegistry[i].lastSeenMs;

        if (nodeRegistry[i].online && (now - ref) > HEARTBEAT_TIMEOUT_MS)
        {
            nodeRegistry[i].online = false;
            mqtt.publishNodeOffline(nodeRegistry[i].nodeId);
        }
    }
}

// ----------------------------------------------------------------
// LoRaWAN Emergency Transmit — wake, join, send, shut back down
// ----------------------------------------------------------------
enum LoRaWANAlert { ALERT_BLACKOUT, ALERT_BACKEND_UNREACHABLE, ALERT_POWER_RESTORED };

bool sendLoRaWANAlert(LoRaWANAlert alert)
{
    lorawan.wakeup();

    if (!lorawan.init() || !lorawan.join())
    {
        Serial.println("[LoRaWAN] Failed to re-join TTN for alert");
        lorawan.shutdown();
        return false;
    }

    bool sent = false;
    switch (alert)
    {
    case ALERT_BLACKOUT:
        sent = lorawan.sendBlackoutAlert(GATEWAY_BATTERY_LEVEL);
        break;
    case ALERT_BACKEND_UNREACHABLE:
        sent = lorawan.sendBackendUnreachableAlert(GATEWAY_BATTERY_LEVEL);
        break;
    case ALERT_POWER_RESTORED:
        sent = lorawan.sendPowerRestored(GATEWAY_BATTERY_LEVEL);
        break;
    }

    lorawan.shutdown();
    return sent;
}

// ----------------------------------------------------------------
// Message Routing — translates LoRa payloads to MQTT events
// ----------------------------------------------------------------
void routePayload(const LoRaPayload &payload)
{
    updateNodeLastSeen(payload.nodeId);

    // Per-node deduplication: detect retransmissions by comparing the incoming
    // counter against the last processed one.  A retransmission still receives
    // an ACK (to stop the node from sending more) but its payload is not
    // forwarded to MQTT a second time, keeping airtime and backend load down.
    //
    // This is the primary mitigation for the airtime-exhaustion / burst loop:
    // even when we cannot ACK (budget exceeded) the duplicate suppression
    // prevents the backend from seeing the same event repeatedly, and once the
    // budget recovers the first ACK silences the node.
    bool isDuplicate = false;
    for (int i = 0; i < nodeCount; i++)
    {
        if (nodeRegistry[i].nodeId != payload.nodeId) continue;

        if (payload.msgType == LORA_MSG_HEARTBEAT)
            nodeRegistry[i].lastHeartbeatMs = millis();

        if (nodeRegistry[i].seenCounter &&
            nodeRegistry[i].lastMsgCounter == payload.data.sensorData.messageCounter)
        {
            isDuplicate = true;
        }
        else
        {
            nodeRegistry[i].lastMsgCounter = payload.data.sensorData.messageCounter;
            nodeRegistry[i].seenCounter    = true;
        }
        break;
    }

    // ACK alarm/RFID uplinks (including retransmissions) so the node stops sending.
    // Heartbeats are intentionally not acked — missed heartbeats are handled by
    // the offline-timeout in checkNodeHealth.
    if (payload.msgType == LORA_MSG_MOTION_ALARM ||
        payload.msgType == LORA_MSG_RFID_SCANNED)
    {
        if (airtimeWouldExceed())
        {
            Serial.printf("[Gateway] Skipping ACK for node %d — airtime budget exhausted\n",
                          payload.nodeId);
        }
        else if (lora.sendAck(payload.nodeId, payload.data.sensorData.messageCounter))
        {
            airtimeRecord();
        }
        else
        {
            Serial.printf("[Gateway] WARNING: ACK TX failed for node %d (counter %lu)\n",
                          payload.nodeId,
                          (unsigned long)payload.data.sensorData.messageCounter);
        }
    }

    if (isDuplicate)
    {
        Serial.printf("[Gateway] Duplicate from node %d (counter %lu) — ACK sent, payload suppressed\n",
                      payload.nodeId, (unsigned long)payload.data.sensorData.messageCounter);
        return;
    }

    switch (payload.msgType)
    {
    case LORA_MSG_HEARTBEAT:
        Serial.printf("[Gateway] HEARTBEAT from node %d | counter: %u\n",
                      payload.nodeId, payload.data.sensorData.messageCounter);
        break;
    case LORA_MSG_MOTION_ALARM:
        Serial.printf("[Gateway] MOTION ALARM from node %d | motion: %s | counter: %u\n",
                      payload.nodeId,
                      payload.data.sensorData.motionDetected ? "YES" : "NO",
                      payload.data.sensorData.messageCounter);
        break;
    case LORA_MSG_RFID_SCANNED:
        Serial.printf("[Gateway] RFID SCAN from node %d | uid: 0x%08X | counter: %u\n",
                      payload.nodeId,
                      payload.data.sensorData.rfidUid,
                      payload.data.sensorData.messageCounter);
        break;
    default:
        Serial.printf("[Gateway] UNKNOWN msg type 0x%02X from node %d\n",
                      payload.msgType, payload.nodeId);
        break;
    }

    bool published = mqtt.publishEvent(payload);
    Serial.printf("[Gateway] MQTT publish %s for node %d\n", published ? "OK" : "FAILED", payload.nodeId);

    if (published)
    {
        mqttFailCount    = 0;
        backendAlertSent = false;
    }
    else
    {
        mqttFailCount++;
        if (lorawanReady && !backendAlertSent && mqttFailCount >= MQTT_FAIL_ALERT_THRESHOLD)
        {
            Serial.println("[Gateway] MQTT publish failures exceeded threshold — sending LoRaWAN backend alert");
            if (sendLoRaWANAlert(ALERT_BACKEND_UNREACHABLE))
            {
                backendAlertSent = true;
            }
        }
    }
}

// ----------------------------------------------------------------
// Setup & Loop
// ----------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    Serial.println("[Gateway] Starting...");

    pinMode(POWER_SENSE_PIN, INPUT);
    externalPower = digitalRead(POWER_SENSE_PIN);

    loraReady = lora.moduleInit();
    if (loraReady)
    {
        Serial.println("[Gateway] LoRa initialized — listening for nodes...");
    }
    else
    {
        Serial.println("[Gateway] ERROR: LoRa initialization failed! LoRa receive disabled.");
    }

    lorawan.wakeup();
    if (lorawan.init() && lorawan.join())
    {
        lorawanReady = true;
        lorawan.shutdown();
        Serial.println("[Gateway] LoRaWAN verified — emergency fallback active (powered down).");
    }
    else
    {
        Serial.println("[Gateway] WARNING: LoRaWAN init/join failed — emergency fallback unavailable.");
    }

    mqtt.init(mqttCallback);
    Serial.println("[Gateway] MQTT adapter initialized.");
}

void loop()
{
    // Power state monitoring — triggers LoRaWAN blackout / restored alerts
    bool powerNow = digitalRead(POWER_SENSE_PIN);
    if (lorawanReady)
    {
        if (!powerNow && externalPower && !blackoutAlertSent)
        {
            Serial.println("[Gateway] External power lost — sending LoRaWAN blackout alert");
            if (sendLoRaWANAlert(ALERT_BLACKOUT))
            {
                blackoutAlertSent = true;
            }
        }
        else if (powerNow && !externalPower)
        {
            Serial.println("[Gateway] External power restored — sending LoRaWAN notification");
            sendLoRaWANAlert(ALERT_POWER_RESTORED);
            blackoutAlertSent = false;
        }
    }
    externalPower = powerNow;

    mqtt.alive_loop();

    if (loraReady)
    {
        LoRaPayload payload;
        if (lora.receivePayload(payload))
        {
            routePayload(payload);
        }
    }

    static uint32_t lastHealthCheck = 0;
    if (millis() - lastHealthCheck >= HEALTH_CHECK_INTERVAL_MS)
    {
        checkNodeHealth();
        lastHealthCheck = millis();
    }

    static uint32_t lastTelemetry = 0;
    if (millis() - lastTelemetry >= TELEMETRY_INTERVAL_MS)
    {
        mqtt.publishGatewayTelemetry(GATEWAY_BATTERY_LEVEL);
        lastTelemetry = millis();
    }

    delay(10);
}
