#include <Arduino.h>
#include <HardwareSerial.h>
#include <LoRaP2P_Adapter.h>

// =====================================================
// ALARM NODE
// =====================================================

#define NODE_ID 2

#define LED_PIN 27

// RN2483A LoRa pins
#define LORA_RST 21
#define LORA_RX 32
#define LORA_TX 33

#define HEARTBEAT_INTERVAL_MS 25000
#define FLASH_INTERVAL_MS 250

HardwareSerial loraSerial(2);
LoraP2P lora(LORA_RST, LORA_RX, LORA_TX, loraSerial);

static uint32_t msgCounter = 0;
static uint32_t nextHeartbeatAt = 0;

static bool alarmActive = false;
static uint32_t alarmOffAt = 0; // 0 = no auto-off
static uint32_t lastFlashToggle = 0;
static bool ledState = false;

// =====================================================
// HEARTBEAT
// =====================================================

static void scheduleNextHeartbeat()
{
  int32_t jitter = random(
      -(int32_t)(HEARTBEAT_INTERVAL_MS / 5),
      (int32_t)(HEARTBEAT_INTERVAL_MS / 5));

  nextHeartbeatAt = millis() + HEARTBEAT_INTERVAL_MS + jitter;
}

static void sendHeartbeat()
{
  LoRaPayload hb;

  hb.nodeId = NODE_ID;
  hb.msgType = LORA_MSG_HEARTBEAT;
  hb.data.sensorData.messageCounter = ++msgCounter;

  if (lora.transmitWithLBT(hb))
  {
    Serial.printf("[Alarm Node %d] HEARTBEAT SENT #%lu\n",
                  NODE_ID,
                  (unsigned long)msgCounter);
  }
  else
  {
    Serial.println("[Alarm Node] Heartbeat skipped — channel busy");
  }
}

// =====================================================
// ALARM CONTROL
// =====================================================

static void activateAlarm(uint8_t durationSecs)
{
  alarmActive = true;
  lastFlashToggle = millis();

  if (durationSecs > 0)
  {
    alarmOffAt = millis() + (uint32_t)durationSecs * 1000;
    Serial.printf("[Alarm Node] ALARM ON (auto-off in %u s)\n", durationSecs);
  }
  else
  {
    alarmOffAt = 0;
    Serial.println("[Alarm Node] ALARM ON");
  }
}

static void deactivateAlarm()
{
  alarmActive = false;
  alarmOffAt = 0;
  ledState = false;
  digitalWrite(LED_PIN, LOW);
  Serial.println("[Alarm Node] ALARM OFF");
}

// =====================================================
// COMMAND HANDLER
// =====================================================

static void handleCommand(const LoRaPayload &cmd)
{
  uint8_t action = cmd.data.commandData.actionId;
  uint8_t param = cmd.data.commandData.parameter;

  switch (action)
  {
  case CMD_ALARM_ON:
    activateAlarm(param);
    break;

  case CMD_ALARM_OFF:
    deactivateAlarm();
    break;

  default:
    Serial.printf("[Alarm Node] Unknown action 0x%02X\n", action);
    break;
  }
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);
  randomSeed(esp_random());

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Initializing Alarm Node...");

  if (!lora.moduleInit())
  {
    Serial.println("[Alarm Node] LoRa init FAILED");
    while (true)
    {
      delay(1000);
    }
  }

  Serial.printf("[Alarm Node %d] READY\n", NODE_ID);

  scheduleNextHeartbeat();
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  // ---------------- RECEIVE COMMANDS ----------------

  LoRaPayload incoming;

  if (lora.receivePayload(incoming))
  {
    if (incoming.msgType == LORA_MSG_COMMAND &&
        incoming.nodeId == NODE_ID)
    {
      handleCommand(incoming);
    }
  }

  // ---------------- LED FLASH ----------------

  if (alarmActive)
  {
    if (alarmOffAt != 0 && (int32_t)(millis() - alarmOffAt) >= 0)
    {
      deactivateAlarm();
    }
    else if ((int32_t)(millis() - lastFlashToggle) >= FLASH_INTERVAL_MS)
    {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      lastFlashToggle = millis();
    }
  }

  // ---------------- HEARTBEAT ----------------

  if ((int32_t)(millis() - nextHeartbeatAt) >= 0)
  {
    sendHeartbeat();
    scheduleNextHeartbeat();
  }

  delay(10);
}