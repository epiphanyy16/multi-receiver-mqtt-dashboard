// LED-only wiring test for the Arduino Nano 33 IoT.
//
// Connections:
//   WiFi status: built-in LED (D13)
//   MQTT status: D2 -> 220-330 ohm resistor -> LED anode; LED cathode -> GND
//   Activity:    D3 -> 220-330 ohm resistor -> LED anode; LED cathode -> GND
//
// No WiFi, MQTT, or BLE connection is required.

const int WIFI_LED_PIN = LED_BUILTIN;
const int MQTT_LED_PIN = 2;
const int ACTIVITY_LED_PIN = 3;

const unsigned long LED_ON_MS = 1500;
const unsigned long BETWEEN_TESTS_MS = 500;
const unsigned long BETWEEN_CYCLES_MS = 3000;

void setLEDs(int wifiLevel, int mqttLevel, int activityLevel)
{
  digitalWrite(WIFI_LED_PIN, wifiLevel);
  digitalWrite(MQTT_LED_PIN, mqttLevel);
  digitalWrite(ACTIVITY_LED_PIN, activityLevel);
}

void testLED(const char* label, int pin)
{
  Serial.print("ON: ");
  Serial.println(label);

  digitalWrite(pin, HIGH);
  delay(LED_ON_MS);
  digitalWrite(pin, LOW);
  delay(BETWEEN_TESTS_MS);
}

void setup()
{
  pinMode(WIFI_LED_PIN, OUTPUT);
  pinMode(MQTT_LED_PIN, OUTPUT);
  pinMode(ACTIVITY_LED_PIN, OUTPUT);
  setLEDs(LOW, LOW, LOW);

  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("Arduino Nano 33 IoT LED wiring test");
  Serial.println("The sequence repeats automatically.");
}

void loop()
{
  Serial.println();
  Serial.println("Starting LED test:");

  testLED("built-in WiFi LED (D13)", WIFI_LED_PIN);
  testLED("external MQTT LED (D2)", MQTT_LED_PIN);
  testLED("external activity LED (D3)", ACTIVITY_LED_PIN);

  Serial.println("ON: all three LEDs");
  setLEDs(HIGH, HIGH, HIGH);
  delay(LED_ON_MS);
  setLEDs(LOW, LOW, LOW);

  Serial.println("Cycle complete. Repeating in 3 seconds.");
  delay(BETWEEN_CYCLES_MS);
}
