// Receiver 001: BLE advertisement scanner and MQTT publisher.
// Configure the four values below before uploading.

#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include <PubSubClient.h>

char ssid[] = "YOUR_HOTSPOT_NAME";
char pass[] = "YOUR_HOTSPOT_PASSWORD";

const char* targetAddress = "E2:15:00:0A:72:43";
IPAddress mqttServer(192, 168, 1, 5);

const char* receiverId = "001";
const char* mqttClientId = "BLEReceiver001";
const char* mqttTopic = "BLEReceiver/001";

int packetCounter = 0;
unsigned long lastBLEScan = 0;
int wifiStatus = WL_IDLE_STATUS;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

void wifiReconnect() {
  Serial.print("Attempting to connect to WiFi SSID: ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  wifiStatus = WiFi.status();
  Serial.println();
  Serial.print("WiFi status code: ");
  Serial.println(wifiStatus);

  if (wifiStatus != WL_CONNECTED) {
    Serial.println("Connection attempt failed; retrying...");
  }
}

void mqttReconnect() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected");
    wifiReconnect();
  }

  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker at ");
    Serial.println(mqttServer);

    if (client.connect(mqttClientId)) {
      Serial.print("Connected as ");
      Serial.println(mqttClientId);
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  WiFi.disconnect();
  delay(1000);

  String firmwareVersion = WiFi.firmwareVersion();
  if (firmwareVersion < "3.0.0") {
    Serial.println("NINA-W102 firmware 3.0.0 or later is required for simultaneous WiFi and BLE");
    Serial.print("Current firmware version: ");
    Serial.println(firmwareVersion);
    while (1);
  }

  client.setServer(mqttServer, 1883);

  while (WiFi.status() != WL_CONNECTED) {
    wifiReconnect();
  }
  Serial.println("Connected to WiFi");

  if (!BLE.begin()) {
    Serial.println("Starting Bluetooth Low Energy module failed");
    while (1);
  }

  Serial.print("BLE receiver ");
  Serial.print(receiverId);
  Serial.print(" scanning for ");
  Serial.println(targetAddress);
  BLE.scanForAddress(targetAddress);
}

void loop() {
  if (!client.connected()) {
    mqttReconnect();
  }
  client.loop();

  BLEDevice peripheral = BLE.available();
  if (peripheral) {
    lastBLEScan = millis();
    packetCounter++;
    int rssi = peripheral.rssi();

    Serial.println("Discovered target peripheral");
    Serial.print("Receiver ID: ");
    Serial.println(receiverId);
    Serial.print("Packet Count: ");
    Serial.println(packetCounter);
    Serial.print("RSSI: ");
    Serial.println(rssi);

    char payload[96];
    snprintf(
      payload,
      sizeof(payload),
      "{\"receiver_id\":\"%s\",\"rssi\":%d,\"packet_count\":%d}",
      receiverId,
      rssi,
      packetCounter
    );

    Serial.print("Payload: ");
    Serial.println(payload);

    if (client.connected() && client.publish(mqttTopic, payload)) {
      Serial.println("Data Sent");
    } else {
      Serial.println("MQTT publish failed");
    }
    Serial.println();
  }

  unsigned long now = millis();
  if (now - lastBLEScan > 400) {
    BLE.stopScan();
    BLE.scanForAddress(targetAddress);
    lastBLEScan = millis();
  }
}
