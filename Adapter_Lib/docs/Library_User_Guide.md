# Library User Guide

### _General Description_:

The library is created to use for the Node- and Gateway development to send and receive data.\
Supported Communication protocols:

- LoRa P2P
- MQTT
- LoRaWAN

## **_LoRaP2P Adapter User Guide_**

The LoRaP2P Adapter provides direct peer-to-peer LoRa communication between nodes and the gateway. It handles low-level LoRa module initialization, transmission, and reception of encrypted payloads using AES-128 encryption.

#### **Constructor**

```cpp
LoraP2P(uint8_t rst_pin, uint8_t rx_pin, uint8_t tx_pin, HardwareSerial &serial)
```

Creates a LoRaP2P adapter instance with the following parameters:

- `rst_pin`: GPIO pin connected to the LoRa module reset line
- `rx_pin`: GPIO pin for UART RX (connected to LoRa TX)
- `tx_pin`: GPIO pin for UART TX (connected to LoRa RX)
- `serial`: Reference to the HardwareSerial port

#### **Initialization**

- `bool moduleInit()`: Initializes the LoRa module with optimal configuration parameters.
  - Performs hardware reset and serial communication setup (57600 baud)
  - Configures LoRa parameters: Frequency 866.1 MHz (EU), SF7, BW 125 kHz, 14 dBm power
  - Enables AES-128 encryption/decryption for all payloads
  - Must be called in `setup()` before any transmission or reception
  - `return`: `true` if initialization successful, `false` if any configuration command fails

#### **LoRaPayload Data Structure**

The standard payload structure (16 bytes total) for all LoRa P2P communication:

```cpp
struct LoRaPayload {
    uint8_t nodeId;      // ID of the sending/receiving node
    uint8_t msgType;     // Message type identifier

    union {
        // For sensor data (Heartbeat, Motion Alarm, RFID)
        struct {
            bool motionDetected;     // 1 byte
            uint32_t rfidUid;        // 4 bytes
            uint32_t messageCounter; // 4 bytes
            uint8_t padding[5];      // Padding to reach 14 bytes
        } sensorData;

        // For gateway commands
        struct {
            uint8_t actionId;        // Command type
            uint8_t parameter;       // Command parameter (e.g., duration)
            uint8_t padding[12];     // Padding to reach 14 bytes
        } commandData;
    } data;
};
```

#### **Message Types**

- `LORA_MSG_HEARTBEAT` (0x01): Node heartbeat/status update sent by all nodes
- `LORA_MSG_MOTION_ALARM` (0x02): Motion detection event from motion sensor node
- `LORA_MSG_RFID_SCANNED` (0x03): RFID card scanned event from RFID node
- `LORA_MSG_COMMAND` (0x04): Command from gateway to node (downlink)

#### **Action Types (Gateway Commands)**

- `CMD_ALARM_ON` (0x01): Activate alarm on target node
- `CMD_ALARM_OFF` (0x02): Deactivate alarm on target node

#### **Transmission Method**

- `bool transmitPayload(const LoRaPayload &payload)`: Transmits an encrypted LoRa payload.
  - Automatically encrypts the payload using AES-128
  - Handles all serial communication with the LoRa module
  - `return`: `true` if transmission successful, `false` otherwise

#### **Reception Method**

- `bool receivePayload(LoRaPayload &receivedData)`: Receives and decrypts a LoRa payload.
  - Listens for incoming LoRa packets
  - Automatically decrypts received data using AES-128
  - Populates the provided `receivedData` struct with received information
  - Must be called regularly in `loop()` to check for incoming messages
  - `return`: `true` if valid payload received and decrypted, `false` otherwise

#### **Encryption and Utility Functions**

- `void encryptPayload(const LoRaPayload &inputData, unsigned char *outputBuffer16)`: Encrypts a payload using AES-128-ECB.
- `bool decryptPayload(const unsigned char *inputBuffer16, LoRaPayload &outputData)`: Decrypts a 16-byte buffer to a payload.
- `String bytesToHex(const unsigned char *data, int length)`: Converts byte array to hex string.
- `void hexToBytes(const String &hex, unsigned char *bytes)`: Converts hex string to byte array.

**Security Note**: The AES-128 secret key is shared across all devices and is defined in `LoRaP2P_Adapter.cpp`. All devices must use the same key for encryption/decryption to work properly.

#### **LoRa Module Configuration**

The module is configured with the following parameters during initialization:

- **Frequency**: 866.1 MHz (EU ISM band)
- **Spreading Factor**: SF7 (balance between range and data rate)
- **Bandwidth**: 125 kHz
- **Transmission Power**: 14 dBm (maximum for EU)
- **Coding Rate**: 4/5
- **CRC**: Enabled
- **Preamble Length**: 8
- **Watchdog Timer**: 60 seconds

#### **Usage Example**

For complete working examples, refer to the example files in the `Adapter_Lib/examples/` folder:

- `Lora_Rx_basic_example.cpp`: Basic receive example without encryption
- `Lora_Rx_decryption_example.cpp`: Receive example with decryption
- `Lora_Tx_basic_example.cpp`: Basic transmit example without encryption
- `Lora_Tx_encryption_example.cpp`: Transmit example with encryption

#### **Tips for Payload Construction**

- Always initialize the payload struct to ensure padding bytes are cleared
- Use appropriate message type for your data
- Message counter should be incremented for each message sent by a node
- Ensure the same AES key is configured on all devices
- Add a small delay time for transmission/reception (typically 1-2 seconds per message)

## **_MQTT Adapter User Guide_**

The MQTT Adapter is going to be used by the gateway to maintain the connection with the backend server by forwarding all the incoming LoRa messages from the nodes.

#### **Constructor**

```cpp
MqttAdapter(const char *ssid, const char *password, const char *mqttServer, uint16_t mqttPort, const char *clientId)
```

Creates an MQTT adapter instance with the following parameters:

- `ssid`: WiFi network SSID
- `password`: WiFi network password
- `mqttServer`: IP address or hostname of the MQTT broker
- `mqttPort`: Port number of the MQTT broker (typically 1883)
- `clientId`: Unique identifier for this MQTT client

#### **Initialization and Connection Management**

- `void init(MQTT_CALLBACK_SIGNATURE)`: Initializes the MQTT adapter and establishes connection to the broker. Must be called in `setup()`.
  - Parameter: MQTT callback function to handle incoming messages from the broker
- `void alive_loop()`: Maintains the MQTT connection and processes incoming messages. Must be called regularly in `loop()`.
  - Automatically attempts to reconnect if the connection is lost
  - Synchronizes time with NTP server upon WiFi connection
  - Subscribes to `home/gateway/commands/node/+` to receive commands for all nodes

#### **Publishing Methods (Uplink - Gateway to the Backend Server)**

- `bool publishEvent(const LoRaPayload &payload)`: Publishes node events based on message type.
  - Supports HEARTBEAT, MOTION_ALARM, and RFID_SCANNED events
  - Topics: `home/nodes/{nodeId}/status`, `home/nodes/{nodeId}/alarm`, `home/nodes/{nodeId}/authentication`
  - Includes timestamp and device status information
  - `return`: `true` if publish successful, `false` otherwise

- `bool publishNodeOffline(uint8_t nodeId)`: Publishes node offline status.
  - Topic: `home/nodes/{nodeId}/status`
  - Marks device status as OFFLINE
  - `return`: `true` if publish successful, `false` otherwise

- `bool publishGatewayTelemetry(uint8_t gatewayBatteryLevel)`: Publishes gateway telemetry data.
  - Topic: `home/gateway/telemetry`
  - Includes gateway ID, battery level, and timestamp
  - `return`: `true` if publish successful, `false` otherwise

#### **Receiving Methods (Downlink - Backend Server to the Gateway )**

- `void processIncomingMessage(char *topic, byte *payload, unsigned int length, LoRaPayload &outPayload)`: Processes incoming MQTT commands from the broker.
  - Parses JSON payload containing `actionId` and `parameter`
  - Extracts target node ID from topic string
  - Populates `outPayload` struct with command data for transmission to nodes
  - Expected topic format: `home/gateway/commands/node/{nodeId}`

- `bool getPendingCommand(uint8_t requestingNodeId, LoRaPayload &outPayload)`: Retrieves pending commands for a specific node.
  - Checks for commands addressed to the requesting node
  - `return`: `true` if a pending command exists, `false` otherwise

#### **Message Types and Device Status**

The adapter supports the following message types:

- `MQTT_MSG_UNKNOWN` (0x00): Unknown message
- `MQTT_MSG_GATEWAY_TELEMETRY` (0x01): Gateway status and battery information
- `MQTT_MSG_HEARTBEAT` (0x02): Node heartbeat/status update
- `MQTT_MSG_AUTHENTICATION` (0x03): RFID authentication events
- `MQTT_MSG_MOTIONALARM` (0x04): Motion detection alarms

Device status values:

- `MQTT_DEVICE_ONLINE` (0x1): Device is online and operational
- `MQTT_DEVICE_OFFLINE` (0x0): Device is offline

#### **Usage Example**

For complete working examples, refer to the example files in the `Adapter_Lib/examples/` folder:

- `MQTT_Publish_function_test.cpp`: Demonstrates publishing various MQTT messages (telemetry, events, node status)
- `MQTT_Received_Payload_test.cpp`: Demonstrates receiving and processing incoming MQTT commands from the backend

#### **MQTT Topic Structure**

- **Uplink (Gateway to Backend Server)**:
  - `home/gateway/telemetry`: Gateway status and telemetry
  - `home/nodes/{nodeId}/status`: Node heartbeat and status
  - `home/nodes/{nodeId}/alarm`: Motion alarm events
  - `home/nodes/{nodeId}/authentication`: RFID authentication events

- **Downlink (Backend Server to Gateway)**:
  - `home/gateway/commands/node/{nodeId}`: Commands from backend server to specific nodes

#### **JSON Payload Format**

All MQTT messages use JSON format with the following common fields:

- `nodeId` or `gatewayId`: Identifier of the device
- `type`: Message type identifier
- `deviceStatus`: Current device status (ONLINE/OFFLINE)
- `timestamp`: Object containing:
  - `raw_timestamp`: Unix epoch time
  - `year`, `month`, `day`: Date components
  - `hour`, `minute`, `second`: Time components
- `dataType`: Type of data (varies by message type)
- `value`: Message-specific data value

## **_LoRaWAN Adapter User Guide_**

The LoRaWAN Adapter provides connectivity to The Things Network (TTN) for emergency backup communication. It handles OTAA (Over-The-Air Activation) join procedures and uplink transmission to the LoRaWAN network, enabling cloud-based notifications when local infrastructure is down.

#### **Constructor**

```cpp
LoRaWAN(uint8_t rst_pin, uint8_t rx_pin, uint8_t tx_pin, HardwareSerial &serial,
        const char *devEUI, const char *appEUI, const char *appKey)
```

Creates a LoRaWAN adapter instance with the following parameters:

- `rst_pin`: GPIO pin connected to module reset pin
- `rx_pin`: GPIO pin for UART RX (module TX)
- `tx_pin`: GPIO pin for UART TX (module RX)
- `serial`: Reference to HardwareSerial object (typically `Serial2`)
- `devEUI`: Device EUI from TTN Console (16 hex characters)
- `appEUI`: Application EUI from TTN Console (16 hex characters)
- `appKey`: Application Key from TTN Console (32 hex characters)

#### **Initialization and Network Join**

- `bool init()`: Initializes the LoRaWAN module and configures TTN parameters. Must be called in `setup()`.
  - Performs hardware reset
  - Configures MAC layer for EU868 frequency band
  - Sets DevEUI, AppEUI, and AppKey
  - Enables Adaptive Data Rate (ADR)
  - `return`: `true` if initialization successful, `false` otherwise

- `bool join()`: Performs OTAA join to TTN network.
  - Initiates join procedure
  - Waits up to 30 seconds for network acceptance
  - Must be called after `init()` and before any transmission
  - `return`: `true` if join accepted, `false` if denied or timeout

- `bool isJoined()`: Checks if module is currently joined to network.
  - `return`: `true` if joined, `false` otherwise

#### **LoRaWAN Payload Structure**

The adapter uses a compact 8-byte payload structure optimized for LoRaWAN airtime:

```cpp
struct LoRaWANPayload {
    uint8_t messageType;    // Message type identifier (1 byte)
    uint32_t timestamp;     // Unix timestamp or uptime (4 bytes)
    uint8_t batteryLevel;   // Battery percentage 0-100 (1 byte)
    uint8_t reserved[2];    // Reserved for future use (2 bytes)
};
```

#### **Message Types**

- `LORAWAN_MSG_BLACKOUT` (0x01): Power blackout alert
- `LORAWAN_MSG_RESTORED` (0x02): Power restored notification
- `LORAWAN_MSG_BACKEND_UNREACHABLE` (0x03): Backend server unreachable alert
- `LORAWAN_MSG_CUSTOM` (0xFF): Custom payload

#### **Transmission Methods**

- `bool sendPayload(const LoRaWANPayload &payload, uint8_t port = 1, bool confirmed = false)`: Sends a structured payload.
  - `port`: LoRaWAN port number (1-223)
  - `confirmed`: If `true`, requires network acknowledgment
  - `return`: `true` if transmission successful, `false` otherwise

- `bool sendRaw(const uint8_t *data, uint8_t length, uint8_t port = 1, bool confirmed = false)`: Sends raw byte array.
  - `data`: Pointer to data buffer
  - `length`: Number of bytes to send
  - `return`: `true` if transmission successful, `false` otherwise

- `bool sendHex(const String &hexData, uint8_t port = 1, bool confirmed = false)`: Sends hex-encoded string.
  - `hexData`: Hex string (e.g., "0102030405")
  - `return`: `true` if transmission successful, `false` otherwise

#### **Predefined Emergency Messages**

- `bool sendBlackoutAlert(uint8_t batteryLevel)`: Sends power blackout alert.
  - Uses confirmed transmission with automatic retry mechanism (up to 3 attempts)
  - Automatically populates payload with `LORAWAN_MSG_BLACKOUT` type
  - `batteryLevel`: Current battery percentage (0-100)
  - `return`: `true` if alert sent successfully, `false` otherwise

- `bool sendPowerRestored(uint8_t batteryLevel)`: Sends power restored notification.
  - Uses unconfirmed transmission to save airtime
  - Automatically populates payload with `LORAWAN_MSG_RESTORED` type
  - `return`: `true` if notification sent successfully, `false` otherwise

- `bool sendBackendUnreachableAlert(uint8_t batteryLevel)`: Sends backend server unreachable alert.
  - Uses confirmed transmission with automatic retry mechanism (up to 3 attempts)
  - Automatically populates payload with `LORAWAN_MSG_BACKEND_UNREACHABLE` type
  - `return`: `true` if alert sent successfully, `false` otherwise

#### **Utility Methods**

- `void reset()`: Performs hardware reset of LoRa module.
  - Clears serial buffer
  - Resets module to factory state

- `String getDevEUI()`: Retrieves hardware EUI from module.
  - Useful for verifying device identity
  - `return`: Device EUI as hex string

#### **Power Management Methods**

- `void shutdown()`: Puts module into hardware shutdown mode.
  - Holds RST pin LOW to completely disable module
  - Clears join state
  - Module requires `init()` and `join()` after wakeup

- `void wakeup()`: Wakes module from hardware shutdown.
  - Releases RST pin (sets HIGH)
  - Clears serial buffer
  - Module must be reinitialized with `init()` and `join()` before use

#### **Usage Example**

For a complete working example, refer to `LoRaWAN_emergency_example.cpp` in the `Adapter_Lib/examples/` folder.

#### **TTN Integration**

The LoRaWAN adapter integrates with The Things Network for cloud-based emergency notifications:

1. **Device Registration**: Register your device in TTN Console with OTAA activation
2. **Payload Decoder**: Configure payload formatter in TTN Console to parse binary payload
3. **Webhook Integration**: Configure TTN webhook to forward data to Telegram Bot API or other services
4. **Coverage**: Verify TTN gateway coverage in your area at https://www.thethingsnetwork.org/map

Refer to the LoRaWAN example file for TTN payload formatter implementation.

#### **Best Practices**

- **Airtime Considerations**: LoRaWAN has fair use policy. Avoid sending messages more frequently than necessary
- **Confirmed vs Unconfirmed**: Critical alerts use confirmed transmission with retry. Non-critical messages use unconfirmed
- **Payload Size**: Keep payloads small (<51 bytes) to minimize airtime. The 8-byte payload structure is optimized for LoRaWAN
- **Join Timing**: OTAA join can take 5-30 seconds. Perform join during initialization, not during emergency
- **Battery Optimization**: Use power management methods (`shutdown()`/`wakeup()`) when module is not needed
- **Error Handling**: Always check return values from transmission methods
- **Regional Settings**: Module is configured for EU868 frequency band by default

#### **Troubleshooting**

**Join fails:**

- Verify DevEUI, AppEUI, and AppKey are correct and match TTN Console
- Check TTN gateway coverage in your area
- Ensure module is configured for correct frequency band
- Wait at least 30 seconds for join acceptance

**Transmission fails:**

- Ensure module is joined (`isJoined()` returns `true`)
- Check for duty cycle limitations (wait longer between transmissions)
- Verify TTN gateway is within range and operational
- Check module serial connection and baud rate (57600)

**No data in TTN Console:**

- Verify payload decoder is configured in TTN Application
- Check webhook integration is properly configured
- Monitor TTN gateway traffic in Console to verify uplinks are received
