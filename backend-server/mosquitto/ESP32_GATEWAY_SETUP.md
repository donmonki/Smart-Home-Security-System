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

The script creates:
- `ca.crt` - Root CA certificate (goes on ESP32)
- `server.crt` - Server certificate for Mosquitto broker
- `server.key` - Server private key for Mosquitto

**Important Configuration:**

Before running the script, ensure the CN and SAN values match your server's IP or hostname:
- Update `CN` (Common Name) in the certificate subject
- Update DNS entries and IP entries in Subject Alternative Names (SAN)
- The script defaults use placeholder values - these MUST be customized

Creates:

- `ca.crt` - Root certificate for ESP32
- `server.crt` / `server.key` - Server certificates for Mosquitto

### 2. Extract CA Certificate for ESP32

```bash
cat certs/ca.crt
```

Copy the entire certificate (including `-----BEGIN CERTIFICATE-----` and `-----END CERTIFICATE-----` lines) to embed in your ESP32 code as `ROOT_CA_CERT`.

### 3. ESP32 Configuration

**Connection Settings:**

- **Host**: Your server IP (e.g., 192.168.0.188)
- **Port**: `8883`
- **CA Certificate**: Full contents from `ca.crt` (PEM format)
- **Libraries**: WiFiClientSecure, PubSubClient

**Key Points:**

- Use `WiFiClientSecure` instead of `WiFiClient`
- Load CA certificate with `_wifiClient.setCACert(ROOT_CA_CERT)`
- Ensure proper time sync via NTP (TLS requires valid system time)
- Never use `setInsecure()` in production

## Certificate Verification & Debugging

### Step 1: Verify Certificate Format

Ensure the CA certificate is valid PEM format:

```bash
openssl x509 -in certs/ca.crt -text -noout
```

Expected output should show:
- `Certificate:`
- `Subject: ... CN = SmartHome-CA`
- `Issuer: ... CN = SmartHome-CA` (self-signed)
- `X509v3 Basic Constraints: critical CA:TRUE`

### Step 2: Verify Server Certificate

Check that the server certificate was properly signed by the CA:

```bash
openssl x509 -in certs/server.crt -text -noout
```

Look for:
- `Issuer: ... CN = SmartHome-CA` (must match CA's CN)
- `Subject: ... CN = <YOUR_SERVER_IP>`
- `X509v3 Subject Alternative Name:` with your DNS entries and IP addresses

### Step 3: Verify Certificate Chain

Verify the server certificate is signed by the CA:

```bash
openssl verify -CAfile certs/ca.crt certs/server.crt
```

Expected output: `certs/server.crt: OK`

### Step 4: Check Certificate Matching

Verify the CA certificate matches the server certificate's issuer:

```bash
# Extract issuer from server cert
openssl x509 -in certs/server.crt -noout -issuer

# Extract subject from CA cert
openssl x509 -in certs/ca.crt -noout -subject
```

The `issuer` from server cert should match the `subject` from CA cert.

### Step 5: Verify Subject Alternative Names (SAN)

Confirm your server's IP/hostname is in the SAN:

```bash
openssl x509 -in certs/server.crt -noout -text | grep -A 1 "Subject Alternative Name"
```

Expected output should include:
```
DNS:localhost, DNS:mosquitto, DNS:192.168.0.188, IP Address:127.0.0.1, IP Address:192.168.0.188
```

### Step 6: Check Certificate Validity Period

Ensure certificates are not expired:

```bash
openssl x509 -in certs/ca.crt -noout -dates
openssl x509 -in certs/server.crt -noout -dates
```

Both should show `notBefore` and `notAfter` dates with `notAfter` being 10 years from generation date.

## Testing

### Test Mosquitto Broker

Verify Mosquitto is using the certificates:

```bash
# Check if broker is running
docker-compose ps mosquitto

# View Mosquitto logs
docker-compose logs mosquitto | grep -i "tls\|cert\|8883"
```

### Test TLS Connection from Computer

Test TLS connection using mosquitto_pub:

```bash
# Install mosquitto clients if needed
brew install mosquitto  # macOS
apt install mosquitto-clients  # Ubuntu/Debian

# Test publish with TLS
mosquitto_pub -h <broker-ip> -p 8883 \
  --cafile certs/ca.crt \
  -t "test/topic" -m "Hello from TLS"

# Test subscribe
mosquitto_sub -h <broker-ip> -p 8883 \
  --cafile certs/ca.crt \
  -t "test/topic"
```

### Test ESP32 Connection

Monitor ESP32 output and look for:
- `[MQTT] Configuring TLS/SSL encryption...`
- `[MQTT] Server configured: <ip>:8883`
- `[MQTT] Attempting TLS connection...`
- `[MQTT] Connected!` and `TLS/SSL handshake successful`

If connection fails, check:
- NTP time sync successful (required for TLS)
- Certificate validation in Serial monitor
- Port 8883 accessibility from ESP32 network

## Troubleshooting

### Connection fails - Certificate verification error

**Cause**: Certificate mismatch or improper format.

**Solutions**:
1. Verify certificate chain: `openssl verify -CAfile certs/ca.crt certs/server.crt`
2. Check ESP32's certificate copy in MQTT_Adapter.h - must include BEGIN/END lines
3. Ensure no extra whitespace or formatting issues in ROOT_CA_CERT variable
4. Verify no newline at the START of the certificate (some cert tools add this)

### Connection fails - Connection refused

**Cause**: Mosquitto not running or not listening on 8883.

**Solutions**:
```bash
# Verify Mosquitto is running
docker-compose ps mosquitto

# Restart broker with new certificates
docker-compose restart mosquitto

# Check Mosquitto logs
docker-compose logs mosquitto | tail -50
```

### Connection times out

**Cause**: Network/firewall issue or wrong IP.

**Solutions**:
```bash
# Test port accessibility
telnet <broker-ip> 8883

# Verify correct IP in certificate SAN
openssl x509 -in certs/server.crt -noout -text | grep "IP Address"
```

### TLS handshake failure 

**Cause**: System time on ESP32 is wrong or certificate not valid yet.

**Solutions**:
1. Verify NTP sync: Check Serial logs for `[NTP] Time Synced!`
2. Check certificate validity dates: `openssl x509 -in certs/ca.crt -noout -dates`
3. Regenerate certificates if they're outdated

### Certificate validation fails

**Last resort only** - For debugging:

Temporarily enable insecure mode to test if TLS layer works:

```cpp
// In MQTT_Adapter.cpp init() function - ONLY FOR DEBUGGING
_wifiClient.setInsecure(); // Skips all certificate validation
```

If this connects successfully, the issue is certificate-related. Re-verify the certificate chain using steps above.

## Certificate Renewal

Certificates are valid for 10 years. To regenerate:

```bash
rm -rf certs/
./generate-certs.sh
docker-compose restart mosquitto
# Re-flash ESP32 with new ROOT_CA_CERT
```

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
