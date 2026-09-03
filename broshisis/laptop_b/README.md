# Laptop B — MQTT Broker

## What This Does
Runs a Mosquitto MQTT broker that accepts connections from all devices on the hotspot network. Laptop A publishes to it, Laptop C subscribes via WebSockets.

## Install Mosquitto

**macOS (Homebrew):**
```bash
brew install mosquitto
```

> **Note:** Homebrew puts the broker binary in `/opt/homebrew/sbin/` which is NOT on your PATH by default. The client tools (`mosquitto_pub`, `mosquitto_sub`) will work, but the `mosquitto` broker command won't be found unless you use the full path (see below).

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install mosquitto mosquitto-clients
```

**Windows:**
Download from https://mosquitto.org/download/ and install.

## Start the Broker

From this directory (`laptop_b/`), run:

**macOS (use full path):**
```bash
/opt/homebrew/sbin/mosquitto -c mosquitto.conf -v
```

**Linux / Windows (or if mosquitto is on PATH):**
```bash
mosquitto -c mosquitto.conf -v
```

The `-v` flag enables verbose logging — you'll see every connection and message in the terminal.

**Expected output:**
```
1693123456: mosquitto version 2.x.x starting
1693123456: Config loaded from mosquitto.conf.
1693123456: Opening ipv4 listen socket on port 1883.
1693123456: Opening ipv4 listen socket on port 9001.
```

> **Keep this terminal open** — the broker runs in the foreground.

## Find This Laptop's IP Address

You'll need this IP for Laptop A and Laptop C to connect.

**macOS:**
```bash
ipconfig getifaddr en0
```

**Linux:**
```bash
hostname -I | awk '{print $1}'
```

**Windows:**
```bash
ipconfig
```
Look for the IPv4 address under the WiFi adapter (usually `192.168.43.x` on a mobile hotspot).

## Verify the Broker is Working

Open a **second terminal** on Laptop B and run the subscriber logger:

```bash
pip install paho-mqtt   # first time only
python subscriber_log.py
```

Open a **third terminal** and publish a test message:

```bash
# If mosquitto-clients is installed:
mosquitto_pub -h localhost -t "BLEReceiver/001" -m '{"rssi":-50,"packet_count":0}'

# Or use Python:
python -c "import paho.mqtt.client as m; c=m.Client(); c.connect('localhost'); c.publish('BLEReceiver/001', '{\"test\":true}'); c.disconnect()"
```

You should see the message appear in both the verbose broker output and the subscriber logger.

## Firewall Notes

The broker needs to accept incoming connections on ports **1883** and **9001**. If other laptops can't connect:

**macOS:** System Settings → Network → Firewall → ensure it's off, or allow `mosquitto` through.

**Linux:**
```bash
sudo ufw allow 1883
sudo ufw allow 9001
```

**Windows:** Allow `mosquitto.exe` through Windows Firewall when prompted, or add rules manually for ports 1883 and 9001.
