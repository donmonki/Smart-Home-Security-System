#include <Arduino.h>
#include <HardwareSerial.h>
#include <LoRaP2P_Adapter.h>
#include "esp_sleep.h"

#define NODE_ID 2

#define LED_PIN 27

#define LORA_RST 21
#define LORA_RX 32
#define LORA_TX 33

#define HEARTBEAT_INTERVAL_MS 25000UL

// Deep sleep duration between command-listen windows (only when no alarm is active)
#define SLEEP_TIME_US 5000000ULL
#define SLEEP_TIME_MS (SLEEP_TIME_US / 1000ULL)

// Awake window to listen for incoming commands after each wakeup
#define LISTEN_WINDOW_MS 10000UL

#define FLASH_INTERVAL_MS 250UL

HardwareSerial loraSerial(2);
LoraP2P lora(LORA_RST, LORA_RX, LORA_TX, loraSerial);

// Persisted across deep sleep cycles
RTC_DATA_ATTR uint32_t msgCounter = 0;
RTC_DATA_ATTR uint32_t msUntilHeartbeat = 0;

static bool alarmActive = false;
static uint32_t alarmExpiresAt = 0;
static uint32_t lastFlashToggle = 0;
static bool ledState = false;

static void sendHeartbeat()
{
  LoRaPayload hb;

  hb.nodeId = NODE_ID;
  hb.msgType = LORA_MSG_HEARTBEAT;
  hb.data.sensorData.messageCounter = ++msgCounter;

  if (lora.transmitWithLBT(hb))
    Serial.printf("[Alarm Node %d] HEARTBEAT SENT #%lu\n", NODE_ID, (unsigned long)msgCounter);
  else
    Serial.println("[Alarm Node] Heartbeat skipped — channel busy");

  msUntilHeartbeat = HEARTBEAT_INTERVAL_MS;
}

static void activateAlarm(uint8_t durationSecs)
{
  alarmActive = true;
  lastFlashToggle = millis();

  if (durationSecs > 0)
  {
    alarmExpiresAt = millis() + (uint32_t)durationSecs * 1000;
    Serial.printf("[Alarm Node] ALARM ON for %u seconds\n", durationSecs);
  }
  else
  {
    alarmExpiresAt = 0;
    Serial.println("[Alarm Node] ALARM ON (indefinite)");
  }
}

static void deactivateAlarm()
{
  alarmActive = false;
  alarmExpiresAt = 0;
  ledState = false;

  digitalWrite(LED_PIN, LOW);

  Serial.println("[Alarm Node] ALARM OFF");
}

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

// Blocks until the alarm ends. Handles LED flashing, incoming commands,
// and heartbeats on schedule while the alarm is running.
static void runAlarmLoop()
{
  uint32_t heartbeatDue = millis() + msUntilHeartbeat;

  while (alarmActive)
  {
    // Heartbeat
    if ((int32_t)(millis() - heartbeatDue) >= 0)
    {
      sendHeartbeat(); // also resets msUntilHeartbeat in RTC RAM
      heartbeatDue = millis() + HEARTBEAT_INTERVAL_MS;
    }

    // Incoming commands
    LoRaPayload incoming;
    if (lora.receivePayload(incoming))
    {
      if (incoming.msgType == LORA_MSG_COMMAND && incoming.nodeId == NODE_ID)
      {
        Serial.println("[Alarm Node] Command received during alarm");
        handleCommand(incoming);
      }
    }

    // Timed alarm expiry
    if (alarmExpiresAt != 0 && (int32_t)(millis() - alarmExpiresAt) >= 0)
      deactivateAlarm();

    // LED flash
    if (alarmActive && (int32_t)(millis() - lastFlashToggle) >= (int32_t)FLASH_INTERVAL_MS)
    {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      lastFlashToggle = millis();
    }

    delay(10);
  }

  // Sync remaining time back to RTC RAM so the sleep-cycle accounting
  // stays accurate after the alarm ends.
  int32_t remaining = (int32_t)(heartbeatDue - millis());
  msUntilHeartbeat = (remaining > 0) ? (uint32_t)remaining : 0;
}

static void goToSleep()
{
  Serial.println("[Alarm Node] Going to deep sleep...");
  Serial.flush();
  delay(100);

  esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);
  esp_deep_sleep_start();
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  randomSeed(esp_random());

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Initializing Alarm Node...");

  if (!lora.moduleInit())
  {
    Serial.println("[Alarm Node] LoRa init FAILED");
    while (true)
      delay(1000);
  }

  Serial.printf("[Alarm Node %d] READY\n", NODE_ID);

  // Heartbeat
  if (msUntilHeartbeat > 0)
  {
    if (msUntilHeartbeat <= (uint32_t)SLEEP_TIME_MS)
      msUntilHeartbeat = 0;
    else
      msUntilHeartbeat -= (uint32_t)SLEEP_TIME_MS;
  }

  if (msUntilHeartbeat == 0)
    sendHeartbeat();

  // Command listen window
  uint32_t listenStart = millis();

  while (millis() - listenStart < LISTEN_WINDOW_MS)
  {
    LoRaPayload incoming;

    if (lora.receivePayload(incoming))
    {
      if (incoming.msgType == LORA_MSG_COMMAND && incoming.nodeId == NODE_ID)
      {
        Serial.println("[Alarm Node] Command received");
        handleCommand(incoming);

        // If an alarm was just triggered, hand off to the blocking alarm
        // loop — the node stays awake for as long as the alarm is active.
        if (alarmActive)
        {
          runAlarmLoop();
          break;
        }
      }
    }

    delay(10);
  }

  goToSleep();
}

void loop()
{
}