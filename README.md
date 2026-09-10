# Multi-Receiver BLE → MQTT Dashboard

This project uses two Arduino Nano 33 IoT boards to detect advertisements from
the same BLE switch, publish each receiver's RSSI and packet count to MQTT, and
display every received event in a web dashboard.

## System at a glance

```text
BLE switch
   ├── Arduino receiver 001 ── BLEReceiver/001 ──┐
   └── Arduino receiver 002 ── BLEReceiver/002 ──┤
                                                  ▼
                                         Mosquitto broker
                                                  │
                                                  ▼
                                           Flask dashboard
```

| Item | Current value |
|---|---|
| Arduino board | Arduino Nano 33 IoT |
| Receiver sketches | `arduino/receiver_001/receiver_001.ino` and `arduino/receiver_002/receiver_002.ino` |
| Reference sketch | `ESW_BLE_Code_1.ino` |
| BLE target address | `E2:15:00:0A:72:43` |
| MQTT broker address in the sketches | `172.20.10.2:1883` |
| MQTT topics | `BLEReceiver/001`, `BLEReceiver/002` |
| Dashboard subscription | `BLEReceiver/+` |
| Dashboard URL | `http://localhost:5001` |
| Wi-Fi retry interval | 10 seconds |
| BLE scan restart interval | 400 ms, with a 100 ms pause |
| MQTT delivery level | QoS 0 from the Arduinos |

The Wi-Fi name and password in Git are placeholders. Set real credentials
locally before flashing, but do not commit them.

## LED behaviour

Both deployment sketches use three LEDs:

| LED | Pin | Meaning |
|---|---|---|
| Wi-Fi | `LED_BUILTIN` | On while Wi-Fi is connected |
| MQTT | D2 | On while both Wi-Fi and MQTT are connected |
| Activity | D3 | One pulse when BLE is detected but publish fails; three pulses after a successful publish |

Wire each external LED as `pin → resistor → LED anode`, with the LED cathode
connected to GND. The reference `ESW_BLE_Code_1.ino` does not contain LED
logic; LEDs are an addition in the two deployment sketches.

## Wi-Fi and reconnection logic

The deployment sketches preserve the reference sketch's connection flow:

1. `wifi_reconnect()` calls `WiFi.begin(ssid, pass)`.
2. Startup retries every 10 seconds until `WiFi.status()` is
   `WL_CONNECTED`.
3. Before reconnecting MQTT, the sketch checks Wi-Fi and restores it first.
4. MQTT retries every 5 seconds until connected.
5. The LED additions only report connection state; they do not replace the
   Wi-Fi logic.

The Nano 33 IoT needs a 2.4 GHz, WPA2-compatible network. It cannot join a
5 GHz-only network. NINA-W102 firmware 3.0.0 or later is required to run Wi-Fi
and BLE together.

## MQTT event format

Each receiver has a unique MQTT client ID, topic, and payload receiver ID:

```json
{
  "receiver_id": "001",
  "rssi": -61,
  "packet_count": 42
}
```

Receiver 002 sends the same format with `"receiver_id": "002"`. The dashboard
marks a payload invalid if its `receiver_id` does not match the receiver suffix
in its MQTT topic.

## Repository layout

| Path | Purpose |
|---|---|
| `arduino/receiver_001/receiver_001.ino` | Deployment sketch for receiver 001 |
| `arduino/receiver_002/receiver_002.ino` | Deployment sketch for receiver 002 |
| `arduino/connection_test/connection_test.ino` | Standalone LED wiring test |
| `ESW_BLE_Code_1.ino` | Supplied/reference single-receiver sketch |
| `broker/` | Mosquitto configuration and Docker Compose service |
| `dashboard/` | Flask dashboard, tests, and Docker Compose service |
| `TESTING_GUIDE.md` | Detailed four-laptop setup, testing, and troubleshooting |
| `broshisis/` | Earlier/alternative prototype files; not the primary deployment |
| `task1/` | Earlier task sketch; not the primary deployment |

## Configure the Arduino receivers

Install these Arduino libraries:

- `WiFiNINA`
- `ArduinoBLE`
- `PubSubClient`

In both deployment sketches, update:

```cpp
char ssid[] = "YOUR_HOTSPOT_NAME";
char pass[] = "YOUR_HOTSPOT_PASSWORD";
const char* targetAddress = "E2:15:00:0A:72:43";
IPAddress mqtt_server(172, 20, 10, 2);
```

Use the hotspot/local-network IPv4 address of the laptop running Mosquitto for
`mqtt_server`. Flash receiver 001 with its `001` sketch and receiver 002 with
its `002` sketch. Open Serial Monitor at 115200 baud; each sketch waits for the
serial connection during startup.

## Test the LED wiring

Upload `arduino/connection_test/connection_test.ino`. It requires no Wi-Fi,
MQTT, or BLE configuration and automatically repeats this sequence:

1. Built-in/D13 LED
2. External D2 MQTT LED
3. External D3 activity LED
4. All three LEDs together

Each step remains on for 1.5 seconds. Open Serial Monitor at 115200 baud for
labels while the sequence runs. If an external LED does not light, check its
polarity, resistor, pin connection, and GND connection.

## Run the MQTT broker

On the broker laptop:

```bash
cd broker
docker compose up -d
docker compose ps
docker compose logs -f mosquitto
```

The broker listens on TCP port 1883. Anonymous access is enabled for a trusted,
isolated classroom network only; do not expose it to the internet.

To inspect all receiver messages:

```bash
docker compose exec mosquitto mosquitto_sub -v -t 'BLEReceiver/+'
```

## Run the dashboard

On the dashboard laptop:

```bash
cd dashboard
cp .env.example .env
```

Set `MQTT_HOST` in `.env` to the broker laptop's IPv4 address, then run:

```bash
docker compose up -d --build
docker compose logs -f dashboard
```

Open <http://localhost:5001>. The dashboard keeps the newest 20,000 events in
memory by default, so restarting its container clears displayed history.

## Verify the full path

From the `dashboard` directory:

```bash
docker compose exec dashboard sh -c \
  'python burst_test.py --broker "$MQTT_HOST" --dashboard http://127.0.0.1:5000 --count 500'
```

A passing burst test verifies the broker-to-dashboard path. It does not test
BLE reception or Arduino-to-broker delivery.

## Important limitations

- Arduino publishes use MQTT QoS 0, so packets can be lost during a connection
  outage and are not retried.
- One physical switch press may produce multiple BLE advertisements, so it can
  create multiple rows.
- The two receivers detect independently; their RSSI and packet counts do not
  need to match.
- The broker IP is compiled into each sketch and must be updated if the
  network assigns a different address.

For the complete four-laptop procedure and troubleshooting steps, see
[`TESTING_GUIDE.md`](TESTING_GUIDE.md).
