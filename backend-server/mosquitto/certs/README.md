# 🔐 MQTT TLS Certificates Directory

This directory contains TLS certificates for secure MQTT communication with the ESP32 gateway.

## ⚠️ Important: Certificates Are NOT in Git

For security reasons, certificate files (`.crt`, `.key`, `.srl`) are **excluded from version control** via `.gitignore`.

## 🔨 How to Generate Certificates

Each team member must generate their own certificates locally:

```bash
cd backend-server/mosquitto
./generate-certs.sh
```

This script will create:

- `ca.crt` - Root CA certificate (copy this to ESP32 code)
- `server.crt` - Server certificate for Mosquitto broker
- `server.key` - Server private key
- `ca.srl` - Certificate serial number file

## 📋 After Generation

1. Restart Mosquitto to use the new certificates:

   ```bash
   docker-compose restart mqtt
   ```

2. Copy the contents of `ca.crt` to your ESP32 gateway code

3. See `ESP32_GATEWAY_SETUP.md` for complete setup instructions

## 🤝 Team Collaboration

- ✅ **DO commit**: The `generate-certs.sh` script
- ✅ **DO commit**: This README file
- ❌ **DON'T commit**: Any `.crt`, `.key`, or `.srl` files
- ❌ **DON'T share**: Your private keys with anyone

## 🔍 Verification

After generating, verify the certificates exist:

```bash
ls -la certs/
```

You should see:

- `ca.crt`
- `server.crt`
- `server.key`
- `ca.srl` (optional, used for signing)
