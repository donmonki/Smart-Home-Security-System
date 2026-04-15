#include "MQTT_Adapter.h"

MqttAdapter::MqttAdapter(const char* ssid, const char* password, const char* mqttServer, uint16_t mqttPort, const char* clientId)
    : _ssid(ssid), _password(password), _mqttServer(mqttServer), _mqttPort(mqttPort), _clientId(clientId) {
    _mqttClient.setClient(_wifiClient);
}

void MqttAdapter::init(MQTT_CALLBACK_SIGNATURE) {
    _mqttClient.setServer(_mqttServer, _mqttPort);
    _mqttClient.setCallback(callback); // Set the function that handles incoming backend commands
    reconnect();
}

void MqttAdapter::alive_loop() {
    if (!_mqttClient.connected()) {
        reconnect();
    }
    _mqttClient.loop();
}

void MqttAdapter::reconnect() {
    // Loop until we're reconnected
    while (!_mqttClient.connected()) {
        Serial.print("Connecting to WiFi...");
        WiFi.begin(_ssid, _password);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.println(" Connected!");

        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (_mqttClient.connect(_clientId)) {
            Serial.println("connected");
            
            // Once connected, subscribe to the command topic from the backend
            // Using the '+' wildcard means we listen for commands to any node
            _mqttClient.subscribe("home/gateway/commands/node/+");
        } else {
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

bool MqttAdapter::publishNodeTelemetry(const LoRaPayload& payload) {
    // Create a JSON document
    StaticJsonDocument<200> doc;
    

    doc["nodeId"] = payload.nodeId;
    doc["msgType"] = payload.msgType;
    doc["status"] = "ONLINE";
    char jsonBuffer[200];
    serializeJson(doc, jsonBuffer);

    // Create a dynamic topic (e.g., "home/nodes/4/telemetry")
    char topicStr[50];
    snprintf(topicStr, sizeof(topicStr), "home/nodes/%d/telemetry", payload.nodeId);

    // Publish
    return _mqttClient.publish(topicStr, jsonBuffer);
}

bool MqttAdapter::publishGatewayTelemetry(uint8_t gatewayBatteryLevel) {
    StaticJsonDocument<200> doc;
    
    doc["gatewayId"] = _clientId; 
    doc["status"] = "ONLINE";
    doc["batteryLevel"] = gatewayBatteryLevel;

    char jsonBuffer[200];
    serializeJson(doc, jsonBuffer);

    // Publish to a gateway-specific topic
    return _mqttClient.publish("home/gateway/telemetry", jsonBuffer);
}

bool MqttAdapter::publishAuthentication(const LoRaPayload& payload) {
    StaticJsonDocument<200> doc;
    doc["nodeId"] = payload.nodeId;
    doc["msgType"] = payload.msgType;
    char rfidStr[10];
    snprintf(rfidStr, sizeof(rfidStr), "%08X", payload.data.sensorData.rfidUid);
    doc["rfidUid"] = rfidStr;

    char jsonBuffer[200];
    serializeJson(doc, jsonBuffer);

    char topicStr[50];
    snprintf(topicStr, sizeof(topicStr), "home/nodes/%d/authentication", payload.nodeId);

    
    return _mqttClient.publish(topicStr, jsonBuffer);
}


bool MqttAdapter::publishAlarm(const LoRaPayload& payload) {
    StaticJsonDocument<200> doc;
    doc["nodeId"] = payload.nodeId;
    doc["motionDetected"] = payload.data.sensorData.motionDetected;
    doc["alert"] = "MOTION_ALARM";

    char jsonBuffer[200];
    serializeJson(doc, jsonBuffer);

    char topicStr[50];
    snprintf(topicStr, sizeof(topicStr), "home/nodes/%d/alarm", payload.nodeId);

    
    return _mqttClient.publish(topicStr, jsonBuffer);
}


bool MqttAdapter::publishNodeOffline(uint8_t nodeId) {
    StaticJsonDocument<200> doc;
    doc["nodeId"] = nodeId;
    doc["status"] = "OFFLINE";
    doc["alert"] = "HEARTBEAT_TIMEOUT";

    char jsonBuffer[200];
    serializeJson(doc, jsonBuffer);

    char topicStr[50];
    snprintf(topicStr, sizeof(topicStr), "home/nodes/%d/status", nodeId);

    // Send the alert to the backend
    return _mqttClient.publish(topicStr, jsonBuffer);
}


// ===============================
// MQTT Downlink Methods
// ===============================


void MqttAdapter::processIncomingMessage(char* topic, byte* payload, unsigned int length) {
    char jsonStr[length + 1];
    memcpy(jsonStr, payload, length);
    jsonStr[length] = '\0';

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (error) {
        Serial.println("Error: Failed to parse backend command.");
        return;
    }

    // Extract the target Node ID from the topic
    String topicStr(topic);
    int lastSlash = topicStr.lastIndexOf('/');
    uint8_t targetNode = topicStr.substring(lastSlash + 1).toInt();

    // Populate our single pending command variable
    _pendingCommand.nodeId = targetNode; 
    _pendingCommand.msgType = MSG_COMMAND; 
    _pendingCommand.data.commandData.actionId = doc["actionId"];
    _pendingCommand.data.commandData.parameter = doc["parameter"];

    _hasPendingCommand = true; // Set pending command flag 

    Serial.printf("Saved command for Node %d\n", targetNode);
}

bool MqttAdapter::getPendingCommand(uint8_t requestingNodeId, LoRaPayload &outPayload) {
    // Check if there's a pending command for this node
    if (_hasPendingCommand && _pendingCommand.nodeId == requestingNodeId) {
        
        // Copy out pending command 
        outPayload = _pendingCommand;
        
        // Clear the pending command flag
        _hasPendingCommand = false; 
        
        return true;
    }
    return false;
}


//bool MqttAdapter::publishJoinRequest(const LoRaPayload& payload) {
//     StaticJsonDocument<200> doc;
    
//     // Convert the 6-byte MAC array to a string like "A1:B2:C3:D4:E5:F6"
//     char macStr[18];
//     snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", 
//              payload.joinData.macAddress[0], payload.joinData.macAddress[1], 
//              payload.joinData.macAddress[2], payload.joinData.macAddress[3], 
//              payload.joinData.macAddress[4], payload.joinData.macAddress[5]);

//     doc["macAddress"] = macStr;
//     doc["status"] = "requesting_id";

//     char jsonBuffer[200];
//     serializeJson(doc, jsonBuffer);

//     return _mqttClient.publish("home/gateway/join_requests", jsonBuffer);
// }