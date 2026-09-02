// --- Libraries ---
#include <WiFiNINA.h>     // Provides WiFi connectivity for boards with NINA-W102 modules (like Arduino Nano 33 IoT)
#include <ArduinoBLE.h>   // Provides Bluetooth Low Energy (BLE) functionality
#include <PubSubClient.h> // Provides MQTT client functionality for publishing and subscribing to topics

// --- Configuration & Global Variables ---

// WiFi Credentials
char ssid[] = "YOUR_HOTSPOT_NAME"; // Your WiFi network name (SSID)
char pass[] = "YOUR_HOTSPOT_PASSWORD"; // Your WiFi network password

// EH Switch BLE MAC Address
const char* targetAddress = "E2:15:00:0A:72:43"; // The MAC address of the specific BLE device we want to listen to

int packetCounter = 0;         // Keeps track of how many times we've received data from the target BLE device
unsigned long lastBLEScan = 0; // Stores the time (in milliseconds) when the last BLE scan was restarted

// MQTT Server Settings
IPAddress mqtt_server(172, 20, 10, 2); // IP address of the MQTT broker (laptop 1's hotspot IP)

int wifi_status = WL_IDLE_STATUS; // Stores the current status of the WiFi connection

WiFiClient wifiClient;           // Creates a base WiFi client to handle the network connection
PubSubClient client(wifiClient); // Creates the MQTT client, passing the WiFi client to it for network access

// Function to attempt a connection to the WiFi network
// Calls WiFi.begin() ONCE, then polls status - does NOT re-trigger begin() repeatedly,
// which was previously interrupting connection attempts mid-handshake.
void wifi_reconnect() {
  Serial.print("Attempting to connect to WiFi SSID: ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass); // called once per attempt

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) { // up to 20 seconds
    delay(500);
    Serial.print(".");
    attempts++;
  }

  wifi_status = WiFi.status();
  Serial.println();
  Serial.print("Status code: ");
  Serial.println(wifi_status);

  if (wifi_status != WL_CONNECTED) {
    Serial.println("Connection attempt failed, will retry...");
  }
}

// Function to handle reconnecting to both WiFi (if dropped) and the MQTT broker
void mqtt_reconnect()
{
  wifi_status = WiFi.status(); // Check the current WiFi connection status

  // If we are not connected to WiFi, try to reconnect first
  if (wifi_status != WL_CONNECTED)
  {
    Serial.println("WiFi disconnected");

    // Loop until WiFi connects successfully
    while (wifi_status != WL_CONNECTED)
    {
      wifi_reconnect();
    }
    Serial.println("Reconnected to WiFi");
    Serial.println();
  }

  // Once WiFi is confirmed connected, check the MQTT broker connection
  while (!client.connected())
  {
    Serial.print("Attempting to connect to MQTT Server: ");
    Serial.println(mqtt_server);

    // Attempt to connect to MQTT with the client ID "BLEReceiver"
    if (client.connect("BLEReceiver"))
    {
      Serial.println("Connected to MQTT Server");
      Serial.println();
    }
    else
    {
      // If connection fails, print the error code (rc=return code)
      Serial.print("MQTT Connection Failed, rc=");
      Serial.println(client.state());
      Serial.println("Waiting 5 seconds before attempting to reconnect");
      delay(5000); // Wait 5 seconds before trying again
    }
  }
}

// The setup function runs once when you press reset or power the board
void setup()
{
  // Initialize serial communication for debugging output
  Serial.begin(115200);
  while (!Serial); // Wait for the serial port to connect (needed for native USB boards)

  // Clear any stale/lingering WiFi state from a previous run
  WiFi.disconnect();
  delay(1000);

  // Check the WiFi module firmware version
  String fv = WiFi.firmwareVersion();
  // Using both WiFi and BLE at the same time requires firmware version 3.0.0 or higher
  if (fv < "3.0.0")
  {
    Serial.println("NINA-W102 firmware version 3.0.0 or later is required to run Wi-Fi and BLE simultaneously");
    Serial.print("Current firmware version: ");
    Serial.println(fv);

    while(1); // Halt execution entirely if the firmware is too old
  }

  // Configure the MQTT client with the server IP and standard MQTT port (1883)
  client.setServer(mqtt_server, 1883);

  // Connect to WiFi initially
  while (wifi_status != WL_CONNECTED)
  {
    wifi_reconnect();
  }
  Serial.println("Connected to WiFi");
  Serial.println();

  // Initialize the BLE hardware
  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1); // Halt execution if BLE fails to start
  }

  Serial.println("BLE Central - Scan For Address");
  Serial.println();

  // Start scanning for BLE devices, filtering ONLY for our specific target device's MAC address
  BLE.scanForAddress(targetAddress);
}

// The loop function runs continuously after setup() finishes
void loop()
{
  // Ensure we are connected to the MQTT broker. Reconnect if the connection is lost.
  if (!client.connected())
  {
    mqtt_reconnect();
  }
  // client.loop() must be called regularly to allow the MQTT client to process incoming messages and maintain its connection to the server
  client.loop();

  // Check if a new BLE device (matching our target address) has been found during the scan
  BLEDevice peripheral = BLE.available();

  if (peripheral)
  {
    int rssi;
    // Update our timer since we just heard from the peripheral
    lastBLEScan = millis();

    // Increment how many times we've seen this device
    packetCounter++;
    Serial.println("Discovered target peripheral");

    // Print Packet Count for debugging
    Serial.print("Packet Count: ");
    Serial.println(packetCounter);

    // Get the RSSI (Received Signal Strength Indicator), which tells us roughly how close the device is
    rssi = peripheral.rssi();
    Serial.print("RSSI: ");
    Serial.println(rssi);

    // Serializing RSSI and Packet Count into a JSON string format
    // Example output: {"rssi":-75,"packet_count":42}
    String payload1 = "{\"rssi\":";
    String payload2 = payload1 + rssi;
    String payload3 = payload2 + "," + "\"packet_count\":";
    String payload4 = payload3 + packetCounter;
    String payload = payload4 + "}";

    Serial.print("Payload:");
    Serial.println(payload);

    // Convert the String to a character array so it can be sent via MQTT
    char attributes[40];
    payload.toCharArray(attributes, 40);

    // Publish the formatted JSON string to the "BLEReceiver/001" MQTT topic
    if (client.connected())
    {
      client.publish("BLEReceiver/001", attributes);
      Serial.println("Data Sent");
      Serial.println();
    }
  }

  // Workaround for BLE scanning issues:
  // Occasionally, the BLE scanner might hang or stop finding devices.
  // We check if it has been more than 400 milliseconds since we last found the device.
  // If so, we restart the scan process to keep things running smoothly.
  // (removed the artificial delay(100) here - it was closing the "listening window"
  //  for 100ms every ~400ms cycle, which is a real chance to miss a brief EH switch burst)
  unsigned long now = millis();
  if (now - lastBLEScan > 400)
  {
    BLE.stopScan(); // Stop the current scan
    BLE.scanForAddress(targetAddress); // Restart the scan specifically for our device
    lastBLEScan = millis(); // Reset the timer
  }
}