# Smart-Home-Security-System
Project work repository for developing a Smart Home Security System - 34346 Networking technologies and application development for IoT 



# Project Overview

This repository contains the firmware and backend architecture for a flexible, multi-node IoT smart home ecosystem. The system is built around a custom ESP32-based LoRa gateway that orchestrates communication between various low-power sensor and actuator nodes.

The primary objective of this project is to provide a reliable, locally hosted automation platform. It emphasizes data privacy through local encryption, modular node integration, and system resilience, featuring an automated LoRaWAN fallback mechanism to ensure critical notifications are still delivered during power or network outages.


# System Architecture
The system follows a three-tier edge-to-cloud computing model, separating hardware endpoints from backend logic to maximize both battery life and system stability.

1. Edge Layer (The Nodes)

Comprised of application-specific microcontrollers handling individual tasks (e.g., RFID scanning, PIR motion detection, LED actuation).

Nodes maintain a deep-sleep state by default, waking only on interrupt (sensor trigger) to transmit a data payload via LoRa RF, before immediately returning to sleep. No complex logic is handled at this layer.

2. Gateway Layer 

A custom ESP32 acts as the central hub, constantly listening to the local LoRa frequency.

It serves as a protocol bridge, translating custom LoRa RF packets into standardized IP-based MQTT messages.

This layer also acts as the system's fail-safe supervisor. It monitors local power states and manages the hardware switch over to the LoRaWAN backup network if standard Wi-Fi/MQTT routes fail.

3. Application Layer (The Backend)

Hosted on a localized server running Docker, ensuring that the software environment is portable and reproducible.

All complex processing—such as user authentication and user interface hosting is isolated here.


# Security Model
To ensure data integrity and prevent unauthorized access, the architecture implements a security model across both RF and IP communication boundaries:

- Link Layer Security (AES): All LoRa P2P communication between the end-nodes and the central gateway is secured using the Advanced Encryption Standard (AES). 

- Transport Layer Security (TLS): All IP-based traffic between the custom gateway and the localized Mosquitto message broker is encrypted via TLS (MQTT). 

# Technology Stack

This project integrates a diverse stack of hardware, communication protocols, and software infrastructure:

- **Hardware**
    -   Microcontrollers: ESP32 (Used for both Gateway and End-Nodes).

    - Radios: LoRa RN2483 communication module 

    - Sensors/Actuators: RC522 (RFID), Standard PIR sensors, LEDs.

- **Protocols & Security**
    - LoRa P2P: Point-to-Point communication for local node-to-gateway traffic (AES Encrypted).

    - LoRaWAN: Low-Power Wide-Area Network protocol used exclusively for emergency cloud uplink using TTN.

    - MQTT: Lightweight messaging protocol for device-to-backend communication (TLS Encrypted).


- **Containerization**: Docker & Docker Compose for isolated and reproducible backend deployment.

- **Message Broker**: Eclipse Mosquitto (configured for MQTTS).

- **Automation Platform**: Home Assistant (serves as the primary logic engine and user dashboard).

- **External APIs**: The Things Network (TTN) for LoRaWAN routing; Telegram Bot API for push notifications.

# Project Structure

### `/Adapter_Lib`
**Shared Communication Library** — Custom LoRa P2P and MQTT communication abstraction layer
- `docs/` — Library documentation and setup guides
  - `Library_User_Guide.md` — Complete API reference and usage examples
  - `TTN_TELEGRAM_SETUP.md` — LoRaWAN and Telegram integration guides
- `examples/` — Ready to use example sketches for each LoRa and MQTT protocols
- `src/` — API implementation for LoRaP2P, LoRaWAN and MQTT protocols
  

### `/Gateway`
**ESP32 Central Gateway Firmware** — Main hub that bridges LoRa RF to MQTT/IP
- `platformio.ini` — PlatformIO project configuration with library dependencies
- `src/` — Gateway firmware source code
  - Main gateway application handling RF-to-IP protocol translation, monitoring, and LoRaWAN failover logic

### `/backend-server`
**Backend Services** 
-  Docker-based application layer
    - `docker-compose.yml` — Multi-container orchestration configuration
    - `README.md` — Backend setup and deployment instructions
    - `home-assistant/` — Home Assistant configuration and integrations
    - `mosquitto/` — MQTT broker (Mosquitto) configuration for TLS-encrypted message handling

### `/nodes`
**ESP32 Nodes Firmware** — PlatformIO projects
- `alarm-node` - Alarm node implementation
- `authentication-node`- RFID authentication node implementation
- `motion-node` - Motion sensor node implementation

---


# Contribution Table


| Contributor |Github name| Contributed Files | Topics / Areas |
|---|---|---|---|
| Mark Toth - s252839|donmonki| `/Adapter_Lib/*`, `/backend-server/mosquitto` | LoRaP2P Protocol, MQTT protocol, Encryption implementation |
| Kristian Greif - s257578 |kristiangreif| `/Adapter_Lib/src/LoRaWAN_Adapter.h & .cpp`, `/backend-server/*` | Backend server (HomeAssistant + mosquitto) docker setup, Backend MQTT integration with TLS, LoRaWAN protocol, TTN -> Pipedream -> Telegram bot setup |
| Lex Van Cauter - s234556 | Trilex214 | `/Adapter_Lib/LoRaP2P_Adapter.cpp`, `/Gateway/*` | Gateway implementation and orchestration, LBT + ACK methods in LoRa P2P |
| Mairo Trump - s234544 | LilMirts | `/backend-server/home-assistant/*` | Backend logic implementation, MQTT and Telegram integrations in HA |


# Github Development guide

!IMPORTANT!
Please do your developments on **branches** and when you are done with that, create a pull request for merging back to the main!


For each component you are developing, create a different project folder using PlatformIO in the repository.

In each PlatformIO project folder which requires to use the shared communication library, in the `platformio.ini` file include the following lines:

```
lib_extra_dirs = ../Adapter_Lib

lib_deps =
    knolleary/PubSubClient
    bblanchon/ArduinoJson
```


For the usage of the shared communication library, please refer to the library documentation and example files within the `Adapter_Lib` folder.
