#include "MQTT_Adapter.h"

MqttAdapter::MqttAdapter(const char *ssid, const char *password, const char *mqttServer, uint16_t mqttPort, const char *clientId)
    : _ssid(ssid), _password(password), _mqttServer(mqttServer), _mqttPort(mqttPort), _clientId(clientId)
{
    _mqttClient.setClient(_wifiClient);
}

void MqttAdapter::init(MQTT_CALLBACK_SIGNATURE)
{
    // Configure TLS/SSL encryption
    Serial.println("[MQTT] Configuring TLS/SSL encryption...");
    Serial.println("[MQTT] Loading Root CA Certificate from PROGMEM");

    /*
    * IMPORTANT NOTE: The Certificate handling has a very strict format requirement. Please refer to the documentation for debugging TLS issues.
    * If you encounter any certificate validation error or not able to maintain a stable connection comment out the setCACert line and uncomment setInsecure to bypass certificate.
    */
   // Set CA certificate using setCACert method
    _wifiClient.setCACert(ROOT_CA_CERT);
    // _wifiClient.setInsecure(); 
    
    _mqttClient.setServer(_mqttServer, _mqttPort);
    _mqttClient.setCallback(callback);
    Serial.printf("[MQTT] Server configured: %s:%d\n", _mqttServer, _mqttPort);
    reconnect();
}

void MqttAdapter::alive_loop()
{
    if (!_mqttClient.connected())
    {
        reconnect();
    }
    _mqttClient.loop();
}

void MqttAdapter::reconnect()
{
    while (!_mqttClient.connected())
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.print("[WiFi] Connecting to WiFi...");
            WiFi.begin(_ssid, _password);
            while (WiFi.status() != WL_CONNECTED)
            {
                delay(500);
                Serial.print(".");
            }
            Serial.println(" [WiFi] Connected!");
            Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());

            // Get the time from NTP server after the WiFi is connected
            // 7200 = UTC+2 for CEST
            configTime(7200, 0, "pool.ntp.org");

            Serial.print("[NTP] Waiting for NTP time sync...");
            time_t now = time(nullptr);
            // Wait until the time is strictly greater than Jan 1, 2026 to ensure we have a valid timestamp
            int ntpTimeout = 0;

            // Wait until time is strictly greater than Jan 1, 2026, OR timeout after 15 seconds
            while (now < 1767225600 && ntpTimeout < 30)
            {
                delay(500);
                Serial.print(".");
                now = time(nullptr);
                ntpTimeout++;
            }

            if (now < 1767225600)
            {
                Serial.println(" [NTP] NTP Timeout! TLS handshake may fail.");
            }
            else
            {
                Serial.println(" [NTP] Time Synced!");
                struct tm timeinfo;
                if (getLocalTime(&timeinfo))
                {
                    Serial.print("[NTP] ESP32 Current Time: ");
                    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
                }
            }
        }

        Serial.printf("[MQTT] Attempting TLS connection to %s:%d...", _mqttServer, _mqttPort);

        // Attempt to connect with TLS
        if (_mqttClient.connect(_clientId))
        {
            Serial.println("[MQTT] Connected!");
            Serial.println("[MQTT] TLS/SSL handshake successful - Connection is encrypted");

            // Once connected, subscribe to the command topic from the backend wildcard
            // is used to receive commands for all nodes.
            _mqttClient.subscribe("home/gateway/commands/node/+");
            Serial.println("[MQTT] Subscribed to: home/gateway/commands/node/+");
        }
        else
        {
            char buf[100];
            _wifiClient.lastError(buf, sizeof(buf));
            Serial.printf("[SSL ERROR] %s\n", buf);
            Serial.printf("[SSL] lastError code details above\n");
            Serial.print(" [MQTT] Failed, rc=");
            Serial.print(_mqttClient.state());
            Serial.println(" (5=connection lost, 4=connection refused, 2=connect failed)");
            Serial.println("[MQTT] Possible causes:");
            Serial.println("  - Check MQTT port (8883 for TLS)");
            Serial.println("  - Verify Root CA certificate in docker if it is using the flashed one");
            Serial.println("  - Restart docker if new certificate was generated andflashed");
            Serial.println("[MQTT] Retrying in 5 seconds...");
            delay(5000);
        }
    }
}

// ===============================
// MQTT Uplink Methods
// ===============================

bool MqttAdapter::publishEvent(const LoRaPayload &payload)
{
    JsonDocument doc;

    doc["nodeId"] = payload.nodeId;

    // Get the Epoch time
    time_t now;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Nested JSON object for timestamp details
    JsonObject ts = doc["timestamp"].to<JsonObject>();

    // Populate the dictionary
    ts["raw_timestamp"] = now;
    ts["year"] = timeinfo.tm_year + 1900; // tm_year is years since 1900
    ts["month"] = timeinfo.tm_mon + 1;    // tm_mon is 0-11
    ts["day"] = timeinfo.tm_mday;
    ts["hour"] = timeinfo.tm_hour;
    ts["minute"] = timeinfo.tm_min;
    ts["second"] = timeinfo.tm_sec;
    doc["deviceStatus"] = MQTT_DEVICE_ONLINE;

    // Topic and payload structure will depend on the message type
    char topicStr[100];

    switch (payload.msgType)
    {

    case LORA_MSG_HEARTBEAT:
        doc["type"] = MQTT_MSG_HEARTBEAT;
        snprintf(topicStr, sizeof(topicStr), "home/nodes/%d/status", payload.nodeId);
        break;

    case LORA_MSG_MOTION_ALARM:
        doc["type"] = MQTT_MSG_MOTIONALARM;
        doc["dataType"] = "boolean";
        doc["value"] = payload.data.sensorData.motionDetected ? 1 : 0;
        snprintf(topicStr, sizeof(topicStr), "home/nodes/%d/alarm", payload.nodeId);
        break;

    case LORA_MSG_RFID_SCANNED:
        doc["type"] = MQTT_MSG_AUTHENTICATION;
        doc["dataType"] = "hex_string";
        char rfidStr[10];
        snprintf(rfidStr, sizeof(rfidStr), "%08X", payload.data.sensorData.rfidUid);
        doc["value"] = rfidStr;
        snprintf(topicStr, sizeof(topicStr), "home/nodes/%d/authentication", payload.nodeId);
        break;

    default:
        doc["type"] = MQTT_MSG_UNKNOWN;
        doc["dataType"] = "null";
        doc["value"] = 0;
        snprintf(topicStr, sizeof(topicStr), "home/nodes/%d/unknown", payload.nodeId);
        break;
    }

    char jsonBuffer[300];
    serializeJson(doc, jsonBuffer);

    // Publish the universal JSON payload to the specific topic
    return _mqttClient.publish(topicStr, jsonBuffer);
}

bool MqttAdapter::publishNodeOffline(uint8_t nodeId)
{
    JsonDocument doc;

    doc["nodeId"] = nodeId;
    doc["type"] = MQTT_MSG_HEARTBEAT;
    // Get the Epoch time
    time_t now;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Nested JSON object for timestamp details
    JsonObject ts = doc["timestamp"].to<JsonObject>();

    // 5. Populate the time dictionary
    ts["raw_timestamp"] = now;
    ts["year"] = timeinfo.tm_year + 1900;
    ts["month"] = timeinfo.tm_mon + 1;
    ts["day"] = timeinfo.tm_mday;
    ts["hour"] = timeinfo.tm_hour;
    ts["minute"] = timeinfo.tm_min;
    ts["second"] = timeinfo.tm_sec;
    doc["deviceStatus"] = MQTT_DEVICE_OFFLINE;

    char jsonBuffer[300];
    serializeJson(doc, jsonBuffer);

    // Build the node specific status topic
    char topicStr[100];
    snprintf(topicStr, sizeof(topicStr), "home/nodes/%d/status", nodeId);

    // Send the alert to the backend
    return _mqttClient.publish(topicStr, jsonBuffer);
}

bool MqttAdapter::publishGatewayTelemetry(uint8_t gatewayBatteryLevel)
{
    JsonDocument doc;

    doc["gatewayId"] = _clientId;
    doc["type"] = MQTT_MSG_GATEWAY_TELEMETRY;
    doc["dataType"] = "int";
    doc["value"] = gatewayBatteryLevel;
    doc["deviceStatus"] = MQTT_DEVICE_ONLINE;

    // Get the Epoch time
    time_t now;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Create a nested JSON object for timestamp details
    JsonObject ts = doc["timestamp"].to<JsonObject>();

    ts["raw_timestamp"] = now;
    ts["year"] = timeinfo.tm_year + 1900;
    ts["month"] = timeinfo.tm_mon + 1;
    ts["day"] = timeinfo.tm_mday;
    ts["hour"] = timeinfo.tm_hour;
    ts["minute"] = timeinfo.tm_min;
    ts["second"] = timeinfo.tm_sec;

    char jsonBuffer[300];
    serializeJson(doc, jsonBuffer);

    // Publish to a gateway-specific topic
    return _mqttClient.publish("home/gateway/telemetry", jsonBuffer);
}

// ===============================
// MQTT Downlink Methods
// ===============================

void MqttAdapter::processIncomingMessage(char *topic, byte *payload, unsigned int length, LoRaPayload &outPayload)
{
    char jsonStr[length + 1];
    memcpy(jsonStr, payload, length);
    jsonStr[length] = '\0';

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (error)
    {
        Serial.println("[MQTT] Error: Failed to parse backend command.");
        return;
    }

    // Extract the target Node ID from the topic
    String topicStr(topic);
    int lastSlash = topicStr.lastIndexOf('/');
    uint8_t targetNode = topicStr.substring(lastSlash + 1).toInt();

    // Populate the outPayload struct based on the received JSON
    outPayload.nodeId = targetNode;
    outPayload.msgType = LORA_MSG_COMMAND;
    outPayload.data.commandData.actionId = doc["actionId"];
    outPayload.data.commandData.parameter = doc["parameter"];

    Serial.printf("[MQTT] Received command for Node %d\n", targetNode);
}
