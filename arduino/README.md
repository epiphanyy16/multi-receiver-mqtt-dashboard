# Arduino receiver logic

This folder holds the Nano 33 IoT sketches that listen for one energy-harvesting BLE switch, publish each sighting over MQTT, and show connection/activity on three LEDs.

The two deployment boards run the same program with different identities. They do not talk to each other. Each independently scans, counts packets, and publishes to its own topic.

```text
BLE switch (MAC E2:15:00:0A:72:43)
        │
        ├── Arduino 001  →  MQTT topic BLEReceiver/001
        └── Arduino 002  →  MQTT topic BLEReceiver/002
                                    │
                                    ▼
                             Mosquitto :1883
                                    │
                                    ▼
                              Flask dashboard
```

The boards never pair with the switch and never read GATT characteristics. They only scan advertisements. One physical press can produce several advertisements, so one press can produce several MQTT messages.

## Files

| Path | Role |
|---|---|
| `receiver_001/receiver_001.ino` | Default sketch for board 001. Wi-Fi: `WiFi.begin`, then wait 10 s. |
| `receiver_002/receiver_002.ino` | Same as 001 except IDs/topic. |
| `dotted_wifi/receiver_001/receiver_001.ino` | Same BLE/MQTT/LED logic; Wi-Fi prints `.` every 500 ms. |
| `dotted_wifi/receiver_002/receiver_002.ino` | Dotted Wi-Fi variant for board 002. |
| `connection_test/connection_test.ino` | LED wiring test only. No Wi-Fi, MQTT, or BLE. |

Flash **one** receiver sketch per board. Do not run a default sketch and a dotted-wifi sketch on the same board at the same time.

## Libraries and hardware

- Board: Arduino Nano 33 IoT (NINA-W102).
- Libraries: `WiFiNINA`, `ArduinoBLE`, `PubSubClient`.
- Network: 2.4 GHz WPA2. 5 GHz-only networks will not join.
- Firmware: NINA **3.0.0 or later** is required so Wi-Fi and BLE can run together. Older firmware hangs in `setup()` after printing the version.

Set locally (do not commit real secrets):

```cpp
char ssid[] = "YOUR_HOTSPOT_NAME";
char pass[] = "YOUR_HOTSPOT_PASSWORD";
const char* targetAddress = "E2:15:00:0A:72:43";
IPAddress mqtt_server(172, 20, 10, 2);
```

`mqtt_server` must be the IPv4 address of the laptop running Mosquitto on the same LAN/hotspot. Serial Monitor is 115200 baud. Deployment sketches wait on `while (!Serial)` at boot, so open Serial or the board will sit there.

## Per-board identity

| Constant | Receiver 001 | Receiver 002 | Why |
|---|---|---|---|
| `receiverId` | `"001"` | `"002"` | Goes in the JSON payload. |
| `mqttClientId` | `"BLEReceiver001"` | `"BLEReceiver002"` | Must be unique. Two boards with the same client ID kick each other off the broker. |
| `mqttTopic` | `"BLEReceiver/001"` | `"BLEReceiver/002"` | Dashboard expects topic suffix to match `receiver_id`. |

MQTT port is **1883**. Publishes use **QoS 0** (PubSubClient default): fire-and-forget, no retry, no queue. If Wi-Fi or MQTT is down when an advertisement arrives, that event is lost.

---

## Hardware connections

Nothing is wired between the two Arduinos, the BLE switch, or the laptops. The switch is wireless (BLE advertisements). The Arduinos talk to the broker over Wi-Fi. The only extra physical wiring on each Nano 33 IoT is the two external LEDs.

Do this **identically on both boards**.

### Power and USB

| Connection | Notes |
|---|---|
| USB micro (Nano) → laptop/USB power | Powers the board and Serial Monitor. Required at boot because the deployment sketches wait on Serial. |
| VIN / barrel | Not used. |
| 5V / 3.3V rails | Not used for the LEDs. Drive LEDs from the GPIO pins (3.3 V logic). |

The Nano 33 IoT I/O is **3.3 V**. Do not feed 5 V into D2/D3.

### LED wiring (each Arduino)

Wi-Fi status uses the **on-board LED on D13**. No jumper for that one.

External LEDs (MQTT + activity), current-limiting resistor **220–330 Ω** each:

```text
                    MQTT LED
D2 ──► 220–330 Ω ──► LED anode ──► LED cathode ──► GND

                    Activity LED
D3 ──► 220–330 Ω ──► LED anode ──► LED cathode ──► GND

                    Wi-Fi LED (on board)
D13 / LED_BUILTIN  (already on the Nano; no extra parts)
```

| Signal | Arduino pin | Breadboard / part | Other end |
|---|---|---|---|
| Wi-Fi status | `LED_BUILTIN` (D13) | Built-in LED | — |
| MQTT status | D2 | Resistor then LED (long lead = anode) | LED short lead (cathode) to **GND** |
| Activity | D3 | Resistor then LED (long lead = anode) | LED short lead (cathode) to **GND** |
| Common ground | any **GND** pin | Shared GND rail | Both LED cathodes |

Polarity: anode (usually the longer lead) toward the pin/resistor; cathode toward GND. Reverse the LED and it stays dark.

Use one GND pin for both LEDs. Either of the Nano’s GND pins is fine.

### What is not connected

| Device | Physical link |
|---|---|
| Energy-harvesting BLE switch | None. It advertises; both Nanos scan MAC `E2:15:00:0A:72:43`. |
| Arduino 001 ↔ Arduino 002 | None. Independent scanners. |
| Arduino ↔ Mosquitto laptop | Wi-Fi only. Put the broker laptop IPv4 in `mqtt_server` (port **1883**). |
| Dashboard laptop | Wi-Fi to the same network; no Arduino wiring. |

Both Nanos must join the **same 2.4 GHz WPA2** hotspot/LAN as the broker.

### Pin map (Nano 33 IoT, this project)

```text
  D13  built-in LED     Wi-Fi connected
  D2   digital out      MQTT LED (external)
  D3   digital out      activity LED (external)
  GND  ground           both external LED cathodes
```

No I2C, SPI, UART (except USB Serial), analog, or interrupt pins are used.

### Check the wiring

Upload `connection_test/connection_test.ino`. It lights D13, then D2, then D3, then all three. If D13 works but D2/D3 do not, the jumper, resistor, polarity, or GND is wrong.

## LED meaning

External LEDs: `pin → resistor → LED anode`, cathode to GND.

| LED | Pin | Constant | Meaning |
|---|---|---|---|
| Wi-Fi | Built-in / D13 | `WIFI_LED_PIN` (`LED_BUILTIN`) | Solid on while `WiFi.status() == WL_CONNECTED`. |
| MQTT | D2 | `MQTT_LED_PIN` | Solid on only while **both** Wi-Fi and MQTT are connected. |
| Activity | D3 | `ACTIVITY_LED_PIN` | Blink when the target BLE advertisement is seen. |

Wi-Fi and MQTT LEDs are **status lamps**. They do not flash on publish. Activity is the only LED that encodes “something just happened.”

`HIGH` turns an LED on, `LOW` turns it off. All three start `LOW` in `setup()`.

### Activity blink patterns

`ACTIVITY_LEVELS` and `ACTIVITY_DURATIONS` are a 5-step sequence:

| Step | Level | Duration (ms) |
|---|---|---|
| 0 | HIGH | 120 |
| 1 | LOW | 180 |
| 2 | HIGH | 60 |
| 3 | LOW | 80 |
| 4 | HIGH | 60 |

Two patterns are derived from that table:

- **Heard BLE, MQTT publish failed:** stop after step 0. Result: one 120 ms flash, then off.
- **Heard BLE and publish succeeded:** run all five steps. Result: three flashes (on/off/on/off/on).

A failed publish still lights D3 so you can tell BLE is working even when the broker path is not.

The animation is **non-blocking**. It uses `millis()` instead of `delay()` so BLE scanning and `client.loop()` keep running while D3 blinks.

If another advertisement arrives while a pattern is running, it is **queued once** (not stacked). If any queued event published successfully, the follow-up pattern uses the three-flash version (`queuedActivityIncludesPublish` is OR’d).

---

## Globals (deployment sketches)

| Name | Type | Role |
|---|---|---|
| `ssid`, `pass` | `char[]` | Hotspot credentials. |
| `targetAddress` | `const char*` | BLE MAC to scan for. All other advertisers are ignored. |
| `receiverId` | `const char*` | JSON field. |
| `mqttClientId` | `const char*` | MQTT CONNECT client id. |
| `mqttTopic` | `const char*` | Publish topic. |
| `mqtt_server` | `IPAddress` | Broker IPv4. |
| `packetCounter` | `int` | Advertisements seen **on this board since boot**. Not shared with the other receiver. |
| `lastBLEScan` | `unsigned long` | `millis()` of last target sighting **or** last scan restart. Used to restart scanning every 400 ms of idle. |
| `wifi_status` | `int` | Last Wi-Fi status from `WiFi.begin` / `WiFi.status`. |
| `activityActive` | `bool` | D3 pattern currently running. |
| `activityIncludesPublish` | `bool` | Current pattern is the 3-flash (success) version. |
| `activityQueued` | `bool` | Another BLE event arrived during the current pattern. |
| `queuedActivityIncludesPublish` | `bool` | At least one queued event published. |
| `activityStep` | `byte` | Index into `ACTIVITY_LEVELS` / `ACTIVITY_DURATIONS`. |
| `nextActivityChange` | `unsigned long` | `millis()` when the current step should advance. |
| `wifiClient` | `WiFiClient` | TCP socket under MQTT. |
| `client` | `PubSubClient` | MQTT client wrapping `wifiClient`. |

---

## Functions (deployment sketches)

Receiver 001 and 002 share these functions. Only identity constants differ.

### `updateConnectionLEDs()`

Reads live link state and drives D13 and D2.

- Wi-Fi LED on iff `WiFi.status() == WL_CONNECTED`.
- MQTT LED on iff Wi-Fi is connected **and** `client.connected()`.

Called often from `setup()`, `loop()`, and reconnect helpers so a drop shows up without waiting for BLE. MQTT LED requires Wi-Fi because a stale MQTT “connected” flag is useless if the radio already left the AP.

### `beginActivityPattern(bool published)`

Starts (or queues) the D3 animation after a target advertisement.

- If a pattern is already running: set `activityQueued`, OR `published` into `queuedActivityIncludesPublish`, return. Does not interrupt the current blink.
- Otherwise: set `activityActive`, remember `published`, reset `activityStep` to 0, write `ACTIVITY_LEVELS[0]` (HIGH), schedule the first step end at `millis() + 120`.

`published` is `true` only when `client.publish(...)` returned success in `loop()`.

### `updateActivityLED()`

Advances the D3 state machine. Called every `loop()` iteration, and once more immediately after `beginActivityPattern` (usually a no-op because the first step has not expired).

1. If nothing is active, or `millis()` has not reached `nextActivityChange`, return.
2. Increment `activityStep`.
3. Stop and force D3 LOW if:
   - this is a fail pattern and `activityStep == 1`, or
   - `activityStep >= 5` (success pattern finished).
4. Otherwise write the next level and schedule the next duration.
5. If the pattern just finished and `activityQueued` is set, clear the queue flags and call `beginActivityPattern` with the queued publish result.

### `wifi_reconnect()` (default sketches)

Prints the SSID, calls `WiFi.begin(ssid, pass)`, stores the return value in `wifi_status`, then `updateConnectionLEDs()`.

The caller then waits 10 seconds before retrying. This variant relies on the status returned by `WiFi.begin()`; it does not separately poll `WiFi.status()` during that delay.

### `wifi_reconnect()` (dotted_wifi sketches)

Prints the SSID, calls `WiFi.begin`, then loops up to **40** times: `delay(500)` and print `.` (about 20 seconds). Then stores `WiFi.status()`, updates LEDs, prints the numeric status, and if still down prints that it will retry.

`mqtt_reconnect()` in this variant does not insert an extra 10 s delay around Wi-Fi; it just calls `wifi_reconnect()` until `WL_CONNECTED`.

### `mqtt_reconnect()`

Always restores Wi-Fi before MQTT, because MQTT is TCP over Wi-Fi.

**Default sketches:**

1. Sample `WiFi.status()`, update LEDs.
2. If Wi-Fi is down, loop: `wifi_reconnect()` then `delay(10000)` until connected.
3. While MQTT is down: force D2 LOW, `client.connect(mqttClientId)`.
   - Success: print, set D2 HIGH.
   - Failure: print `client.state()`, `delay(5000)`, retry.

**Dotted_wifi sketches:** same MQTT retry, but the Wi-Fi wait is `while (WiFi.status() != WL_CONNECTED) { wifi_reconnect(); }` using the dotted helper.

These loops **block**. While they run, BLE is not scanned and activity animation does not advance. Advertisements during an outage are missed (QoS 0, no buffer).

The Arduino does **not** subscribe to any topic. `connect` is publish-only.

### `setup()`

1. `pinMode` all three LED pins `OUTPUT`; write all LOW.
2. `Serial.begin(115200)` and `while (!Serial)`.
3. Dotted variant only: `WiFi.disconnect()` and `delay(1000)` so a previous association is cleared.
4. Read NINA firmware. If `< "3.0.0"`, print and `while (1)`.
5. `client.setServer(mqtt_server, 1883)`.
6. Until Wi-Fi is up: `wifi_reconnect()` (default also `delay(10000)`).
7. `updateConnectionLEDs()`.
8. `BLE.begin()`. On failure, print and `while (1)`.
9. `BLE.scanForAddress(targetAddress)`.

MQTT is **not** connected in `setup()`. The first `loop()` sees `!client.connected()` and calls `mqtt_reconnect()`.

### `loop()`

Order matters.

1. `updateActivityLED()` — advance D3 if a step expired.
2. `updateConnectionLEDs()` — refresh D13/D2.
3. If MQTT is down, `mqtt_reconnect()` (may block).
4. `client.loop()` — service the MQTT TCP session (keepalives). Required even with no subscriptions.
5. `updateConnectionLEDs()` again after MQTT work.
6. `BLE.available()`:
   - If the filtered peripheral is present:
     - Set `lastBLEScan = millis()`.
     - Increment `packetCounter`.
     - Read `peripheral.rssi()`.
     - Build JSON into a 96-byte buffer:
       `{"receiver_id":"...","rssi":-61,"packet_count":42}`
     - If still MQTT-connected, `client.publish(mqttTopic, payload)`.
     - `beginActivityPattern(published)` then `updateActivityLED()`.
7. If `millis() - lastBLEScan > 400`:
   - `BLE.stopScan()`
   - `delay(100)`
   - `BLE.scanForAddress(targetAddress)`
   - `lastBLEScan = millis()`

The 400 ms / 100 ms scan restart exists so the NINA radio does not stall in a half-dead scan. `lastBLEScan` is also updated on a real sighting, so a busy stream of ads delays the restart until 400 ms of quiet.

---

## End-to-end path for one advertisement

1. Switch advertises. ArduinoBLE matches `targetAddress`.
2. `packetCounter` increases on **this** board only.
3. RSSI is this board’s view of the switch (more negative = weaker / farther). The two receivers need not match.
4. JSON is printed on Serial and published to `BLEReceiver/00x` if MQTT is up.
5. D3 starts one flash (publish failed) or three flashes (publish succeeded).
6. Dashboard (elsewhere in the repo) subscribes to `BLEReceiver/+` and checks that JSON `receiver_id` matches the topic suffix.

---

## `connection_test` functions

Standalone sketch to prove LED wiring. No network.

| Function | Role |
|---|---|
| `setLEDs(wifi, mqtt, activity)` | Write all three pins at once. |
| `testLED(label, pin)` | Print `ON: <label>`, pin HIGH for 1500 ms, LOW, wait 500 ms. |
| `setup()` | Configure pins, all off, Serial 115200, 1.5 s delay, print banner. Does **not** wait on Serial forever. |
| `loop()` | Cycle: D13, D2, D3, then all three together 1500 ms, all off, wait 3000 ms, repeat. |

If an external LED never lights, check polarity, resistor, pin, and GND. The built-in LED on D13 should still work.

---

## Why the sketch is built this way

- **Two receivers, one MAC** — compare RSSI and which board heard a press.
- **Scan by address** — ignore classroom BLE noise.
- **Periodic scan restart** — keep the NINA scanner alive.
- **Wi-Fi before MQTT** — MQTT cannot recover on a dead association.
- **Blocking reconnects** — simple for a demo; the cost is missed ads while reconnecting.
- **LEDs as a field debug UI** — dark D13 = Wi-Fi, dark D2 = broker/IP/MQTT, one D3 blink = BLE heard but publish failed, three blinks = full path to the broker.
- **Non-blocking activity LED** — blinking must not stall `client.loop()` or BLE.
- **QoS 0** — lowest overhead; no local store-and-forward on the Arduino.

## Practical limits

- Opening Serial is required for the deployment sketches to leave `setup()`.
- `delay()` inside reconnect and scan restart pauses the rest of `loop()`.
- `packetCounter` eventually wraps at the Nano 33 IoT's 32-bit `int` limit; the dashboard treats it as a per-receiver counter, not a global sequence.
- Broker IP is compiled in. A new DHCP address requires a reflash.
- Anonymous MQTT is for an isolated classroom network only.
