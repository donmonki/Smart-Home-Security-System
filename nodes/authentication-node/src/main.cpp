#include <Arduino.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LoRaP2P_Adapter.h>
#include "esp_sleep.h"

#define NODE_ID 3

#define RFID_SS 5
#define RFID_RST 4

#define RFID_IRQ 15
MFRC522 rfid(RFID_SS, RFID_RST);

#define LORA_RST 21
#define LORA_RX 32
#define LORA_TX 33

#define GREEN_LED_PIN 25
#define RED_LED_PIN 26

#define HEARTBEAT_INTERVAL_MS 25000UL

// How often to re-arm the MFRC522 REQA probe while no card is present.
// REQA is a one-shot command; the timer wakeup retriggers it so the reader
// stays active while the ESP32 is in deep sleep.
#define INTERRUPT_REARM_INTERVAL_US 500000ULL // 500 ms
#define INTERRUPT_REARM_MS (INTERRUPT_REARM_INTERVAL_US / 1000ULL)

HardwareSerial loraSerial(2);
LoraP2P lora(LORA_RST, LORA_RX, LORA_TX, loraSerial);

RTC_DATA_ATTR uint32_t msgCounter = 0;
RTC_DATA_ATTR uint32_t msUntilHeartbeat = 0; // 0 on first boot → send immediately

static void sendHeartbeat()
{
  LoRaPayload hb;
  hb.nodeId = NODE_ID;
  hb.msgType = LORA_MSG_HEARTBEAT;
  hb.data.sensorData.messageCounter = ++msgCounter;

  if (lora.transmitWithLBT(hb))
    Serial.printf("[RFID Node %d] HEARTBEAT SENT #%lu\n", NODE_ID, (unsigned long)msgCounter);
  else
    Serial.println("[RFID Node] Heartbeat skipped — channel busy");

  msUntilHeartbeat = HEARTBEAT_INTERVAL_MS;
}

static uint32_t getUidAsUint32()
{
  uint32_t uid = 0;

  for (byte i = 0; i < rfid.uid.size && i < 4; i++)
    uid = (uid << 8) | rfid.uid.uidByte[i];

  return uid;
}

static void showAuthResult(bool success)
{
  digitalWrite(GREEN_LED_PIN, success ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, success ? LOW : HIGH);
  delay(3000);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
}

static void sendRFIDEvent(uint32_t uid)
{
  LoRaPayload tag;
  tag.nodeId = NODE_ID;
  tag.msgType = LORA_MSG_RFID_SCANNED;
  tag.data.sensorData.rfidUid = uid;
  tag.data.sensorData.messageCounter = ++msgCounter;

  if (lora.transmitWithAck(tag, 2000, 4))
  {
    Serial.printf("[RFID Node %d] RFID ACKED UID=0x%08lX\n", NODE_ID, (unsigned long)uid);
    showAuthResult(true);
  }
  else
  {
    Serial.printf("[RFID Node %d] RFID LOST UID=0x%08lX\n", NODE_ID, (unsigned long)uid);
    showAuthResult(false);
  }
}

/* Arms the MFRC522 to transmit a single REQA probe and assert its IRQ pin
(GPIO15) when a card responds.

REQA is a one-shot ISO 14443 command: the module sends it once, listens
briefly, then goes idle. The timer wakeup re-calls this every 500 ms so
the reader keeps probing while the ESP32 is in deep sleep. */

static void armRfidInterrupt()
{
  // Enable Rx interrupt; IRqInv=1 → IRQ pin goes HIGH on card response.
  rfid.PCD_WriteRegister(MFRC522::ComIEnReg, 0xA0);
  // Clear all pending interrupt bits (write 0 to the Set1 bit).
  rfid.PCD_ClearRegisterBitMask(MFRC522::ComIrqReg, 0x80);
  // Queue the REQA command byte (7-bit short frame).
  rfid.PCD_WriteRegister(MFRC522::FIFODataReg, MFRC522::PICC_CMD_REQA);
  // Execute the Transceive command (send FIFO, then receive).
  rfid.PCD_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_Transceive);
  // StartSend=1, TxLastBits=7 (7 valid bits in the last byte of REQA).
  rfid.PCD_SetRegisterBitMask(MFRC522::BitFramingReg, 0x87);
}

static void goToSleep()
{
  // Wake on MFRC522 IRQ going HIGH (card detected).
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 1);
  // Fallback timer to re-arm the REQA probe if no card responded.
  esp_sleep_enable_timer_wakeup(INTERRUPT_REARM_INTERVAL_US);

  Serial.flush();
  esp_deep_sleep_start();
}

void setup()
{
  esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();

  // Always reinitialise SPI and the MFRC522 — deep sleep powers down the
  // SPI peripheral, so both must be brought up on every wakeup.
  SPI.begin();
  rfid.PCD_Init();

  if (wakeReason == ESP_SLEEP_WAKEUP_TIMER)
  {
    // Decrement the heartbeat countdown by the time spent sleeping.
    if (msUntilHeartbeat <= (uint32_t)INTERRUPT_REARM_MS)
      msUntilHeartbeat = 0;
    else
      msUntilHeartbeat -= (uint32_t)INTERRUPT_REARM_MS;

    if (msUntilHeartbeat > 0)
    {
      // Fast re-arm: no card detected, no heartbeat due.
      // Skip LoRa and Serial to keep this path as short as possible (~50 ms).
      armRfidInterrupt();
      goToSleep();
      return;
    }

    // Heartbeat is due — fall through to the full init path below.
  }

  // Full init path: first boot, or EXT0 wakeup (card detected).
  Serial.begin(115200);
  delay(500);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  Serial.println("Initializing RFID Node...");

  if (!lora.moduleInit())
  {
    Serial.println("[RFID Node] LoRa init FAILED");
    while (true)
      delay(1000);
  }

  Serial.printf("[RFID Node %d] READY\n", NODE_ID);

  if (wakeReason == ESP_SLEEP_WAKEUP_EXT0)
  {
    // Card detected — read it.
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial())
    {
      Serial.print("[RFID Node] UID: ");
      for (byte i = 0; i < rfid.uid.size; i++)
      {
        if (rfid.uid.uidByte[i] < 0x10)
          Serial.print("0");
        Serial.print(rfid.uid.uidByte[i], HEX);
        if (i < rfid.uid.size - 1)
          Serial.print(":");
      }
      Serial.println();

      uint32_t uid = getUidAsUint32();
      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();

      sendRFIDEvent(uid);
    }
    else
    {
      Serial.println("[RFID Node] IRQ fired but card read failed (card removed too quickly)");
    }
  }
  else
  {
    // First boot (msUntilHeartbeat == 0) or heartbeat-due timer wakeup.
    sendHeartbeat();
  }

  armRfidInterrupt();
  Serial.println("[RFID Node] Armed — waiting for card (deep sleep)");
  goToSleep();
}

void loop() {}