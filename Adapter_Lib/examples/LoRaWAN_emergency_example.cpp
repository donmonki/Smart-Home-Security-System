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

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("LoRaWAN Repeating Message Example");
    Serial.println("==================================");

    // Wake and join network
    myLoRaWAN.wakeup();

    if (!myLoRaWAN.init())
    {
        Serial.println("Init failed!");
        while (1)
            delay(1000);
    }

    if (!myLoRaWAN.join())
    {
        Serial.println("Join failed!");
        while (1)
            delay(1000);
    }

    Serial.println("Joined network successfully\n");
}

// all of the lorawan operations take time to complete or fail
// take it into account in prod
void loop()
{
    // Send message
    Serial.println("Sending message...");
    uint8_t batteryLevel = 85;
    myLoRaWAN.sendBlackoutAlert(batteryLevel);

    // Shutdown to save power
    Serial.println("Shutting down module\n");
    myLoRaWAN.shutdown();

    // Wait 2 minutes - at least 1h in production
    delay(120000);

    // Wake and rejoin for next message
    Serial.println("Waking up...");
    myLoRaWAN.wakeup();
    myLoRaWAN.init(); // check if succesful in prod
    myLoRaWAN.join(); // check if succesful in prod
}