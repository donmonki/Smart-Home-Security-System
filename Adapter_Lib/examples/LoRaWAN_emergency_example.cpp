#include <Arduino.h>
#include <LoRaWAN_Adapter.h>
#include <HardwareSerial.h>

#define RST_PIN 23
#define RX_PIN 18
#define TX_PIN 19

// Replace with your actual TTN credentials
const char *DEV_EUI = "0004A30B0103EDFA";
const char *APP_EUI = "1234567890ABCDE0";
const char *APP_KEY = "0BB350EC15ED31F52F37E3892169818E";

HardwareSerial loraSerial(2);

LoRaWAN myLoRaWAN(RST_PIN, RX_PIN, TX_PIN, loraSerial, DEV_EUI, APP_EUI, APP_KEY);

// TODO: Replace with actual battery voltage reading
uint8_t getBatteryLevel()
{
    return 85;
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("LoRaWAN Emergency Alert Example");
    Serial.println("========================================");

    if (!myLoRaWAN.init())
    {
        Serial.println("ERROR: LoRaWAN initialization failed!");
        while (1)
            delay(1000);
    }

    Serial.println("\nAttempting to join The Things Network...");
    if (!myLoRaWAN.join())
    {
        Serial.println("ERROR: Failed to join network!");
        Serial.println("Check your credentials and TTN coverage");
        while (1)
            delay(1000);
    }

    Serial.println("\n✓ Successfully joined TTN!");
    Serial.println("Ready to send emergency alerts");
}

void loop()
{
    // TODO: In real implementation, trigger on actual power monitoring circuit

    Serial.println("\n========================================");
    Serial.println("SIMULATING POWER BLACKOUT!");
    Serial.println("========================================");

    uint8_t batteryLevel = getBatteryLevel();

    if (myLoRaWAN.sendBlackoutAlert(batteryLevel))
    {
        Serial.println("✓ Blackout alert sent successfully!");
        Serial.println("  - Message delivered to TTN");
        Serial.println("  - Telegram notification should arrive shortly");
    }
    else
    {
        Serial.println("✗ Failed to send blackout alert");
        Serial.println("  - Check network connectivity");
        Serial.println("  - Will retry on next attempt");
    }

    Serial.println("\nWaiting 60 seconds before next test..."); // LoRaWAN fair use policy
    delay(60000);
}
