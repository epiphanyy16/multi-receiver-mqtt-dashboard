import json
import logging
import os
import threading
import uuid
from collections import deque
from datetime import datetime, timezone

import paho.mqtt.client as mqtt
from flask import Flask, Response, jsonify, render_template, request, stream_with_context


logging.basicConfig(
    level=os.getenv("LOG_LEVEL", "INFO"),
    format="%(asctime)s %(levelname)s %(message)s",
)
LOGGER = logging.getLogger("ble-dashboard")


class EventStore:
    """Thread-safe, ordered packet history used by MQTT and SSE threads."""

    def __init__(self, max_events=20_000):
        self._events = deque(maxlen=max_events)
        self._next_id = 1
        self._condition = threading.Condition()

    def append(self, event):
        with self._condition:
            stored_event = dict(event)
            stored_event["event_id"] = self._next_id
            self._next_id += 1
            self._events.append(stored_event)
            self._condition.notify_all()
            return dict(stored_event)

    def after(self, event_id):
        with self._condition:
            return [dict(event) for event in self._events if event["event_id"] > event_id]

    def wait_after(self, event_id, timeout=15):
        with self._condition:
            events = [event for event in self._events if event["event_id"] > event_id]
            if not events:
                self._condition.wait(timeout)
                events = [event for event in self._events if event["event_id"] > event_id]
            return [dict(event) for event in events]

    @property
    def total(self):
        with self._condition:
            return self._next_id - 1


def parse_packet(topic, raw_payload):
    receiver_from_topic = topic.rsplit("/", 1)[-1] if "/" in topic else topic
    event = {
        "received_at": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
        "receiver_id": receiver_from_topic,
        "rssi": None,
        "packet_count": None,
        "topic": topic,
        "raw_payload": raw_payload,
        "valid": False,
        "error": None,
    }

    try:
        payload = json.loads(raw_payload)
        if not isinstance(payload, dict):
            raise ValueError("payload must be a JSON object")

        rssi = payload.get("rssi")
        packet_count = payload.get("packet_count")
        if not isinstance(rssi, int) or isinstance(rssi, bool):
            raise ValueError("rssi must be an integer")
        if not isinstance(packet_count, int) or isinstance(packet_count, bool):
            raise ValueError("packet_count must be an integer")

        payload_receiver = str(payload.get("receiver_id", receiver_from_topic))
        if payload_receiver != receiver_from_topic:
            raise ValueError("receiver_id does not match the MQTT topic")

        event.update(
            receiver_id=receiver_from_topic,
            rssi=rssi,
            packet_count=packet_count,
            valid=True,
        )
    except (UnicodeError, json.JSONDecodeError, ValueError) as exc:
        event["error"] = str(exc)

    return event


def create_app(start_mqtt=True):
    app = Flask(__name__)
    store = EventStore(max_events=int(os.getenv("EVENT_HISTORY_SIZE", "20000")))
    mqtt_state = {"connected": False, "reason": "not started"}
    mqtt_state_lock = threading.Lock()

    @app.get("/")
    def index():
        return render_template("index.html")

    @app.get("/api/events")
    def get_events():
        try:
            after_id = max(0, int(request.args.get("after", "0")))
        except ValueError:
            return jsonify({"error": "after must be an integer"}), 400
        return jsonify({"events": store.after(after_id), "total": store.total})

    @app.get("/api/status")
    def status():
        with mqtt_state_lock:
            state = dict(mqtt_state)
        state["packets_received"] = store.total
        return jsonify(state)

    @app.get("/events")
    def event_stream():
        try:
            cursor = max(0, int(request.headers.get("Last-Event-ID", "0")))
        except ValueError:
            cursor = 0

        @stream_with_context
        def generate():
            nonlocal cursor
            while True:
                events = store.wait_after(cursor)
                if not events:
                    yield ": keepalive\n\n"
                    continue
                for event in events:
                    cursor = event["event_id"]
                    yield (
                        f"id: {cursor}\n"
                        f"data: {json.dumps(event, separators=(',', ':'))}\n\n"
                    )

        return Response(
            generate(),
            mimetype="text/event-stream",
            headers={
                "Cache-Control": "no-cache",
                "X-Accel-Buffering": "no",
            },
        )

    if start_mqtt:
        broker_host = os.getenv("MQTT_HOST", "127.0.0.1") or "127.0.0.1"
        broker_port = int(os.getenv("MQTT_PORT", "1883") or "1883")
        topic_filter = os.getenv("MQTT_TOPIC", "BLEReceiver/+")
        client_id = f"BLE-dashboard-{uuid.uuid4().hex[:10]}"

        mqtt_client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=client_id,
            protocol=mqtt.MQTTv311,
        )
        mqtt_client.reconnect_delay_set(min_delay=1, max_delay=30)

        def on_connect(client, userdata, flags, reason_code, properties):
            connected = reason_code == 0
            with mqtt_state_lock:
                mqtt_state["connected"] = connected
                mqtt_state["reason"] = str(reason_code)
            if connected:
                result, _ = client.subscribe(topic_filter, qos=1)
                if result == mqtt.MQTT_ERR_SUCCESS:
                    LOGGER.info("Subscribed to mqtt://%s:%s/%s", broker_host, broker_port, topic_filter)
                else:
                    LOGGER.error("MQTT subscribe failed with code %s", result)
            else:
                LOGGER.error("MQTT connection rejected: %s", reason_code)

        def on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
            with mqtt_state_lock:
                mqtt_state["connected"] = False
                mqtt_state["reason"] = str(reason_code)
            LOGGER.warning("MQTT disconnected: %s", reason_code)

        def on_message(client, userdata, message):
            raw_payload = message.payload.decode("utf-8", errors="replace")
            event = parse_packet(message.topic, raw_payload)
            stored = store.append(event)
            LOGGER.info(
                "packet event_id=%s receiver=%s rssi=%s packet_count=%s topic=%s valid=%s payload=%s",
                stored["event_id"],
                stored["receiver_id"],
                stored["rssi"],
                stored["packet_count"],
                stored["topic"],
                stored["valid"],
                stored["raw_payload"],
            )

        mqtt_client.on_connect = on_connect
        mqtt_client.on_disconnect = on_disconnect
        mqtt_client.on_message = on_message

        with mqtt_state_lock:
            mqtt_state["reason"] = f"connecting to {broker_host}:{broker_port}"
        mqtt_client.connect_async(broker_host, broker_port, keepalive=60)
        mqtt_client.loop_start()

        app.extensions["mqtt_client"] = mqtt_client

    app.extensions["event_store"] = store
    app.extensions["mqtt_state"] = mqtt_state
    return app


if __name__ == "__main__":
    app = create_app()
    app.run(host="0.0.0.0", port=int(os.getenv("PORT", "5000")), threaded=True)
