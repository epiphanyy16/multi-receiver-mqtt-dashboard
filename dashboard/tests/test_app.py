import json

from app import EventStore, create_app, parse_packet


def test_parse_valid_packet_uses_receiver_topic():
    event = parse_packet(
        "BLEReceiver/002",
        '{"receiver_id":"002","rssi":-71,"packet_count":9}',
    )

    assert event["valid"] is True
    assert event["receiver_id"] == "002"
    assert event["rssi"] == -71
    assert event["packet_count"] == 9


def test_parse_rejects_receiver_mismatch_but_keeps_packet():
    event = parse_packet(
        "BLEReceiver/001",
        '{"receiver_id":"002","rssi":-60,"packet_count":1}',
    )

    assert event["valid"] is False
    assert event["receiver_id"] == "001"
    assert "does not match" in event["error"]
    assert event["raw_payload"]


def test_store_keeps_identical_packets_as_separate_events():
    store = EventStore()
    packet = parse_packet("BLEReceiver/001", '{"rssi":-65,"packet_count":4}')

    first = store.append(packet)
    second = store.append(packet)

    assert first["event_id"] == 1
    assert second["event_id"] == 2
    assert len(store.after(0)) == 2


def test_events_api_returns_all_events_in_order():
    app = create_app(start_mqtt=False)
    store = app.extensions["event_store"]
    for packet_number in range(1, 51):
        receiver = "001" if packet_number % 2 else "002"
        store.append(
            parse_packet(
                f"BLEReceiver/{receiver}",
                json.dumps({"rssi": -50 - packet_number, "packet_count": packet_number}),
            )
        )

    response = app.test_client().get("/api/events?after=0")
    body = response.get_json()

    assert response.status_code == 200
    assert len(body["events"]) == 50
    assert [event["event_id"] for event in body["events"]] == list(range(1, 51))
