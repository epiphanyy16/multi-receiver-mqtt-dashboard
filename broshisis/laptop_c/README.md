# Laptop C — Live Dashboard

## What This Does
A single HTML file that connects to the MQTT broker via WebSockets and displays incoming BLE emergency switch data in real time.

## Prerequisites
- A modern web browser (Chrome, Firefox, Edge, Safari)
- That's it — no install, no build step

## Usage

1. Open `dashboard.html` in your browser (just double-click it, or `File → Open`)
2. Enter Laptop B's IP address in the "Broker IP" field
3. Click **Connect**
4. The status dot turns green when connected
5. Incoming packets will appear in the log with timestamps

## What You Should See
When the BLE switch is pressed and the full pipeline is working:
```
[15:30:01] RSSI: -45 | Packet #: 1 | Raw: {"rssi":-45,"packet_count":1}
[15:30:05] RSSI: -42 | Packet #: 2 | Raw: {"rssi":-42,"packet_count":2}
```

## Notes
- The dashboard loads MQTT.js from a CDN (`unpkg.com`), so the laptop needs internet access at least once to load the page. After that, the page can work offline as long as the script is cached.
- If you're fully offline, download `https://unpkg.com/mqtt@5/dist/mqtt.min.js` and save it next to `dashboard.html`, then update the `<script src="...">` to point to the local file.
- The WebSocket port is 9001 (matching the Mosquitto config on Laptop B).
