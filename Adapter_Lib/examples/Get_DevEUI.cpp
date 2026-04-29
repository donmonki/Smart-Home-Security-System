#include <Arduino.h>
#include <HardwareSerial.h>

// Same pins as your main setup
#define RST_PIN 23
#define RX_PIN 18
#define TX_PIN 19

HardwareSerial loraSerial(2);

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("LoRaWAN DevEUI Reader");
    Serial.println("========================================");

    // Initialize LoRa serial
    loraSerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
    delay(100);

    // Hardware reset
    pinMode(RST_PIN, OUTPUT);
    digitalWrite(RST_PIN, LOW);
    delay(100);
    digitalWrite(RST_PIN, HIGH);
    delay(1000);

    // Clear any pending data
    while (loraSerial.available())
    {
        loraSerial.read();
    }

    Serial.println("\nSending reset command...");
    loraSerial.println("sys reset");
    delay(1000);

    // Read response
    while (loraSerial.available())
    {
        Serial.write(loraSerial.read());
    }

    Serial.println("\n\nRequesting DevEUI...");

    // Try RN2483/RN2903 command
    loraSerial.println("sys get hweui");
    delay(500);

    String devEUI = "";
    while (loraSerial.available())
    {
        char c = loraSerial.read();
        if (c != '\r' && c != '\n')
        {
            devEUI += c;
        }
    }

    if (devEUI.length() == 16)
    {
        Serial.println("✓ DevEUI found!");
        Serial.println("========================================");
        Serial.print("DevEUI: ");
        Serial.println(devEUI);
        Serial.println("========================================");
        Serial.println("\nCopy this DevEUI to register your device in TTN!");
    }
    else
    {
        Serial.println("✗ Could not read DevEUI");
        Serial.println("Response: " + devEUI);
        Serial.println("\nTrying alternative command (RAK modules)...");

        loraSerial.println("AT+DEVEUI=?");
        delay(500);

        devEUI = "";
        while (loraSerial.available())
        {
            char c = loraSerial.read();
            Serial.write(c);
            if (c != '\r' && c != '\n' && c != '+' && c != '=' && c != ':')
            {
                devEUI += c;
            }
        }
    }

    Serial.println("\n\nIf DevEUI not displayed:");
    Serial.println("1. Check wiring (RST=23, RX=18, TX=19)");
    Serial.println("2. Verify module power");
    Serial.println("3. Try different baud rate");
    Serial.println("4. Check module datasheet for correct AT command");
}

void loop()
{
    // Echo any serial communication for debugging
    if (loraSerial.available())
    {
        Serial.write(loraSerial.read());
    }
    if (Serial.available())
    {
        loraSerial.write(Serial.read());
    }
}
