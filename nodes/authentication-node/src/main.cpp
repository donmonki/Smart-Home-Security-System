#include <Arduino.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LoRaP2P_Adapter.h>

// =====================================================
// RFID NODE
// =====================================================

#define NODE_ID 3

// RFID RC522 pins
#define RFID_SS 5
#define RFID_RST 4

MFRC522 rfid(RFID_SS, RFID_RST);

// RN2483A LoRa pins
#define LORA_RST 21
#define LORA_RX 32
#define LORA_TX 33

// LED feedback pins
#define GREEN_LED_PIN 25
#define RED_LED_PIN 26
#define LED_ON_DURATION_MS 3000

// Timing
#define HEARTBEAT_INTERVAL_MS 25000
#define RFID_COOLDOWN_MS 1000
#define AUTH_RESPONSE_TIMEOUT_MS 8000

HardwareSerial loraSerial(2);
LoraP2P lora(LORA_RST, LORA_RX, LORA_TX, loraSerial);

static uint32_t msgCounter = 0;
static uint32_t nextHeartbeatAt = 0;
static uint32_t rfidCooldownUntil = 0;
static uint32_t ledOffAt = 0;

// =====================================================
// LED HELPERS
// =====================================================

static void showAuthResult(bool success)
{
  digitalWrite(GREEN_LED_PIN, success ? HIGH : LOW);
  digitalWrite(RED_LED_PIN, success ? LOW : HIGH);
  ledOffAt = millis() + LED_ON_DURATION_MS;
}

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
    Serial.printf("[RFID Node %d] HEARTBEAT SENT #%lu\n",
                  NODE_ID,
                  (unsigned long)msgCounter);
  }
  else
  {
    Serial.println("[RFID Node] Heartbeat skipped — channel busy");
  }
}

// =====================================================
// RFID UID HELPERS
// =====================================================

static uint32_t getUidAsUint32()
{
  uint32_t uid = 0;

  for (byte i = 0; i < rfid.uid.size && i < 4; i++)
  {
    uid = (uid << 8) | rfid.uid.uidByte[i];
  }

  return uid;
}

static void printUid()
{
  Serial.print("RFID UID: ");

  for (byte i = 0; i < rfid.uid.size; i++)
  {
    if (rfid.uid.uidByte[i] < 0x10)
      Serial.print("0");

    Serial.print(rfid.uid.uidByte[i], HEX);

    if (i < rfid.uid.size - 1)
      Serial.print(":");
  }

  Serial.println();
}

// =====================================================
// RFID EVENT
// =====================================================

static void sendRFIDEvent(uint32_t uid)
{
  LoRaPayload tag;

  tag.nodeId = NODE_ID;
  tag.msgType = LORA_MSG_RFID_SCANNED;

  tag.data.sensorData.rfidUid = uid;
  tag.data.sensorData.messageCounter = ++msgCounter;

  if (lora.transmitWithAck(tag, 2000, 4))
  {
    Serial.printf("[RFID Node %d] RFID ACKED #%lu UID=0x%08lX\n",
                  NODE_ID,
                  (unsigned long)msgCounter,
                  (unsigned long)uid);

    // Wait for the server's authentication verdict
    uint32_t deadline = millis() + AUTH_RESPONSE_TIMEOUT_MS;
    bool gotResponse = false;

    while ((int32_t)(millis() - deadline) < 0)
    {
      LoRaPayload rx;
      if (lora.receivePayload(rx) &&
          rx.nodeId == NODE_ID &&
          rx.msgType == LORA_MSG_COMMAND)
      {
        bool granted = (rx.data.commandData.authenticationResult == AUTH_SUCCESS);
        showAuthResult(granted);
        Serial.printf("[RFID Node %d] AUTH %s\n",
                      NODE_ID, granted ? "GRANTED" : "DENIED");
        gotResponse = true;
        break;
      }
      delay(10);
    }

    if (!gotResponse)
    {
      Serial.printf("[RFID Node %d] Auth response timeout\n", NODE_ID);
    }
  }
  else
  {
    Serial.printf("[RFID Node %d] RFID LOST #%lu UID=0x%08lX\n",
                  NODE_ID,
                  (unsigned long)msgCounter,
                  (unsigned long)uid);
  }

  // Start cooldown only after LoRa send/ACK process finishes
  rfidCooldownUntil = millis() + RFID_COOLDOWN_MS;
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);
  randomSeed(esp_random());

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  SPI.begin();
  rfid.PCD_Init();

  Serial.println("Initializing RFID Node...");

  if (!lora.moduleInit())
  {
    Serial.println("[RFID Node] LoRa init FAILED");
    while (true)
    {
      delay(1000);
    }
  }

  Serial.printf("[RFID Node %d] READY\n", NODE_ID);

  scheduleNextHeartbeat();
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  // ---------------- RFID ----------------

  if ((int32_t)(millis() - rfidCooldownUntil) >= 0)
  {
    if (rfid.PICC_IsNewCardPresent() &&
        rfid.PICC_ReadCardSerial())
    {
      printUid();

      uint32_t uid = getUidAsUint32();

      rfid.PICC_HaltA();
      rfid.PCD_StopCrypto1();

      sendRFIDEvent(uid);
    }
  }

  // ---------------- HEARTBEAT ----------------

  if ((int32_t)(millis() - nextHeartbeatAt) >= 0)
  {
    sendHeartbeat();
    scheduleNextHeartbeat();
  }

  // ---------------- LED TIMER ----------------

  if (ledOffAt && (int32_t)(millis() - ledOffAt) >= 0)
  {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, LOW);
    ledOffAt = 0;
  }

  delay(10);
}