#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include <PubSubClient.h>

// WiFi Credentials
char ssid[] = "";
char pass[] = "";

// EH Switch BLE MAC Address 
const char* targetAddress = "";

int packetCounter = 0;
unsigned long lastBLEScan = 0;

// IIIT MQTT Server Settings
IPAddress mqtt_server(0, 0, 0, 0); // Place Holder

int wifi_status = WL_IDLE_STATUS;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// ── LED Pin Definitions ──────────────────────────────────────────────────────
// LED_BUILTIN (pin 13) : WiFi status   – ON = connected, OFF = not connected
// LED_WIFI    (pin 2)  : MQTT status   – ON = connected, OFF = not connected
// LED_BLE     (pin 3)  : BLE/MQTT events
//                        BLE packet received  → fast double-blink (2 × 100 ms)
//                        MQTT publish sent    → slow single blink  (1 × 400 ms)
const int LED_WIFI = 2;
const int LED_BLE  = 3;

// Helper: update WiFi status LED
void update_wifi_led()
{
  digitalWrite(LED_BUILTIN, (WiFi.status() == WL_CONNECTED) ? HIGH : LOW);
}

// Helper: update MQTT status LED
void update_mqtt_led()
{
  digitalWrite(LED_WIFI, client.connected() ? HIGH : LOW);
}

// Helper: fast double-blink on LED_BLE (BLE packet received)
void blink_ble_received()
{
  for (int i = 0; i < 2; i++)
  {
    digitalWrite(LED_BLE, HIGH);
    delay(100);
    digitalWrite(LED_BLE, LOW);
    delay(100);
  }
}

// Helper: slow single blink on LED_BLE (MQTT publish sent)
void blink_mqtt_sent()
{
  digitalWrite(LED_BLE, HIGH);
  delay(400);
  digitalWrite(LED_BLE, LOW);
}
// ────────────────────────────────────────────────────────────────────────────

void wifi_reconnect()
{
  Serial.print("Attempting to connect to WiFi SSID: ");
  Serial.println(ssid);
  wifi_status = WiFi.begin(ssid, pass);
}

void mqtt_reconnect()
{
  wifi_status = WiFi.status();

  if (wifi_status != WL_CONNECTED)
  {
    Serial.println("WiFi disconnected");
    update_wifi_led();

    while (wifi_status != WL_CONNECTED)
    {
      wifi_reconnect();
      delay(10000);
      wifi_status = WiFi.status();
    }
    Serial.println("Reconnected to WiFi");
    Serial.println(); 
    Serial.println();
    update_wifi_led();
  } 

  update_mqtt_led();

  while (!client.connected())
  {
    Serial.print("Attempting to connect to MQTT Server: ");
    Serial.println(mqtt_server);
    if (client.connect("BLEReceiver"))
    {
      Serial.println("Connected to MQTT Server");
      Serial.println();
      update_mqtt_led();
    }

    else
    {
      Serial.print("MQTT Connection Failed, rc=");
      Serial.println(client.state());
      Serial.println("Waiting 5 seconds before attempting to reconnect");
      update_mqtt_led();
      delay(5000);
    }
  }

}

void setup()
{
  Serial.begin(115200);
  while (!Serial);

  // Initialise LED pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_WIFI,    OUTPUT);
  pinMode(LED_BLE,     OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(LED_WIFI,    LOW);
  digitalWrite(LED_BLE,     LOW);

  String fv = WiFi.firmwareVersion();

  if (fv < "3.0.0")
  {
    Serial.println("NINA-W102 firmware version 3.0.0 or later is required to run Wi-Fi and BLE simultaneously");
    Serial.print("Current firmware version: ");
    Serial.println(fv);

    while(1);
  }

  client.setServer(mqtt_server, 1883);
  
  // Connect to WiFi
  while (wifi_status != WL_CONNECTED)
  {
    wifi_reconnect(); 
    delay(10000);
    wifi_status = WiFi.status();
  }
  Serial.println("Connected to WiFi");
  Serial.println();
  update_wifi_led();

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
  if (!client.connected())
  {
    update_mqtt_led();
    mqtt_reconnect();
  }
  client.loop();

  BLEDevice peripheral = BLE.available();

  if (peripheral)
  {
    int rssi;
    lastBLEScan = millis();

    packetCounter ++;
    Serial.println("Discovered target peripheral");

    // BLE packet received – fast double-blink
    blink_ble_received();

    // Print Packet Count
    Serial.print("Packet Count: ");
    Serial.println(packetCounter);
    
    // Print RSSI
    rssi = peripheral.rssi();
    Serial.print("RSSI: ");
    Serial.println(rssi);

    // Serializing RSSI and Packet Count
    String payload1 = "{\"rssi\":";
    String payload2 = payload1 + rssi;
    String payload3 = payload2 + "," + "\"packet_count\":";
    String payload4 = payload3 + packetCounter;
    String payload = payload4 + "}";
    Serial.print("Payload:");
    Serial.println(payload);
    char attributes[40];
    payload.toCharArray(attributes, 40);

    if (client.connected())
    {
      client.publish("BLEReceiver/001", attributes);
      Serial.println("Data Sent");
      Serial.println();

      // MQTT publish sent – slow single blink
      blink_mqtt_sent();
    }
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