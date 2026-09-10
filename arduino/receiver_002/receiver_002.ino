#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include <PubSubClient.h>

// WiFi Credentials
char ssid[] = "YOUR_HOTSPOT_NAME";
char pass[] = "YOUR_HOTSPOT_PASSWORD";

// EH Switch BLE MAC Address
const char* targetAddress = "E2:15:00:0A:72:43";

const char* receiverId = "002";
const char* mqttClientId = "BLEReceiver002";
const char* mqttTopic = "BLEReceiver/002";

int packetCounter = 0;
unsigned long lastBLEScan = 0;

// MQTT Server Settings
IPAddress mqtt_server(172, 20, 10, 2);

int wifi_status = WL_IDLE_STATUS;

// LED wiring: D2/D3 -> resistor -> LED anode; LED cathode -> GND.
const int WIFI_LED_PIN = LED_BUILTIN;
const int MQTT_LED_PIN = 2;
const int ACTIVITY_LED_PIN = 3;

bool activityActive = false;
bool activityIncludesPublish = false;
bool activityQueued = false;
bool queuedActivityIncludesPublish = false;
byte activityStep = 0;
unsigned long nextActivityChange = 0;
const int ACTIVITY_LEVELS[] = {HIGH, LOW, HIGH, LOW, HIGH};
const unsigned int ACTIVITY_DURATIONS[] = {120, 180, 60, 80, 60};

WiFiClient wifiClient;
PubSubClient client(wifiClient);

void updateConnectionLEDs()
{
  bool wifiConnected = WiFi.status() == WL_CONNECTED;
  digitalWrite(WIFI_LED_PIN, wifiConnected ? HIGH : LOW);
  digitalWrite(MQTT_LED_PIN, wifiConnected && client.connected() ? HIGH : LOW);
}

void beginActivityPattern(bool published)
{
  if (activityActive)
  {
    activityQueued = true;
    queuedActivityIncludesPublish = queuedActivityIncludesPublish || published;
    return;
  }

  activityActive = true;
  activityIncludesPublish = published;
  activityStep = 0;
  digitalWrite(ACTIVITY_LED_PIN, ACTIVITY_LEVELS[activityStep]);
  nextActivityChange = millis() + ACTIVITY_DURATIONS[activityStep];
}

void updateActivityLED()
{
  if (!activityActive || millis() < nextActivityChange)
  {
    return;
  }

  activityStep++;

  if ((!activityIncludesPublish && activityStep == 1) || activityStep >= 5)
  {
    digitalWrite(ACTIVITY_LED_PIN, LOW);
    activityActive = false;
  }
  else
  {
    digitalWrite(ACTIVITY_LED_PIN, ACTIVITY_LEVELS[activityStep]);
    nextActivityChange = millis() + ACTIVITY_DURATIONS[activityStep];
  }

  if (!activityActive && activityQueued)
  {
    bool published = queuedActivityIncludesPublish;
    activityQueued = false;
    queuedActivityIncludesPublish = false;
    beginActivityPattern(published);
  }
}

void wifi_reconnect()
{
  Serial.print("Attempting to connect to WiFi SSID: ");
  Serial.println(ssid);
  wifi_status = WiFi.begin(ssid, pass);
  updateConnectionLEDs();
}

void mqtt_reconnect()
{
  wifi_status = WiFi.status();
  updateConnectionLEDs();

  if (wifi_status != WL_CONNECTED)
  {
    Serial.println("WiFi disconnected");

    while (wifi_status != WL_CONNECTED)
    {
      wifi_reconnect();
      delay(10000);
    }
    Serial.println("Reconnected to WiFi");
    Serial.println();
  }

  while (!client.connected())
  {
    digitalWrite(MQTT_LED_PIN, LOW);
    Serial.print("Attempting to connect to MQTT Server: ");
    Serial.println(mqtt_server);

    if (client.connect(mqttClientId))
    {
      Serial.println("Connected to MQTT Server");
      Serial.println();
      digitalWrite(MQTT_LED_PIN, HIGH);
    }
    else
    {
      Serial.print("MQTT Connection Failed, rc=");
      Serial.println(client.state());
      Serial.println("Waiting 5 seconds before attempting to reconnect");
      delay(5000);
    }
  }
}

void setup()
{
  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(MQTT_LED_PIN, OUTPUT);
  pinMode(ACTIVITY_LED_PIN, OUTPUT);
  digitalWrite(WIFI_LED_PIN, LOW);
  digitalWrite(MQTT_LED_PIN, LOW);
  digitalWrite(ACTIVITY_LED_PIN, LOW);

  Serial.begin(115200);
  while (!Serial);

  String fv = WiFi.firmwareVersion();

  if (fv < "3.0.0")
  {
    Serial.println("NINA-W102 firmware version 3.0.0 or later is required to run Wi-Fi and BLE simultaneously");
    Serial.print("Current firmware version: ");
    Serial.println(fv);

    while (1);
  }

  client.setServer(mqtt_server, 1883);

  // Connect to WiFi
  while (wifi_status != WL_CONNECTED)
  {
    wifi_reconnect();
    delay(10000);
  }
  Serial.println("Connected to WiFi");
  Serial.println();
  updateConnectionLEDs();

  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy module failed!");

    while (1);
  }

  Serial.println("BLE Central - Scan For Address");
  Serial.println();

  BLE.scanForAddress(targetAddress);
}

void loop()
{
  updateActivityLED();
  updateConnectionLEDs();

  if (!client.connected())
  {
    mqtt_reconnect();
  }
  client.loop();
  updateConnectionLEDs();

  BLEDevice peripheral = BLE.available();

  if (peripheral)
  {
    int rssi;
    lastBLEScan = millis();

    packetCounter++;
    Serial.println("Discovered target peripheral");

    Serial.print("Receiver ID: ");
    Serial.println(receiverId);

    // Print Packet Count
    Serial.print("Packet Count: ");
    Serial.println(packetCounter);

    // Print RSSI
    rssi = peripheral.rssi();
    Serial.print("RSSI: ");
    Serial.println(rssi);

    // Serialize receiver ID, RSSI, and packet count.
    char payload[96];
    snprintf(
      payload,
      sizeof(payload),
      "{\"receiver_id\":\"%s\",\"rssi\":%d,\"packet_count\":%d}",
      receiverId,
      rssi,
      packetCounter
    );

    Serial.print("Payload:");
    Serial.println(payload);

    bool published = false;
    if (client.connected() && client.publish(mqttTopic, payload))
    {
      published = true;
      Serial.println("Data Sent");
      Serial.println();
    }
    else
    {
      Serial.println("MQTT publish failed");
      Serial.println();
    }

    beginActivityPattern(published);
    updateActivityLED();
  }

  unsigned long now = millis();
  if (now - lastBLEScan > 400)
  {
    BLE.stopScan();
    delay(100);
    BLE.scanForAddress(targetAddress);
    lastBLEScan = millis();
  }
}
