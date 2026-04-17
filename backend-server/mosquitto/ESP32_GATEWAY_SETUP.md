# ESP32 Gateway - MQTT TLS Setup Guide

Quick reference for connecting your ESP32 gateway to Mosquitto with TLS encryption.

## Overview

- **Connection**: Port **8883** with TLS/SSL encryption
- **Authentication**: Gateway verifies broker with CA certificate
- **Security**: Encrypted traffic, no client certificates needed

## Setup Steps

### 1. Generate Certificates

```bash
cd backend-server/mosquitto
./generate-certs.sh
```

Creates:

- `ca.crt` - Root certificate for ESP32
- `server.crt` / `server.key` - Server certificates for Mosquitto

### 2. Get CA Certificate for ESP32

```bash
cat certs/ca.crt
```

Copy the entire certificate (including BEGIN/END lines) to embed in your ESP32 code.

### 3. ESP32 Configuration

**Connection Settings:**

- **Host**: Your server IP (e.g., 192.168.1.100)
- **Port**: `8883`
- **CA Certificate**: Contents from `ca.crt`
- **Libraries**: WiFiClientSecure, PubSubClient

**Key Points:**

- Use `WiFiClientSecure` instead of `WiFiClient`
- Load CA certificate with `setCACert()`
- Never use `setInsecure()` in production

## Testing

Test TLS connection from your computer:

```bash
# Install mosquitto clients
brew install mosquitto  # macOS

# Test publish
mosquitto_pub -h <broker-ip> -p 8883 \
  --cafile backend-server/mosquitto/certs/ca.crt \
  -t "test/topic" -m "Hello from TLS"
```

## Troubleshooting

**Connection fails:**

- Verify Mosquitto is running: `docker-compose ps`
- Check logs: `docker-compose logs mosquitto`
- Test port access: `telnet <broker-ip> 8883`

**Certificate errors:**

- Ensure entire CA certificate is copied (including BEGIN/END)
- Verify certificate isn't expired
- Confirm server IP is correct

## Optional: Password Authentication

Add username/password for extra security:

```bash
docker-compose exec mosquitto mosquitto_passwd -c /mosquitto/config/passwd gateway_user
```

Then update `mosquitto.conf`:

```conf
allow_anonymous false
password_file /mosquitto/config/passwd
```
