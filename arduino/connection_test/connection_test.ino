#include <WiFiNINA.h>
#include <ArduinoBLE.h>
#include <PubSubClient.h>

// Fill these in locally before uploading. Do not commit real credentials.
char ssid[] = "YOUR_HOTSPOT_NAME";
char pass[] = "YOUR_HOTSPOT_PASSWORD";

const char* targetAddress = "E2:15:00:0A:72:43";
IPAddress mqtt_server(172, 20, 10, 2);

const int WIFI_LED_PIN = LED_BUILTIN;
const int MQTT_LED_PIN = 2;
const int ACTIVITY_LED_PIN = 3;

const unsigned long WIFI_ATTEMPT_MS = 10000;
const unsigned long BLE_TEST_MS = 15000;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool wifiPassed = false;
bool mqttPassed = false;
bool bleStarted = false;
bool targetSeen = false;

void setLEDs(int wifiLevel, int mqttLevel, int activityLevel)
{
  digitalWrite(WIFI_LED_PIN, wifiLevel);
  digitalWrite(MQTT_LED_PIN, mqttLevel);
  digitalWrite(ACTIVITY_LED_PIN, activityLevel);
}

void testLED(const char* name, int pin)
{
  Serial.print("Observe: ");
  Serial.print(name);
  Serial.println(" should be ON now.");
  digitalWrite(pin, HIGH);
  delay(1500);
  digitalWrite(pin, LOW);
  delay(500);
}

void runVisualLEDTest()
{
  Serial.println("\n=== VISUAL LED TEST ===");
  Serial.println("Expected order: built-in/D13, D2, D3, then all LEDs.");

  setLEDs(LOW, LOW, LOW);
  testLED("WiFi LED (built-in/D13)", WIFI_LED_PIN);
  testLED("MQTT LED (D2)", MQTT_LED_PIN);
  testLED("Activity LED (D3)", ACTIVITY_LED_PIN);

  Serial.println("Observe: all three LEDs should be ON now.");
  setLEDs(HIGH, HIGH, HIGH);
  delay(1500);
  setLEDs(LOW, LOW, LOW);
  delay(500);
}

void runWiFiTest()
{
  Serial.println("\n=== WIFI TEST ===");

  if (String(ssid) == "YOUR_HOTSPOT_NAME" ||
      String(pass) == "YOUR_HOTSPOT_PASSWORD")
  {
    Serial.println("FAIL: replace the WiFi credential placeholders first.");
    return;
  }

  for (int attempt = 1; attempt <= 3 && WiFi.status() != WL_CONNECTED; attempt++)
  {
    Serial.print("Attempt ");
    Serial.print(attempt);
    Serial.print("/3: connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, pass);
    unsigned long startedAt = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - startedAt < WIFI_ATTEMPT_MS)
    {
      delay(250);
    }
  }

  wifiPassed = WiFi.status() == WL_CONNECTED;
  if (!wifiPassed)
  {
    Serial.print("FAIL: WiFi did not connect. WiFi.status() = ");
    Serial.println(WiFi.status());
    return;
  }

  Serial.println("PASS: WiFi connected.");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal strength: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

void runMQTTTest()
{
  Serial.println("\n=== MQTT TEST ===");

  if (!wifiPassed)
  {
    Serial.println("SKIP: MQTT requires a working WiFi connection.");
    return;
  }

  mqttClient.setServer(mqtt_server, 1883);

  for (int attempt = 1; attempt <= 3 && !mqttClient.connected(); attempt++)
  {
    Serial.print("Attempt ");
    Serial.print(attempt);
    Serial.print("/3: connecting to broker ");
    Serial.println(mqtt_server);

    if (!mqttClient.connect("BLEConnectionTest"))
    {
      Serial.print("MQTT state: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }

  if (!mqttClient.connected())
  {
    Serial.println("FAIL: could not connect to the MQTT broker.");
    return;
  }

  mqttPassed = mqttClient.publish(
    "healthcheck/arduino",
    "{\"source\":\"connection_test\",\"ok\":true}"
  );

  if (mqttPassed)
  {
    Serial.println("PASS: connected and accepted a test publish.");
  }
  else
  {
    Serial.println("FAIL: connected, but the test publish was rejected.");
  }
}

void runBLETest()
{
  Serial.println("\n=== BLE TEST ===");

  bleStarted = BLE.begin();
  if (!bleStarted)
  {
    Serial.println("FAIL: BLE module did not start.");
    return;
  }

  Serial.println("PASS: BLE module started.");
  Serial.print("Scanning for ");
  Serial.print(targetAddress);
  Serial.print(" for ");
  Serial.print(BLE_TEST_MS / 1000);
  Serial.println(" seconds.");
  Serial.println("Press the switch during this test.");

  BLE.scanForAddress(targetAddress);
  unsigned long startedAt = millis();

  while (millis() - startedAt < BLE_TEST_MS)
  {
    if (mqttClient.connected())
    {
      mqttClient.loop();
    }

    BLEDevice peripheral = BLE.available();
    if (peripheral)
    {
      targetSeen = true;
      Serial.print("PASS: target detected with RSSI ");
      Serial.print(peripheral.rssi());
      Serial.println(" dBm.");
      break;
    }
  }

  BLE.stopScan();

  if (!targetSeen)
  {
    Serial.println("FAIL: target was not detected during the test window.");
  }
}

void printSummary()
{
  Serial.println("\n=== TEST SUMMARY ===");
  Serial.println("Visually confirm that D13, D2, and D3 lit in order.");

  Serial.print("WiFi: ");
  Serial.println(wifiPassed ? "PASS" : "FAIL");

  Serial.print("MQTT connection and publish: ");
  Serial.println(mqttPassed ? "PASS" : "FAIL");

  Serial.print("BLE module: ");
  Serial.println(bleStarted ? "PASS" : "FAIL");

  Serial.print("BLE target detection: ");
  Serial.println(targetSeen ? "PASS" : "FAIL");

  Serial.println("\nFinal LED results:");
  Serial.println("  Built-in/D13 ON = WiFi passed");
  Serial.println("  D2 ON = MQTT connection and publish passed");
  Serial.println("  D3 ON = BLE target was detected");
  Serial.println("Reset the Arduino to run all tests again.");

  setLEDs(
    wifiPassed ? HIGH : LOW,
    mqttPassed ? HIGH : LOW,
    targetSeen ? HIGH : LOW
  );
}

void setup()
{
  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(MQTT_LED_PIN, OUTPUT);
  pinMode(ACTIVITY_LED_PIN, OUTPUT);
  setLEDs(LOW, LOW, LOW);

  Serial.begin(115200);
  while (!Serial)
  {
    // The test begins when Serial Monitor is opened.
  }

  Serial.println("\nMulti-Receiver Hardware Connection Test");
  Serial.println("Board: Arduino Nano 33 IoT");

  String firmwareVersion = WiFi.firmwareVersion();
  Serial.print("NINA firmware: ");
  Serial.println(firmwareVersion);
  if (firmwareVersion < "3.0.0")
  {
    Serial.println("WARNING: update NINA firmware to 3.0.0 or later.");
  }

  runVisualLEDTest();
  runWiFiTest();
  runMQTTTest();
  runBLETest();
  printSummary();
}

void loop()
{
  if (mqttClient.connected())
  {
    mqttClient.loop();
  }
}
