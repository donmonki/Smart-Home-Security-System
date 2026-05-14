#include <Arduino.h>
#include <HardwareSerial.h>
#include <LoRaP2P_Adapter.h>

#define NODE_ID 1

// PIR sensor
#define PIR_PIN 27

// RN2483A LoRa pins
#define LORA_RST 21
#define LORA_RX 32
#define LORA_TX 33

#define HEARTBEAT_INTERVAL_MS 25000

HardwareSerial loraSerial(2);
LoraP2P lora(LORA_RST, LORA_RX, LORA_TX, loraSerial);

static uint32_t msgCounter = 0;
static uint32_t nextHeartbeatAt = 0;
static bool lastMotionState = false;

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
    Serial.printf("[Motion Node %d] HEARTBEAT SENT #%lu\n",
                  NODE_ID,
                  (unsigned long)msgCounter);
  }
  else
  {
    Serial.println("[Motion Node] Heartbeat skipped — channel busy");
  }
}

static void sendMotionAlarm()
{
  LoRaPayload alarm;

  alarm.nodeId = NODE_ID;
  alarm.msgType = LORA_MSG_MOTION_ALARM;

  alarm.data.sensorData.motionDetected = true;
  alarm.data.sensorData.messageCounter = ++msgCounter;

  if (lora.transmitWithAck(alarm, 600, 4))
  {
    Serial.printf("[Motion Node %d] MOTION ACKED #%lu\n",
                  NODE_ID,
                  (unsigned long)msgCounter);
  }
  else
  {
    Serial.printf("[Motion Node %d] MOTION LOST #%lu\n",
                  NODE_ID,
                  (unsigned long)msgCounter);
  }
}

void setup()
{
  Serial.begin(115200);
  randomSeed(esp_random());

  pinMode(PIR_PIN, INPUT);

  Serial.println("Initializing Motion Node...");

  if (!lora.moduleInit())
  {
    Serial.println("[Motion Node] LoRa init FAILED");
    while (true)
    {
      delay(1000);
    }
  }

  Serial.printf("[Motion Node %d] READY\n", NODE_ID);

  scheduleNextHeartbeat();
}

void loop()
{
  bool motionNow = digitalRead(PIR_PIN);

  if (motionNow && !lastMotionState)
  {
    Serial.println("Motion detected!");
    sendMotionAlarm();
  }

  lastMotionState = motionNow;

  if ((int32_t)(millis() - nextHeartbeatAt) >= 0)
  {
    sendHeartbeat();
    scheduleNextHeartbeat();
  }

  delay(10);
}