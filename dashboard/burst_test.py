#!/usr/bin/env python3
"""Publish a two-receiver burst and confirm every packet reaches the dashboard."""

import argparse
import json
import sys
import time
import uuid
from urllib.request import urlopen

import paho.mqtt.client as mqtt


def get_json(url):
    with urlopen(url, timeout=5) as response:
        return json.load(response)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--broker", required=True, help="MQTT broker host/IP")
    parser.add_argument("--dashboard", default="http://127.0.0.1:5000")
    parser.add_argument("--count", type=int, default=100, help="Total packets to publish")
    parser.add_argument("--timeout", type=float, default=15)
    args = parser.parse_args()

    if args.count < 1:
        parser.error("--count must be positive")

    dashboard_url = args.dashboard.rstrip("/")
    status = get_json(f"{dashboard_url}/api/status")
    if not status.get("connected"):
        print(f"Dashboard is not connected to MQTT: {status.get('reason')}", file=sys.stderr)
        return 2

    baseline = int(status["packets_received"])
    run_id = uuid.uuid4().hex
    client = mqtt.Client(
        callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"BLE-burst-test-{run_id[:10]}",
        protocol=mqtt.MQTTv311,
    )
    client.connect(args.broker, 1883, keepalive=30)
    client.loop_start()

    expected_sequences = set(range(1, args.count + 1))
    try:
        for sequence in expected_sequences:
            receiver = "001" if sequence % 2 else "002"
            payload = json.dumps(
                {
                    "receiver_id": receiver,
                    "rssi": -40 - (sequence % 50),
                    "packet_count": (sequence + 1) // 2,
                    "test_run": run_id,
                    "test_sequence": sequence,
                },
                separators=(",", ":"),
            )
            result = client.publish(f"BLEReceiver/{receiver}", payload, qos=1)
            result.wait_for_publish(timeout=5)
            if result.rc != mqtt.MQTT_ERR_SUCCESS:
                raise RuntimeError(f"publish {sequence} failed with MQTT code {result.rc}")
    finally:
        client.disconnect()
        client.loop_stop()

    deadline = time.monotonic() + args.timeout
    received_sequences = []
    while time.monotonic() < deadline:
        body = get_json(f"{dashboard_url}/api/events?after={baseline}")
        received_sequences = []
        for event in body["events"]:
            try:
                payload = json.loads(event["raw_payload"])
            except json.JSONDecodeError:
                continue
            if payload.get("test_run") == run_id:
                received_sequences.append(payload.get("test_sequence"))
        if expected_sequences.issubset(received_sequences):
            break
        time.sleep(0.25)

    missing = sorted(expected_sequences - set(received_sequences))
    duplicates = len(received_sequences) - len(set(received_sequences))
    print(f"Sent: {args.count}")
    print(f"Displayed by dashboard API: {len(received_sequences)}")
    print(f"Missing test sequences: {missing}")
    print(f"Duplicate deliveries: {duplicates}")

    if missing:
        return 1
    print("PASS: every published test packet reached the dashboard.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
