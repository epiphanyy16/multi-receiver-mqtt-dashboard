# Testing Guide — 3-Machine BLE Emergency Switch Demo

This guide walks you through verifying each stage of the pipeline, from broker to full end-to-end.

**Notation:** Throughout this guide, `<BROKER_IP>` means Laptop B's IP address on the hotspot (e.g. `192.168.43.100`).

---

## Prerequisites Checklist

Before testing, make sure:
- [ ] All 3 laptops are connected to the **same WiFi** (mobile hotspot)
- [ ] You know Laptop B's IP address (see Laptop B README)
- [ ] Mosquitto is installed on Laptop B
- [ ] Python 3 + `pyserial` + `paho-mqtt` are installed on Laptop A
- [ ] Python 3 + `paho-mqtt` is installed on Laptop B (for the subscriber logger)
- [ ] Arduino Nano 33 IoT is connected to Laptop A via USB

---

## Stage A: Verify the Broker is Running (Laptop B)

### A1. Start the broker

On Laptop B, open a terminal in the `laptop_b/` directory:

```bash
mosquitto -c mosquitto.conf -v
```

**✅ Expected output:**
```
1693123456: mosquitto version 2.x.x starting
1693123456: Config loaded from mosquitto.conf.
1693123456: Opening ipv4 listen socket on port 1883.
1693123456: Opening ipv4 listen socket on port 9001.
1693123456: mosquitto version 2.x.x running
```

**❌ If you see "Error: Address already in use":** Another Mosquitto instance (or another program) is using port 1883 or 9001. Kill it with `killall mosquitto` or change the port.

### A2. Test pub/sub locally on Laptop B

Open a **second terminal** on Laptop B:
```bash
python subscriber_log.py
```

**✅ Expected output:**
```
[INFO] Connecting to broker at localhost:1883...
[CONNECTED] Subscribed to topic: BLEReceiver/001
[INFO] Waiting for messages (Ctrl+C to quit)...
```

Open a **third terminal** on Laptop B and publish a test message:
```bash
mosquitto_pub -h localhost -t "BLEReceiver/001" -m '{"rssi":-99,"packet_count":0}'
```

Or if `mosquitto_pub` isn't available:
```bash
python -c "
import paho.mqtt.client as m
c = m.Client()
c.connect('localhost')
c.publish('BLEReceiver/001', '{\"rssi\":-99,\"packet_count\":0}')
c.disconnect()
"
```

**✅ Expected output in subscriber terminal:**
```
[2026-08-27 15:30:00] Topic: BLEReceiver/001  |  Payload: {"rssi":-99,"packet_count":0}
```

**✅ Expected output in broker (verbose) terminal:**
```
... Received PUBLISH from ... (... 'BLEReceiver/001', ...)
... Sending PUBLISH to ... (... 'BLEReceiver/001', ...)
```

### A3. Test connectivity from another laptop

From Laptop A or Laptop C, run:
```bash
mosquitto_pub -h <BROKER_IP> -t "BLEReceiver/001" -m '{"test":"from_remote"}'
```

Or:
```bash
python -c "
import paho.mqtt.client as m
c = m.Client()
c.connect('<BROKER_IP>')
c.publish('BLEReceiver/001', '{\"test\":\"from_remote\"}')
c.disconnect()
"
```

**✅ The message should appear** in Laptop B's subscriber logger and broker verbose output.

**❌ If it doesn't connect:** Check firewall on Laptop B (see Laptop B README). Try pinging `<BROKER_IP>` from the other laptop.

---

## Stage B: Verify Serial Reading (Laptop A)

### B1. Identify the serial port

**macOS:**
```bash
ls /dev/cu.usbmodem*
```

**Linux:**
```bash
ls /dev/ttyACM*
```

**Windows:** Check Device Manager → Ports.

### B2. Test serial output directly

Before running the bridge, verify the Arduino is outputting data. Use any serial monitor:

**macOS/Linux (with screen):**
```bash
screen /dev/cu.usbmodem14101 115200
```
(Exit with `Ctrl+A` then `K` then `Y`)

**Or with Python:**
```bash
python -c "
import serial, time
s = serial.Serial('/dev/cu.usbmodem14101', 115200, timeout=1)
print('Listening... press the BLE switch')
while True:
    line = s.readline().decode('utf-8', errors='replace').strip()
    if line:
        print(line)
"
```

**✅ When the BLE switch is pressed, you should see:**
```
Discovered target peripheral
Packet Count: 1
RSSI: -45
Payload:{"rssi":-45,"packet_count":1}
Data Sent
```

**❌ If no output:** Check USB connection, make sure no other program (Arduino IDE Serial Monitor) has the port open, verify the port name is correct.

---

## Stage C: Verify Serial → MQTT Publishing (Laptop A → Laptop B)

### C1. Start the bridge on Laptop A

```bash
python serial_to_mqtt.py --port /dev/cu.usbmodem14101 --broker <BROKER_IP>
```

**✅ Expected startup output:**
```
[MQTT] Connecting to broker at <BROKER_IP>:1883...
[SERIAL] Opening /dev/cu.usbmodem14101 at 115200 baud...
[SERIAL] Port opened. Waiting for data...

============================================================
  Serial Monitor + MQTT Bridge Active
  Port: /dev/cu.usbmodem14101  |  Broker: <BROKER_IP>:1883
  Topic: BLEReceiver/001
============================================================

[MQTT] Connected to broker successfully
```

### C2. Press the BLE switch

**✅ Expected output on Laptop A (bridge console):**
```
[15:30:01] Discovered target peripheral
[15:30:01] Packet Count: 1
[15:30:01] RSSI: -45
[15:30:01] Payload:{"rssi":-45,"packet_count":1}
[MQTT] Published to BLEReceiver/001: {"rssi":-45,"packet_count":1}
[15:30:01] Data Sent
```

**✅ Expected output on Laptop B (subscriber logger):**
```
[2026-08-27 15:30:01] Topic: BLEReceiver/001  |  Payload: {"rssi":-45,"packet_count":1}
```

**❌ If the bridge connects to MQTT but nothing shows on Laptop B:** Make sure the subscriber is running and subscribed to `BLEReceiver/001`.

**❌ If `[MQTT] ERROR: Could not connect to broker`:** Check broker IP, firewall, network connectivity.

---

## Stage D: Verify the Dashboard (Laptop C)

### D1. Open the dashboard

Open `laptop_c/dashboard.html` in a browser on Laptop C.

### D2. Connect to the broker

1. Enter `<BROKER_IP>` in the "Broker IP" field
2. Leave the WS Port as `9001`
3. Click **Connect**

**✅ Expected:**
- Status dot turns **green**
- Log shows:
  ```
  [15:30:00] Connecting to ws://<BROKER_IP>:9001...
  [15:30:00] Connected to broker at <BROKER_IP>
  [15:30:00] Subscribed to topic: BLEReceiver/001
  ```

**❌ If the dot stays red / "Connection closed" appears:**
- Verify Laptop B's broker is running with the WebSocket listener on port 9001
- Check firewall on Laptop B for port 9001
- Open browser dev console (F12 → Console) for detailed WebSocket errors
- Make sure you're using `ws://` not `wss://` (the config doesn't use TLS)

### D3. Test with a manual publish

From Laptop B (or any laptop with `mosquitto_pub`):
```bash
mosquitto_pub -h <BROKER_IP> -t "BLEReceiver/001" -m '{"rssi":-77,"packet_count":42}'
```

**✅ Expected on dashboard:**
```
[15:30:05] RSSI: -77 | Packet #: 42 | Raw: {"rssi":-77,"packet_count":42}
```
Stats bar should update: "Packets received: 1", "Last RSSI: -77", "Last packet #: 42".

---

## Stage E: Full End-to-End Test

With everything running:
- **Laptop B:** Mosquitto broker + subscriber logger
- **Laptop A:** Serial-to-MQTT bridge
- **Laptop C:** Dashboard open and connected

### E1. Press the BLE switch

### E2. Verify on all 3 laptops

| Laptop | Where to look | What you should see |
|--------|--------------|---------------------|
| **A** | Bridge console | Serial lines + `[MQTT] Published to BLEReceiver/001: {"rssi":...}` |
| **B** | Subscriber logger | `[timestamp] Topic: BLEReceiver/001 \| Payload: {"rssi":...}` |
| **B** | Broker verbose output | `Received PUBLISH from LaptopA_SerialBridge` + `Sending PUBLISH to LaptopC_Dashboard_...` |
| **C** | Dashboard log | `[timestamp] RSSI: -XX \| Packet #: N` + updated stats |

### E3. Press the switch multiple times

- Verify `packet_count` increments on each press
- Verify RSSI values update (they may vary slightly)
- Verify all packets appear on all 3 laptops with no drops

---

## Stage F: Common Failure Points & Debugging

### 1. Broker not reachable from other laptops

**Symptoms:** `Connection refused` or timeout when connecting from Laptop A or C.

**Fixes:**
- Verify all laptops are on the same WiFi network
- Check Laptop B's firewall (see Laptop B README)
- Verify Laptop B's IP: `ipconfig getifaddr en0` (macOS) or `hostname -I` (Linux)
- Try `ping <BROKER_IP>` from the other laptop
- Ensure Mosquitto is running (not crashed)

### 2. WebSocket port not open (Dashboard can't connect)

**Symptoms:** Dashboard status stays red, browser console shows WebSocket connection error.

**Fixes:**
- Verify `mosquitto.conf` has the `listener 9001` + `protocol websockets` block
- Verify you restarted Mosquitto after editing the config
- Check firewall for port 9001 specifically
- Try from Laptop B itself: open `dashboard.html` with broker IP as `localhost`

### 3. Wrong MQTT topic name

**Symptoms:** Messages are published but not received by subscribers/dashboard.

**Fixes:**
- Topic must be exactly `BLEReceiver/001` (case-sensitive)
- Check the subscriber/dashboard is subscribed to the same topic
- Use `mosquitto_sub -h <BROKER_IP> -t "#"` to subscribe to ALL topics and verify what's actually being published

### 4. Wrong serial port name

**Symptoms:** `[SERIAL] ERROR: Could not open port`

**Fixes:**
- Re-check port name (see Stage B1)
- Close Arduino IDE's Serial Monitor — only one program can use the port at a time
- On Linux, you may need `sudo usermod -a -G dialout $USER` and re-login, or run with `sudo`
- Try unplugging and re-plugging the Arduino

### 5. Serial port open but no data

**Symptoms:** Bridge is running but no lines appear, even when pressing the switch.

**Fixes:**
- The Arduino may be stuck in the WiFi/MQTT connection loop (it has `while` loops that block)
- Since the Arduino's `ssid`, `pass`, and `mqtt_server` are empty/placeholder, **the Arduino will hang at startup** trying to connect to WiFi
- **This is the key insight:** The Arduino sketch as-is will block at `while (wifi_status != WL_CONNECTED)` in `setup()` because the credentials are empty
- To work around this: either fill in the WiFi credentials in the sketch (but you said not to modify it), or use a separate test sketch that just outputs dummy data over Serial for demo purposes

> **Important:** If the Arduino code has empty WiFi credentials, it will not reach the BLE scanning loop. You'll need to either:
> (a) Fill in valid WiFi credentials and the broker IP in the .ino file, or
> (b) For demo/testing purposes, use the smoke test script below to simulate Serial output

### 6. Firewall blocking on mobile hotspot

**Symptoms:** Ping works but MQTT connections fail.

**Fixes:**
- Some mobile hotspots have "AP isolation" enabled, which prevents devices from communicating with each other
- On Android: check hotspot settings, disable "client isolation" if available
- On iPhone: this is usually not an issue
- Alternative: use a dedicated WiFi router instead of a phone hotspot

---

## Stage G: What "Working" Looks Like vs "Broken"

### ✅ Working — Normal Output

**Laptop B (broker, verbose):**
```
1693123460: New connection from 192.168.43.101:52341 on port 1883.
1693123460: New client connected from 192.168.43.101:52341 as LaptopA_SerialBridge (p2, c1, k60).
1693123465: New connection from 192.168.43.102:49821 on port 9001.
1693123465: New client connected from 192.168.43.102:49821 as LaptopC_Dashboard_a1b2c3 (p2, c1, k60).
1693123470: Received PUBLISH from LaptopA_SerialBridge (d0, q0, r0, m0, 'BLEReceiver/001', ... (35 bytes))
1693123470: Sending PUBLISH to LaptopC_Dashboard_a1b2c3 (d0, q0, r0, m0, 'BLEReceiver/001', ... (35 bytes))
```

**Laptop A (bridge):**
```
[15:30:01] Discovered target peripheral
[15:30:01] Packet Count: 1
[15:30:01] RSSI: -45
[15:30:01] Payload:{"rssi":-45,"packet_count":1}
[MQTT] Published to BLEReceiver/001: {"rssi":-45,"packet_count":1}
[15:30:01] Data Sent
```

**Laptop C (dashboard):**
- Green status dot
- Log entry: `[15:30:01] RSSI: -45 | Packet #: 1 | Raw: {"rssi":-45,"packet_count":1}`
- Stats: Packets received: 1, Last RSSI: -45, Last packet #: 1

### ❌ Broken — Common Bad States

| Symptom | Likely Cause |
|---------|-------------|
| Bridge prints Serial lines but no `[MQTT] Published` | MQTT not connected — check broker IP |
| Bridge shows nothing at all | Arduino stuck in WiFi connect loop (empty credentials) or wrong serial port |
| Subscriber logger shows nothing | Wrong topic, or publisher isn't reaching broker |
| Dashboard stays red | WebSocket port 9001 blocked, broker not running, or wrong IP |
| Dashboard connects (green) but no messages | Topic mismatch, or nothing is being published |
| Everything works once then stops | Arduino BLE scan restart issue — normal, just press switch again |
| Broker shows "Socket error" | Client disconnected unexpectedly — usually harmless, client will reconnect |

---

## Quick Smoke Test Script

If you want a fast way to test the broker + dashboard without the Arduino, run this on any laptop:

```bash
# Publishes a fake packet every 3 seconds
python -c "
import paho.mqtt.client as mqtt, time, json, random
c = mqtt.Client()
c.connect('<BROKER_IP>')
i = 0
while True:
    i += 1
    payload = json.dumps({'rssi': random.randint(-80, -30), 'packet_count': i})
    c.publish('BLEReceiver/001', payload)
    print(f'Published: {payload}')
    time.sleep(3)
"
```

This will send fake data to the broker, and you should see it appear on both Laptop B's subscriber and Laptop C's dashboard.
