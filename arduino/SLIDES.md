# Arduino Multi-Receiver System

- Two Arduino Nano 33 IoT receivers
- Both detect the same BLE switch
- Each receiver sends its observations independently

---

# Overall Architecture

```text
                       ┌─ Arduino 001 ─┐
BLE switch ────────────┤               ├─ MQTT broker ─ Dashboard
                       └─ Arduino 002 ─┘
```

- BLE connects the switch to the Arduinos
- Wi-Fi connects the Arduinos to MQTT

---

# Multi-Receiver Logic

- Both Arduinos scan for the same BLE address
- Each Arduino has a unique receiver ID
- Each measures the signal strength from its location
- The receivers do not communicate with each other

---

# MQTT Communication

- Receiver 001 publishes to `BLEReceiver/001`
- Receiver 002 publishes to `BLEReceiver/002`
- Each message contains:
  - Receiver ID
  - Signal strength (RSSI)
  - Packet count
- The dashboard listens to both topics

---

# LED Indicators

- **Built-in LED:** Wi-Fi connected
- **D2 LED:** MQTT connected
- **D3 LED:** BLE activity
  - One blink: MQTT publish failed
  - Three blinks: MQTT publish succeeded

---

# Complete Flow

1. The BLE switch sends an advertisement
2. Both Arduinos try to detect it
3. Each Arduino measures RSSI
4. The result is published through MQTT
5. The dashboard displays messages from both receivers

