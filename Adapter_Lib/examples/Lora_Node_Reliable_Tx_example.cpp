// Recommended node-side pattern for talking to the gateway.
//
// Demonstrates the reliability protocol every node firmware should follow:
//   - Periodic HEARTBEATs sent via transmitWithLBT() with ±20% interval jitter
//   - Event-driven MOTION_ALARM sent via transmitWithAck() (4 retries, 600ms wait)
//   - Listening for downlink LORA_MSG_COMMAND from the gateway
//   - Ignoring LORA_MSG_ACK frames not addressed to this node
//
// See Library_User_Guide.md → "Reliable Communication Protocol" for the full contract.

#include <Arduino.h>
#include <HardwareSerial.h>
#include <LoRaP2P_Adapter.h>

// LoRa module pins
#define RST_PIN 23
#define RX_PIN  18
#define TX_PIN  19

// Identity of this node — must be unique on the network
#define NODE_ID 1

// Heartbeat cadence — actual interval is HEARTBEAT_INTERVAL_MS ± 20% jitter
#define HEARTBEAT_INTERVAL_MS 25000

// Simulated motion-sensor input
#define MOTION_PIN 13

HardwareSerial loraSerial(2);
LoraP2P        lora(RST_PIN, RX_PIN, TX_PIN, loraSerial);

static uint32_t msgCounter      = 0;
static uint32_t nextHeartbeatAt = 0;
static bool     lastMotionState = false;

static void scheduleNextHeartbeat()
{
    // ±20% jitter so multiple nodes don't drift onto the same boundary
    int32_t jitter = random(-(int32_t)(HEARTBEAT_INTERVAL_MS / 5),
                             (int32_t)(HEARTBEAT_INTERVAL_MS / 5));
    nextHeartbeatAt = millis() + HEARTBEAT_INTERVAL_MS + jitter;
}

static void sendHeartbeat()
{
    LoRaPayload hb;
    hb.nodeId  = NODE_ID;
    hb.msgType = LORA_MSG_HEARTBEAT;
    hb.data.sensorData.messageCounter = ++msgCounter;

    // Heartbeats: LBT only, no ACK — best-effort. The gateway's offline timeout
    // (30s) handles loss; retransmitting heartbeats wastes airtime.
    if (lora.transmitWithLBT(hb))
    {
        Serial.printf("[Node %d] HEARTBEAT #%lu sent\n", NODE_ID, (unsigned long)msgCounter);
    }
    else
    {
        Serial.println("[Node] Heartbeat TX skipped — channel busy");
    }
}

static void sendMotionAlarm(bool motion)
{
    LoRaPayload alarm;
    alarm.nodeId  = NODE_ID;
    alarm.msgType = LORA_MSG_MOTION_ALARM;
    alarm.data.sensorData.motionDetected  = motion;
    alarm.data.sensorData.messageCounter  = ++msgCounter;

    // Alarms: ACK + retry. transmitWithAck blocks up to ~600ms * (1 + retries)
    // waiting for the gateway's LORA_MSG_ACK matching this messageCounter.
    if (lora.transmitWithAck(alarm, /*ackTimeoutMs*/ 600, /*maxRetries*/ 4))
    {
        Serial.printf("[Node %d] MOTION ALARM #%lu acknowledged by gateway\n",
                      NODE_ID, (unsigned long)msgCounter);
    }
    else
    {
        Serial.printf("[Node %d] MOTION ALARM #%lu LOST — no ACK after retries\n",
                      NODE_ID, (unsigned long)msgCounter);
    }
}

static void handleDownlink()
{
    LoRaPayload rx;
    if (!lora.receivePayload(rx)) return;

    // Frames addressed to other nodes are not for us
    if (rx.nodeId != NODE_ID) return;

    switch (rx.msgType)
    {
    case LORA_MSG_COMMAND:
        Serial.printf("[Node %d] Command received: action=%d param=%d\n",
                      NODE_ID,
                      rx.data.commandData.actionId,
                      rx.data.commandData.parameter);
        // TODO: actuate alarm/buzzer here
        break;

    case LORA_MSG_ACK:
        // ACKs targeted at this node but arriving outside transmitWithAck's
        // wait window can be safely dropped — the helper already consumed the
        // ones it cared about.
        break;

    default:
        Serial.printf("[Node %d] Unexpected downlink msgType 0x%02X\n",
                      NODE_ID, rx.msgType);
        break;
    }
}

void setup()
{
    Serial.begin(115200);
    randomSeed(esp_random());
    pinMode(MOTION_PIN, INPUT);

    if (!lora.moduleInit())
    {
        Serial.println("[Node] LoRa init FAILED — halting");
        while (true) delay(1000);
    }
    Serial.printf("[Node %d] Ready. Reliable TX protocol active.\n", NODE_ID);
    scheduleNextHeartbeat();
}

void loop()
{
    // 1. Event-driven: detect motion edge and fire an acked alarm
    bool motionNow = digitalRead(MOTION_PIN);
    if (motionNow && !lastMotionState)
    {
        sendMotionAlarm(true);
    }
    lastMotionState = motionNow;

    // 2. Periodic: jittered heartbeat
    if ((int32_t)(millis() - nextHeartbeatAt) >= 0)
    {
        sendHeartbeat();
        scheduleNextHeartbeat();
    }

    // 3. Always service downlinks (commands + stray ACKs)
    handleDownlink();

    delay(10);
}
