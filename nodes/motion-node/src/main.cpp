#include <Arduino.h>
#include <HardwareSerial.h>
#include <LoRaP2P_Adapter.h>
#include "esp_sleep.h"

#define NODE_ID 1
#define PIR_PIN 27

#define LORA_RST 21
#define LORA_RX 32
#define LORA_TX 33

#define HEARTBEAT_INTERVAL_MS 25000
#define SLEEP_TIME_US 25000000ULL

HardwareSerial loraSerial(2);
LoraP2P lora(LORA_RST, LORA_RX, LORA_TX, loraSerial);

RTC_DATA_ATTR uint32_t msgCounter = 0;

static void sendHeartbeat()
{
  LoRaPayload hb;
  hb.nodeId = NODE_ID;
  hb.msgType = LORA_MSG_HEARTBEAT;
  hb.data.sensorData.messageCounter = ++msgCounter;

  lora.transmitWithLBT(hb);
  Serial.printf("[Motion Node %d] HEARTBEAT SENT #%lu\n", NODE_ID, (unsigned long)msgCounter);
}

static void sendMotionAlarm()
{
  LoRaPayload alarm;
  alarm.nodeId = NODE_ID;
  alarm.msgType = LORA_MSG_MOTION_ALARM;
  alarm.data.sensorData.motionDetected = true;
  alarm.data.sensorData.messageCounter = ++msgCounter;

  if (lora.transmitWithAck(alarm, 600, 4))
    Serial.printf("[Motion Node %d] MOTION ACKED #%lu\n", NODE_ID, (unsigned long)msgCounter);
  else
    Serial.printf("[Motion Node %d] MOTION LOST #%lu\n", NODE_ID, (unsigned long)msgCounter);
}

static void goToSleep()
{
  Serial.println("[Motion Node] Going to deep sleep...");
  delay(200);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, 1);
  esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);
  esp_deep_sleep_start();
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  pinMode(PIR_PIN, INPUT);

  Serial.println("Initializing Motion Node...");

  if (!lora.moduleInit())
  {
    Serial.println("[Motion Node] LoRa init FAILED");
    while (true)
      delay(1000);
  }

  esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();

  if (wakeReason == ESP_SLEEP_WAKEUP_EXT0)
  {
    Serial.println("Motion detected!");
    sendMotionAlarm();
  }
  else
  {
    sendHeartbeat();
  }

  goToSleep();
}

void loop() {}