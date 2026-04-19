#include "MQTT_Adapter.h"

MqttAdapter::MqttAdapter(const char *ssid, const char *password, const char *mqttServer, uint16_t mqttPort, const char *clientId)
    : _ssid(ssid), _password(password), _mqttServer(mqttServer), _mqttPort(mqttPort), _clientId(clientId)
{
    _mqttClient.setClient(_wifiClient);
}

void MqttAdapter::init(MQTT_CALLBACK_SIGNATURE)
{
    _mqttClient.setServer(_mqttServer, _mqttPort);
    _mqttClient.setCallback(callback); // Set the function that handles incoming backend commands
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
            Serial.print("Connecting to WiFi...");
            WiFi.begin(_ssid, _password);
            while (WiFi.status() != WL_CONNECTED)
            {
                delay(500);
                Serial.print(".");
            }
            Serial.println(" Connected!");
            // Get the time from NTP server after the WiFi is connected
            // 7200 = UTC+2 for CEST
            configTime(7200, 0, "pool.ntp.org");

            Serial.print("Waiting for NTP time sync...");
            time_t now = time(nullptr);
            // Wait until the time is strictly greater than Jan 1, 2026 to ensure we have a valid timestamp
            while (now < 1767225600)
            {
                delay(500);
                Serial.print(".");
                now = time(nullptr);
            }
            Serial.println(" Time Synced!");
        }
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (_mqttClient.connect(_clientId))
        {
            Serial.println("connected");
            // Once connected, subscribe to the command topic from the backend wildcard
            // is used to receive commands for all nodes.
            _mqttClient.subscribe("home/gateway/commands/node/+");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(_mqttClient.state());
            Serial.println(" try again in 5 seconds");
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
        Serial.println("Error: Failed to parse backend command.");
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

    Serial.printf("Received command for Node %d\n", targetNode);
}

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

    // We will build the target topic dynamically based on the event type
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
