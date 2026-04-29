#!/bin/bash
# Script to generate self-signed certificates for Mosquitto MQTT broker
# For local development with ESP32 gateway

CERT_DIR="./certs"
DAYS_VALID=3650  # 10 years for local development

echo "Generating certificates for Mosquitto MQTT broker..."
echo "Certificate directory: $CERT_DIR"

# Create certs directory if it doesn't exist
mkdir -p "$CERT_DIR"

# ---------------------------------------------------------
# 1. Generate CA (Certificate Authority)
# ---------------------------------------------------------
echo ""
echo "Step 1: Creating Certificate Authority (CA)..."

# Create a config specifically for the CA so mbedTLS accepts it
cat > "$CERT_DIR/ca.cnf" << EOF
[req]
distinguished_name = req_distinguished_name
x509_extensions = v3_ca
prompt = no

[req_distinguished_name]
C = US
ST = State
L = City
O = SmartHome
OU = Security
CN = SmartHome-CA

[v3_ca]
basicConstraints = critical,CA:TRUE
keyUsage = critical, digitalSignature, cRLSign, keyCertSign
EOF

openssl genrsa -out "$CERT_DIR/ca.key" 2048

# Pass the CA config to ensure the CA:TRUE constraint is applied
openssl req -new -x509 -days $DAYS_VALID -key "$CERT_DIR/ca.key" \
  -out "$CERT_DIR/ca.crt" -config "$CERT_DIR/ca.cnf"

echo "CA certificate created: ca.crt (This goes on your ESP32)"


# ---------------------------------------------------------
# 2. Generate Server Key
# ---------------------------------------------------------
echo ""
echo "Step 2: Creating server private key..."
openssl genrsa -out "$CERT_DIR/server.key" 2048
echo "Server key created: server.key"


# ---------------------------------------------------------
# 3. Create OpenSSL config for SAN
# ---------------------------------------------------------
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
CN = YOUR_SERVER_IP_OR_HOSTNAME
# IMPORTANT: Update the CN to match your server's IP or hostname!!!!!!!!!!
[v3_req]
subjectAltName = @alt_names
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
# IMPORTANT: Update the DNS.3 and IP.2 to match your server's IP or hostname!!!!!!!!!!
[alt_names]
DNS.1 = localhost
DNS.2 = mosquitto
DNS.3 = YOUR_SERVER_IP_OR_HOSTNAME
IP.1 = 127.0.0.1
IP.2 = YOUR_SERVER_IP_OR_HOSTNAME
EOF


# ---------------------------------------------------------
# 4 & 5. Generate CSR and Sign Server Certificate
# ---------------------------------------------------------
echo ""
echo "--------------------Certificate Generation---------------------"
echo "Step 4: Creating and signing server certificate..."
openssl req -new -key "$CERT_DIR/server.key" -out "$CERT_DIR/server.csr" \
  -config "$CERT_DIR/san.cnf"

openssl x509 -req -in "$CERT_DIR/server.csr" -CA "$CERT_DIR/ca.crt" \
  -CAkey "$CERT_DIR/ca.key" -CAcreateserial -out "$CERT_DIR/server.crt" \
  -days $DAYS_VALID -extensions v3_req -extfile "$CERT_DIR/san.cnf"

echo "Server certificate created: server.crt"

# Clean up temporary files
rm "$CERT_DIR/server.csr" "$CERT_DIR/san.cnf" "$CERT_DIR/ca.cnf"

# Set appropriate permissions for Docker
chmod 644 "$CERT_DIR/ca.crt"
chmod 644 "$CERT_DIR/server.crt"
chmod 644 "$CERT_DIR/server.key" # Changed to 644 so Mosquitto container can read it

echo ""
echo " ----Certificate generation COMPLETED!----"
echo ""
echo " Generated files:"
echo "  - ca.crt        (Root certificate - copy this to ESP32)"
echo "  - server.crt    (Server certificate for Mosquitto)"
echo "  - server.key    (Server private key for Mosquitto)"
echo ""
echo " Next steps:"
echo "  1. Restart Mosquitto broker: docker-compose restart mosquitto"
echo "  2. Copy ca.crt contents to your ESP32 code"
echo "  3. Configure ESP32 to connect to port 8883 with TLS"
echo "  4. Copy the ca.crt to the MQTT_Adapter.h file in the ROOT_CA_CERT variable"
echo ""
echo "To view the CA certificate for ESP32:"
echo "  cat $CERT_DIR/ca.crt"