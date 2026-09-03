#!/usr/bin/env python3
"""
MQTT Subscriber Logger (Laptop B)

Subscribes to the BLEReceiver/001 topic and prints every message
with a timestamp. Run this alongside the Mosquitto broker to see
incoming data in real time.

Usage:
    python subscriber_log.py
    python subscriber_log.py --broker 192.168.1.100
"""

import argparse
import time
import paho.mqtt.client as mqtt

MQTT_TOPIC = "BLEReceiver/001"


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[CONNECTED] Subscribed to topic: {MQTT_TOPIC}")
        client.subscribe(MQTT_TOPIC)
    else:
        print(f"[ERROR] Connection failed with code {rc}")


def on_message(client, userdata, msg):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    payload = msg.payload.decode("utf-8", errors="replace")
    print(f"[{timestamp}] Topic: {msg.topic}  |  Payload: {payload}")


def main():
    parser = argparse.ArgumentParser(description="MQTT subscriber logger")
    parser.add_argument(
        "--broker", default="localhost",
        help="MQTT broker address (default: localhost)"
    )
    parser.add_argument(
        "--port", type=int, default=1883,
        help="MQTT broker port (default: 1883)"
    )
    args = parser.parse_args()

    client = mqtt.Client(client_id="LaptopB_Logger")
    client.on_connect = on_connect
    client.on_message = on_message

    print(f"[INFO] Connecting to broker at {args.broker}:{args.port}...")
    client.connect(args.broker, args.port, keepalive=60)

    print("[INFO] Waiting for messages (Ctrl+C to quit)...\n")

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\n[INFO] Shutting down.")
        client.disconnect()


if __name__ == "__main__":
    main()
