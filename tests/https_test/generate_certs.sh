#!/bin/bash

# Simple SSL Certificate Generator for macOS/Linux
# Generates self-signed certificates for HTTPS testing

set -e

# Check for OpenSSL
if ! command -v openssl >/dev/null 2>&1; then
    echo "Error: OpenSSL not found. Install with: brew install openssl"
    exit 1
fi

echo "🔐 Generating SSL certificates..."

# Generate server private key
openssl genrsa -out test_key.pem 2048 2>/dev/null

# Generate server certificate
openssl req -new -x509 -key test_key.pem -out test_cert.pem -days 365 \
    -subj "/C=US/ST=Test/L=Test/O=Test/CN=localhost" \
    -addext "subjectAltName=DNS:localhost,DNS:*.localhost,IP:127.0.0.1,IP:::1" \
    2>/dev/null

# Generate client private key and certificate
openssl genrsa -out test_client_key.pem 2048 2>/dev/null
openssl req -new -x509 -key test_client_key.pem -out test_client_cert.pem -days 365 \
    -subj "/C=US/ST=Test/L=Test/O=TestClient/CN=test-client" \
    2>/dev/null

# Create combined file
cat test_cert.pem test_key.pem > test_combined.pem

# Set permissions
chmod 600 test_key.pem test_client_key.pem test_combined.pem
chmod 644 test_cert.pem test_client_cert.pem

echo "✅ Certificates generated:"
echo "   test_cert.pem - Server certificate"
echo "   test_key.pem - Server private key"
echo "   test_client_cert.pem - Client certificate"
echo "   test_client_key.pem - Client private key"
echo "   test_combined.pem - Combined cert + key"

echo
echo "🚀 Usage in C++:"
echo "   server.load_certificate(\"test_cert.pem\", \"test_key.pem\");"

echo
echo "🔍 Test with:"
echo "   curl -k https://localhost:8443/"
echo "   openssl s_client -connect localhost:8443"