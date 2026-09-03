# Laptop A — Serial-to-MQTT Bridge

## What This Does
Reads Serial output from the Arduino Nano 33 IoT (connected via USB) and publishes the JSON payload to an MQTT broker over WiFi.

## Prerequisites
- Python 3.7+
- Arduino Nano 33 IoT connected via USB

## Install Dependencies
```bash
pip install pyserial paho-mqtt
```

## Find Your Serial Port

**macOS:**
```bash
ls /dev/cu.usbmodem*
```
Look for something like `/dev/cu.usbmodem14101`.

**Linux:**
```bash
ls /dev/ttyACM*
```
Usually `/dev/ttyACM0`.

**Windows:**
Open Device Manager → Ports (COM & LPT). Look for the Arduino port (e.g. `COM3`).

## Usage
```bash
python serial_to_mqtt.py --port <SERIAL_PORT> --broker <BROKER_IP>
```

**Example (macOS):**
```bash
python serial_to_mqtt.py --port /dev/cu.usbmodem14101 --broker 192.168.43.100
```

**Example (Linux):**
```bash
python serial_to_mqtt.py --port /dev/ttyACM0 --broker 192.168.43.100
```

**Example (Windows):**
```bash
python serial_to_mqtt.py --port COM3 --broker 192.168.43.100
```

Replace `192.168.43.100` with Laptop B's actual IP address on the hotspot.

## Optional Arguments
- `--broker-port 1883` — Broker port (default: 1883)
- `--baud 115200` — Baud rate (default: 115200, matches Arduino sketch)

## What You Should See
When the BLE switch is pressed, console output will look like:
```
[15:30:01] Discovered target peripheral
[15:30:01] Packet Count: 1
[15:30:01] RSSI: -45
[15:30:01] Payload:{"rssi":-45,"packet_count":1}
[MQTT] Published to BLEReceiver/001: {"rssi":-45,"packet_count":1}
[15:30:01] Data Sent
```
