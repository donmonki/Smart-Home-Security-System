#!/bin/bash
# Script to generate self-signed certificates for Mosquitto MQTT broker
# For local development with ESP32 gateway

CERT_DIR="./certs"
DAYS_VALID=3650  # 10 years for local development

echo "🔐 Generating certificates for Mosquitto MQTT broker..."
echo "Certificate directory: $CERT_DIR"

# Create certs directory if it doesn't exist
mkdir -p "$CERT_DIR"

# 1. Generate CA (Certificate Authority) key and certificate
echo ""
echo "Step 1: Creating Certificate Authority (CA)..."
openssl genrsa -out "$CERT_DIR/ca.key" 2048

openssl req -new -x509 -days $DAYS_VALID -key "$CERT_DIR/ca.key" -out "$CERT_DIR/ca.crt" \
  -subj "/C=US/ST=State/L=City/O=SmartHome/OU=Security/CN=SmartHome-CA"

echo "✅ CA certificate created: ca.crt (This goes on your ESP32)"

# 2. Generate server key
echo ""
echo "Step 2: Creating server private key..."
openssl genrsa -out "$CERT_DIR/server.key" 2048
echo "✅ Server key created: server.key"

# 3. Create OpenSSL config for SAN (Subject Alternative Names)
echo ""
echo "Step 3: Creating OpenSSL config for Subject Alternative Names..."
cat > "$CERT_DIR/san.cnf" << EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
C = US
ST = State
L = City
O = SmartHome
OU = Security
CN = localhost

[v3_req]
subjectAltName = @alt_names

[alt_names]
DNS.1 = localhost
DNS.2 = mosquitto
IP.1 = 127.0.0.1
EOF

# 4. Generate server certificate signing request (CSR) with SAN
echo ""
echo "Step 4: Creating server certificate signing request with SAN..."
openssl req -new -key "$CERT_DIR/server.key" -out "$CERT_DIR/server.csr" \
  -config "$CERT_DIR/san.cnf"

# 5. Sign the server certificate with CA
echo ""
echo "Step 5: Signing server certificate with CA..."
openssl x509 -req -in "$CERT_DIR/server.csr" -CA "$CERT_DIR/ca.crt" \
  -CAkey "$CERT_DIR/ca.key" -CAcreateserial -out "$CERT_DIR/server.crt" \
  -days $DAYS_VALID -extensions v3_req -extfile "$CERT_DIR/san.cnf"

echo "✅ Server certificate created: server.crt (valid for localhost, mosquitto, 127.0.0.1)"

# Clean up temporary files
rm "$CERT_DIR/server.csr" "$CERT_DIR/san.cnf"

# Set appropriate permissions
chmod 644 "$CERT_DIR/ca.crt"
chmod 644 "$CERT_DIR/server.crt"
chmod 600 "$CERT_DIR/server.key"

echo ""
echo "✨ Certificate generation complete!"
echo ""
echo "📁 Generated files:"
echo "  - ca.crt        (Root certificate - copy this to ESP32)"
echo "  - server.crt    (Server certificate for Mosquitto)"
echo "  - server.key    (Server private key for Mosquitto)"
echo ""
echo "📋 Next steps:"
echo "  1. Restart Mosquitto broker: docker-compose restart mosquitto"
echo "  2. Copy ca.crt contents to your ESP32 code"
echo "  3. Configure ESP32 to connect to port 8883 with TLS"
echo ""
echo "To view the CA certificate for ESP32:"
echo "  cat $CERT_DIR/ca.crt"
