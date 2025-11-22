#!/usr/bin/env bash
set -euo pipefail

# Usage:
#  ./create_mosquitto_passwd.sh <username> [password]
# If password is not provided as second argument the script will prompt for it interactively.

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG_DIR="$ROOT_DIR/config"
PASSFILE="$CONFIG_DIR/passwords"

if [ -z "${1-}" ]; then
  echo "Usage: $0 <username> [password]"
  exit 1
fi
USER="$1"

# Check if Docker is running
echo "Checking if Docker is running..."
if ! docker version >/dev/null 2>&1; then
  echo "❌ Docker is not running or not accessible."
  echo ""
  echo "Manual alternative:"
  echo "1. Install mosquitto locally: apt-get install mosquitto-clients (Ubuntu) or brew install mosquitto (macOS)"
  echo "2. Run: mosquitto_passwd -c \"$PASSFILE\" $USER"
  echo "3. Or start Docker and run this script again"
  exit 1
fi
echo "✅ Docker is running"

# Ensure config directory exists
mkdir -p "$CONFIG_DIR"

echo "Creating password file for user: $USER"
echo "Config directory: $CONFIG_DIR"

# If password provided use -b (non-interactive). Otherwise run interactive mosquitto_passwd.
if [ -n "${2-}" ]; then
  PASS="$2"
  echo "Running: docker run --rm -v \"$CONFIG_DIR\":/mosquitto/config eclipse-mosquitto mosquitto_passwd -c -b /mosquitto/config/passwords $USER [password hidden]"
  docker run --rm -v "$CONFIG_DIR":/mosquitto/config eclipse-mosquitto mosquitto_passwd -c -b /mosquitto/config/passwords "$USER" "$PASS"
else
  echo "No password provided as argument. You will be prompted to enter it interactively."
  echo "Running: docker run --rm -it -v \"$CONFIG_DIR\":/mosquitto/config eclipse-mosquitto mosquitto_passwd -c /mosquitto/config/passwords $USER"
  docker run --rm -it -v "$CONFIG_DIR":/mosquitto/config eclipse-mosquitto mosquitto_passwd -c /mosquitto/config/passwords "$USER"
fi

if [ $? -eq 0 ]; then
  echo "✅ Password file created successfully at: $PASSFILE"
  if [ -f "$PASSFILE" ]; then
    echo "File details:"
    ls -l "$PASSFILE"
    echo ""
    echo "Next steps:"
    echo "1. Set environment variables:"
    echo "   export MQTT_USERNAME='$USER'"
    echo "   export MQTT_PASSWORD='[your_password]'"
    echo "2. Run: docker-compose -f docker-compose.backend.yml up -d"
  else
    echo "⚠️  Password file was not created at expected location: $PASSFILE"
  fi
else
  echo "❌ Failed to create password file."
  echo "Try checking Docker logs or running Docker with sudo."
fi