// --- Libraries ---
#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include <PubSubClient.h>

// --- Configuration & Global Variables ---

// WiFi Credentials (your mobile hotspot)
char ssid[] = "YOUR_HOTSPOT_NAME";
char pass[] = "YOUR_HOTSPOT_PASSWORD";

// BLE Switch MAC Address (from Step 1 above)
const char* targetAddress = "xx:xx:xx:xx:xx:xx";

int packetCounter = 0;
unsigned long lastBLEScan = 0;

// MQTT Broker = Laptop 1's IP address on the hotspot network
IPAddress mqtt_server(192, 168, 1, 5); // <-- replace with Laptop 1's actual IP

int wifi_status = WL_IDLE_STATUS;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

void wifi_reconnect() {
  Serial.print("Attempting to connect to WiFi SSID: ");
  Serial.println(ssid);
  wifi_status = WiFi.begin(ssid, pass);
}

void mqtt_reconnect() {
  wifi_status = WiFi.status();

  if (wifi_status != WL_CONNECTED) {
    Serial.println("WiFi disconnected");
    while (wifi_status != WL_CONNECTED) {
      wifi_reconnect();
      delay(10000);
    }
    Serial.println("Reconnected to WiFi");
    Serial.println();
  }

  while (!client.connected()) {
    Serial.print("Attempting to connect to MQTT Server: ");
    Serial.println(mqtt_server);

    if (client.connect("BLEReceiver")) {
      Serial.println("Connected to MQTT Server");
      Serial.println();
    } else {
      Serial.print("MQTT Connection Failed, rc=");
      Serial.println(client.state());
      Serial.println("Waiting 5 seconds before attempting to reconnect");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  String fv = WiFi.firmwareVersion();
  if (fv < "3.0.0") {
    Serial.println("NINA-W102 firmware version 3.0.0 or later is required to run Wi-Fi and BLE simultaneously");
    Serial.print("Current firmware version: ");
    Serial.println(fv);
    while (1);
  }

  client.setServer(mqtt_server, 1883);

  while (wifi_status != WL_CONNECTED) {
    wifi_reconnect();
    delay(10000);
  }
  Serial.println("Connected to WiFi");
  Serial.println();

  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1);
  }

  Serial.println("BLE Central - Scan For Address");
  Serial.println();

  BLE.scanForAddress(targetAddress);
}

void loop() {
  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();

  BLEDevice peripheral = BLE.available();

  if (peripheral) {
    int rssi;
    lastBLEScan = millis();
    packetCounter++;
    Serial.println("Discovered target peripheral");

    Serial.print("Packet Count: ");
    Serial.println(packetCounter);

    rssi = peripheral.rssi();
    Serial.print("RSSI: ");
    Serial.println(rssi);

    String payload1 = "{\"rssi\":";
    String payload2 = payload1 + rssi;
    String payload3 = payload2 + "," + "\"packet_count\":";
    String payload4 = payload3 + packetCounter;
    String payload = payload4 + "}";

    Serial.print("Payload:");
    Serial.println(payload);

    char attributes[40];
    payload.toCharArray(attributes, 40);

    if (client.connected()) {
      client.publish("BLEReceiver/001", attributes);
      Serial.println("Data Sent");
      Serial.println();
    }
  }

  unsigned long now = millis();
  if (now - lastBLEScan > 400) {
    BLE.stopScan();
    delay(100);
    BLE.scanForAddress(targetAddress);
    lastBLEScan = millis();
  }
}