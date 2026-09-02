# Multi-Receiver BLE/MQTT Test Guide

This setup uses four laptops:

| Machine | Role |
|---|---|
| Laptop 1 | Arduino Nano 33 IoT receiver `001` and Serial Monitor |
| Laptop 2 | Arduino Nano 33 IoT receiver `002` and Serial Monitor |
| Laptop 3 | Mosquitto MQTT broker |
| Laptop 4 | Web dashboard |

The two Arduinos detect the same ID-card switch independently. Every detection is published on a receiver-specific topic, so a common BLE advertisement can appear twice on the dashboard with different RSSI values.

## 1. Prerequisites

All four laptops, both Nano 33 IoT boards, and the broker must be able to reach the same local network. A phone hotspot is sufficient only if it permits devices on the hotspot to communicate with one another. Some hotspots enable client isolation; use a normal WiFi router or a laptop hotspot if peer-to-peer connections are blocked.

Install:

- Laptop 1 and 2: Arduino IDE 2.x
- Laptop 3 and 4: Docker Engine with the Docker Compose plugin
- Arduino libraries: `WiFiNINA`, `ArduinoBLE`, and `PubSubClient`
- Nano 33 IoT NINA-W102 firmware 3.0.0 or later

The broker permits anonymous MQTT connections for this isolated prototype. Do not expose port 1883 to the internet or use this configuration on an untrusted network.

## 2. Connect the network and find laptop 3's IP

1. Start the hotspot.
2. Connect laptops 1-4 to it.
3. On laptop 3, find its hotspot IPv4 address:

   ```bash
   hostname -I
   ```

   If several addresses are shown, inspect them with:

   ```bash
   ip -4 address
   ```

   Use the address belonging to the WiFi/hotspot interface. It may look like `192.168.1.5`, `192.168.137.1`, `10.42.0.1`, or `172.20.10.2`.

4. From laptop 4, confirm laptop 3 is reachable:

   ```bash
   ping -c 4 LAPTOP_3_IP
   ```

Keep laptop 3 on the same IP during the test. If the hotspot assigns it a new address, update both Arduino sketches and the dashboard `.env` file.

## 3. Configure and flash the Arduinos

The supplied `ESW_BLE_Code_1.ino` remains unchanged. Use the two deployment copies:

- Laptop 1: `arduino/receiver_001/receiver_001.ino`
- Laptop 2: `arduino/receiver_002/receiver_002.ino`

At the top of each sketch, set:

```cpp
char ssid[] = "YOUR_HOTSPOT_NAME";
char pass[] = "YOUR_HOTSPOT_PASSWORD";
const char* targetAddress = "E2:15:00:0A:72:43";
IPAddress mqttServer(192, 168, 1, 5);
```

Replace the SSID and password, verify the ID-card switch MAC address, and write laptop 3's IPv4 address as four comma-separated numbers. Do not give both boards the same receiver sketch: their unique MQTT client IDs prevent the broker from disconnecting one when the other connects.

For each receiver:

1. Connect the Nano 33 IoT over USB.
2. In Arduino IDE, select **Arduino Nano 33 IoT** and the correct serial port.
3. Install the three libraries listed above through Library Manager.
4. Update the NINA firmware with the Arduino IDE firmware updater if the sketch reports a version below 3.0.0.
5. Upload the appropriate receiver sketch.
6. Open Serial Monitor at **115200 baud**. The sketch waits for the serial connection before continuing.

A successful startup contains:

```text
Connected to WiFi
Connected as BLEReceiver001
BLE receiver 001 scanning for E2:15:00:0A:72:43
```

Receiver 002 prints the corresponding `002` values.

## 4. Start the broker on laptop 3

Copy this project, or at least the `broker` folder, to laptop 3. From that folder run:

```bash
cd broker
docker compose up -d
docker compose ps
docker compose logs -f mosquitto
```

The status should become `healthy`, and the logs should show connections from `BLEReceiver001`, `BLEReceiver002`, and later the dashboard.

If laptop 3 has an active UFW firewall, allow MQTT from the local network:

```bash
sudo ufw allow 1883/tcp
```

To observe receiver messages directly at the broker:

```bash
docker compose exec mosquitto mosquitto_sub -v -t 'BLEReceiver/+'
```

Leave this optional subscriber running during early tests.

## 5. Start the dashboard on laptop 4

Copy the `dashboard` folder to laptop 4, then run:

```bash
cd dashboard
cp .env.example .env
```

Edit `.env` and set `MQTT_HOST` to laptop 3's hotspot IPv4 address:

```dotenv
MQTT_HOST=192.168.1.5
MQTT_PORT=1883
EVENT_HISTORY_SIZE=20000
```

Start the dashboard:

```bash
docker compose up -d --build
docker compose logs -f dashboard
```

Open <http://localhost:5000> on laptop 4. The header must show `MQTT: connected`. Every MQTT callback is logged in the container console and added as a separate table row; repeated payloads and repeated RSSI values are not deduplicated.

The columns are:

- Dashboard event: dashboard-wide arrival sequence
- Received: UTC time at laptop 4
- Arduino: `001` or `002`, derived from the MQTT topic
- RSSI: signal strength measured by that Arduino
- Arduino packet count: that receiver's independent counter
- Topic and raw JSON payload

## 6. Test before using the ID card

With the broker and dashboard running, execute this from the `dashboard` folder on laptop 4:

```bash
docker compose exec dashboard sh -c \
  'python burst_test.py --broker "$MQTT_HOST" --dashboard http://127.0.0.1:5000 --count 500'
```

Expected result:

```text
Sent: 500
Displayed by dashboard API: 500
Missing test sequences: []
Duplicate deliveries: 0
PASS: every published test packet reached the dashboard.
```

This checks the broker, MQTT subscription, concurrent receiver topics, dashboard event history, and dashboard API. It does not test BLE radio reception.

## 7. ID-card switch tests

### One receiver at a time

1. Keep receiver 002 powered off.
2. Press the ID-card switch near receiver 001.
3. Confirm laptop 1's Serial Monitor prints one or more blocks like:

   ```text
   Discovered target peripheral
   Receiver ID: 001
   Packet Count: 1
   RSSI: -61
   Payload: {"receiver_id":"001","rssi":-61,"packet_count":1}
   Data Sent
   ```

4. Confirm each `Data Sent` block produces a receiver `001` row on the dashboard.
5. Repeat with only receiver 002 powered on.

### Both receivers

1. Place both receivers where required for the experiment.
2. Keep both Serial Monitors open and confirm both MQTT connections are established.
3. Note each Arduino's current packet count and the dashboard's current final event number.
4. Press the ID-card switch once.
5. Check both Serial Monitors and then the dashboard.
6. Rows from `001` and `002` should be distinct even when they came from the same switch press. Their RSSI and packet counts need not match.
7. Repeat at several distances and floors, recording receiver placement.

The energy-harvesting switch may transmit a short burst containing multiple BLE advertisements. One press can therefore increment a receiver's counter more than once. Conversely, radio conditions can cause one receiver to detect fewer advertisements than the other. The useful reconciliation is:

- For each Arduino, count new `Data Sent` lines after a test starts.
- Filter dashboard rows by that Arduino ID.
- The increase in dashboard rows should equal that Arduino's new `Data Sent` count while all network components remain connected.

## 8. Delivery limitations

The Arduino `PubSubClient` publish call sends at MQTT QoS 0. This keeps the sketches close to the supplied code but means there is no end-to-end retransmission if WiFi drops at the instant of publication. `Data Sent` confirms that the local MQTT client accepted the publish; it cannot prove that laptop 4 displayed it.

For reliable test results:

- Start the broker and dashboard before pressing the switch.
- Keep all laptops awake and connected to the hotspot.
- Watch for `MQTT publish failed`, reconnect messages, or a red dashboard connection status.
- Use the burst test before each experimental session.

The dashboard retains the latest 20,000 events in memory. Browser stream reconnections use event IDs to replay retained events. Restarting the dashboard clears this history, and non-retained Arduino messages sent while it is stopped cannot be recovered. Increase `EVENT_HISTORY_SIZE` if a run may exceed 20,000 packets.

## 9. Troubleshooting

### Only one Arduino appears

- Verify laptop 1 has the `receiver_001` sketch and laptop 2 has `receiver_002`.
- Check the startup lines for client IDs `BLEReceiver001` and `BLEReceiver002`.
- In broker logs, look for repeated disconnect/reconnect cycles. Those normally indicate duplicate MQTT client IDs.
- Run the broker-side `mosquitto_sub` command to distinguish publishing failure from dashboard failure.

### Dashboard says disconnected

- Verify `MQTT_HOST` is laptop 3's current hotspot IP, not `localhost`.
- Confirm `docker compose ps` reports the broker healthy.
- Test `ping LAPTOP_3_IP` from laptop 4.
- Check laptop 3's firewall and hotspot client-isolation settings.
- Restart after correcting the address:

  ```bash
  docker compose up -d --force-recreate
  ```

### Arduino cannot connect to MQTT

- Ensure its `mqttServer` matches laptop 3's address.
- Confirm the board joined the same hotspot.
- Confirm TCP port 1883 is allowed through laptop 3's firewall.
- Watch `docker compose logs -f mosquitto` on laptop 3 while resetting the board.

### Serial Monitor shows no BLE detection

- Verify the switch MAC in `targetAddress`.
- Confirm the switch is producing advertisements with a separate BLE scanner.
- Move the card close to the receiver for the first test.
- Verify ArduinoBLE initializes and NINA firmware is at least 3.0.0.
- Keep Serial Monitor open because the sketch waits at `while (!Serial)`.

### Serial shows packets but the dashboard misses rows

- Check whether every serial block ends in `Data Sent`; `MQTT publish failed` was not delivered.
- Check the broker subscriber and dashboard container logs at the same time.
- Run the 500-message burst test. If it passes, the broker/dashboard path is working and the issue is before or at Arduino publication.
- Confirm the dashboard header remains connected during the full test.

### Stop the services

On laptop 4:

```bash
cd dashboard
docker compose down
```

On laptop 3:

```bash
cd broker
docker compose down
```

Broker persistence and logs remain in Docker volumes. Use `docker compose down -v` only when you intentionally want to erase those volumes.
