#include <ArduinoBLE.h>
#include <DHT.h>
#include <ArduinoJson.h>

// DHT11 sensor configuration
#define DHT_PIN 2
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);

// Buzzer configuration
#define BUZZER_PIN 5
#define LED_PIN LED_BUILTIN  // Use onboard LED

// BLE service and characteristic UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define SENSOR_CHAR_UUID    "87654321-4321-4321-4321-cba987654321"
#define COMMAND_CHAR_UUID   "11111111-2222-3333-4444-555555555555"

// BLE service and characteristics
BLEService sensorService(SERVICE_UUID);
BLEStringCharacteristic sensorCharacteristic(SENSOR_CHAR_UUID, BLERead | BLENotify, 100);
BLEStringCharacteristic commandCharacteristic(COMMAND_CHAR_UUID, BLEWrite, 100);

// Timer variables
unsigned long previousMillis = 0;
const long interval = 5000; // Send data every 5 seconds

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  // Initialize DHT sensor
  dht.begin();
  
  // Initialize buzzer and onboard LED pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize BLE
  if (!BLE.begin()) {
    Serial.println("Failed to start BLE!");
    while (1);
  }
  
  // Set BLE device name and service
  BLE.setLocalName("Arduino_IoT_Sensor_hhx");
  BLE.setAdvertisedService(sensorService);
  
  // Add characteristics to service
  sensorService.addCharacteristic(sensorCharacteristic);
  sensorService.addCharacteristic(commandCharacteristic);
  
  // Add service
  BLE.addService(sensorService);
  
  // Start advertising
  BLE.advertise();
  
  // Startup beep (2 times)
  for (int i = 0; i < 2; i++) {
    playTone(800, 200);  // Beep at 800Hz for 200ms
    delay(300);          // Pause between beeps
  }
  
  Serial.println("Arduino BLE sensor started");
  Serial.println("Waiting for BLE connection...");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Check if BLE device is connected
  BLEDevice central = BLE.central();
  
  if (central) {
    // Device is connected - handle data and commands
    
    // Check for incoming commands
    if (commandCharacteristic.written()) {
      processCommand(commandCharacteristic.value());
    }
    
    // Read and send sensor data every 5 seconds (only when connected)
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      readAndSendData();
      digitalWrite(LED_PIN, HIGH); // Briefly flash LED when sending data
      delay(100);
      digitalWrite(LED_PIN, LOW);
    }
  } else {
    // No device connected - just wait and indicate status
    digitalWrite(LED_PIN, LOW);
    
    // Show we're waiting with a slow blink every 2 seconds
    if (currentMillis % 2000 < 100) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
  }
}

void readAndSendData() {
  // Read DHT11 sensor data
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  // Check if reading was successful
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("DHT sensor read failed!");
    return;
  }
  
  // Create JSON data
  StaticJsonDocument<200> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["timestamp"] = millis();
  doc["device_id"] = "Arduino_hhx";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Send data via BLE (only called when connected)
  sensorCharacteristic.writeValue(jsonString);
  
  // Simple serial output
  Serial.print("→ ");
  Serial.println(jsonString);
}

void playHeartbeat() {
  // Simple heartbeat: lub-dub pattern with LED sync
  digitalWrite(LED_PIN, HIGH);
  playTone(200, 100);  // lub
  digitalWrite(LED_PIN, LOW);
  delay(50);
  
  digitalWrite(LED_PIN, HIGH);
  playTone(400, 80);   // dub
  digitalWrite(LED_PIN, LOW);
  delay(50);
  
  digitalWrite(LED_PIN, HIGH);
  playTone(200, 100);  // lub
  digitalWrite(LED_PIN, LOW);
  delay(50);
  
  digitalWrite(LED_PIN, HIGH);
  playTone(400, 80);   // dub
  digitalWrite(LED_PIN, LOW);
}

void playTone(int frequency, int duration) {
  long period = 1000000L / frequency;
  long halfPeriod = period / 2;
  long cycles = ((long)frequency * (long)duration) / 1000;
  
  for (long i = 0; i < cycles; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(halfPeriod);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(halfPeriod);
  }
}

void processCommand(String command) {
  Serial.print("Received command: ");
  Serial.println(command);
  
  // Parse JSON command
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, command);
  
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Process LED commands
  if (doc.containsKey("led")) {
    String ledCmd = doc["led"];
    if (ledCmd == "on") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED turned ON");
    } else if (ledCmd == "off") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED turned OFF");
    } else if (ledCmd == "toggle") {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      Serial.println("LED toggled");
    }
  }
  
  // Process buzzer commands
  if (doc.containsKey("buzzer")) {
    String buzzerCmd = doc["buzzer"];
    if (buzzerCmd == "beep") {
      playTone(1000, 200);
      Serial.println("Buzzer beeped");
    } else if (buzzerCmd == "heartbeat") {
      playHeartbeat();
      Serial.println("Buzzer played heartbeat");
    }
  }
}