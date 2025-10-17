// Assignment 2 - Arduino Receiver
// Using LDR photoresistor sensor (ARD2-2218) to detect laser blocking

#include <ArduinoBLE.h>
#include <ArduinoJson.h>

// BLE service and characteristic UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define SENSOR_CHAR_UUID    "87654321-4321-4321-4321-cba987654321"
#define COMMAND_CHAR_UUID   "11111111-2222-3333-4444-555555555555"

// BLE service and characteristics
BLEService laserService(SERVICE_UUID);
BLEStringCharacteristic sensorCharacteristic(SENSOR_CHAR_UUID, BLERead | BLENotify, 200);
BLEStringCharacteristic commandCharacteristic(COMMAND_CHAR_UUID, BLEWrite, 100);

#define LDR_PIN A2               // LDR sensor analog pin (using A2 per official examples)
#define LED_PIN 13               // Status indicator LED
#define BUZZER_PIN 5             // Buzzer pin
#define SAMPLE_SIZE 5            // Moving average sample size
#define DETECTION_THRESHOLD 25   // Detection threshold percentage (25%)
#define CONFIRMATION_COUNT 3     // Confirmation count to prevent false positives
#define CALIBRATION_SAMPLES 50   // Calibration sample count

// Moving average array
int ldrReadings[SAMPLE_SIZE];
int readingIndex = 0;
int totalReading = 0;

// State variables
int baselineValue = 0;           // Baseline value (normal laser illumination)
bool isBlocked = false;          // Currently blocked status
int confirmationCounter = 0;     // Confirmation counter
unsigned long lastDetectionTime = 0;
unsigned long blockStartTime = 0;
unsigned long totalBlockedTime = 0;
int blockCount = 0;

// BLE connection state tracking
bool previousConnectionState = false;

// Function forward declarations
void playTone(int frequency, int duration);
void sendLaserEvent(const char* eventType, unsigned long duration = 0);  // Default parameter only in declaration
void handleCommand(String command);
void calibrateBaseline();
void updateLDRReading();
int getAverageReading();
void detectLaserBlock();
void updateStatusIndicator();
void printStatus();
bool checkLDRConnection();

void setup() {
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // === Arduino Nano 33 IoT ADC Configuration ===
  analogReadResolution(10);      // Set ADC to 10-bit resolution (0-1023)
  analogReference(AR_DEFAULT);   // Use default 3.3V reference voltage

  // Initialize serial communication
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Arduino Laser Receiver Starting!");
  Serial.println("Platform: Arduino Nano 33 IoT (3.3V)");
  Serial.println("LDR Sensor: Pin A2");
  Serial.println("Status LED: Pin 13");
  Serial.println("Buzzer: Pin 5");

  // Initialize BLE
  if (!BLE.begin()) {
    Serial.println("BLE initialization failed!");
    while (1);
  }

  // Set BLE device name and service
  BLE.setLocalName("Arduino_Laser_Receiver");
  BLE.setAdvertisedService(laserService);

  // Add characteristics to service
  laserService.addCharacteristic(sensorCharacteristic);
  laserService.addCharacteristic(commandCharacteristic);

  // Add service
  BLE.addService(laserService);

  // Start advertising
  BLE.advertise();

  Serial.println("BLE service started, waiting for connection...");

  // Initialize moving average array
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    ldrReadings[i] = 0;
  }

  // LED blink to indicate startup
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }

  // Calibrate baseline value
  calibrateBaseline();

  Serial.println("System ready, starting laser monitoring...");
}

void loop() {
  // Check BLE connection state
  BLEDevice central = BLE.central();
  bool currentConnectionState = (central != NULL);

  // Check for connection state changes
  if (currentConnectionState != previousConnectionState) {
    if (currentConnectionState) {
      // Device just connected - beep once
      playTone(1000, 300);
      Serial.println("BLE device connected!");
    } else {
      // Device just disconnected - beep three times
      for (int i = 0; i < 3; i++) {
        playTone(600, 200);
        delay(150);
      }
      Serial.println("BLE device disconnected!");
    }
    previousConnectionState = currentConnectionState;
  }

  // Handle incoming commands if connected
  if (central && commandCharacteristic.written()) {
    handleCommand(commandCharacteristic.value());
  }

  // Read LDR sensor value
  updateLDRReading();

  // Detect laser blocking
  detectLaserBlock();

  // Update status indicator
  updateStatusIndicator();

  // Print status information
  printStatus();

  delay(50); // Sampling interval
}

bool checkLDRConnection() {
  // LDR connection detection method:
  // 1. Read multiple samples and check for stable variation
  // 2. Floating pins typically show large random fluctuations
  // 3. Properly connected LDR will have relatively stable readings

  int readings[10];
  int minVal = 1023, maxVal = 0;

  // Collect 10 samples
  for (int i = 0; i < 10; i++) {
    readings[i] = analogRead(LDR_PIN);
    if (readings[i] < minVal) minVal = readings[i];
    if (readings[i] > maxVal) maxVal = readings[i];
    delay(10);
  }

  // Calculate variation range
  int variation = maxVal - minVal;

  // If variation is too large (>200) or readings are abnormal (all 0s or all 1023s), consider disconnected
  if (variation > 200 || minVal == maxVal) {
    Serial.println("Detection result: min=" + String(minVal) + " max=" + String(maxVal) + " variation=" + String(variation));
    return false;
  }

  return true;
}

void calibrateBaseline() {
  Serial.println("Detecting LDR sensor connection...");

  // Check if LDR is connected
  if (!checkLDRConnection()) {
    Serial.println("Error: LDR sensor not detected!");
    Serial.println("Please check if LDR sensor is properly connected to A2 pin");

    // Error beep
    for (int i = 0; i < 5; i++) {
      playTone(600, 200);
      delay(300);
    }

    // Wait for sensor connection
    while (!checkLDRConnection()) {
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
      Serial.println("Waiting for LDR sensor connection...");
    }
  }

  Serial.println("LDR sensor connection OK!");
  Serial.println("Calibrating baseline value...");
  Serial.println("Please ensure laser is properly illuminating the LDR sensor");

  // Wait 3 seconds for user preparation
  for (int i = 3; i > 0; i--) {
    Serial.println("Countdown: " + String(i) + " seconds");
    delay(1000);
  }

  long sum = 0;
  // Collect multiple samples to calculate average
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    sum += analogRead(LDR_PIN);
    delay(20);
  }

  baselineValue = sum / CALIBRATION_SAMPLES;
  Serial.println("Baseline calibration complete: " + String(baselineValue));

  // Buzzer beep to indicate calibration complete
  playTone(1000, 200);
}

void updateLDRReading() {
  // Remove oldest reading
  totalReading -= ldrReadings[readingIndex];

  // Read new LDR value
  ldrReadings[readingIndex] = analogRead(LDR_PIN);

  // Add new reading to total
  totalReading += ldrReadings[readingIndex];

  // Move to next position
  readingIndex = (readingIndex + 1) % SAMPLE_SIZE;
}

int getAverageReading() {
  return totalReading / SAMPLE_SIZE;
}

void detectLaserBlock() {
  int currentAverage = getAverageReading();

  // Calculate percentage change relative to baseline
  float changePercent = ((float)(baselineValue - currentAverage) / baselineValue) * 100;

  if (changePercent >= DETECTION_THRESHOLD) {
    // Possible block detected
    confirmationCounter++;

    if (confirmationCounter >= CONFIRMATION_COUNT && !isBlocked) {
      // Confirm block
      isBlocked = true;
      blockStartTime = millis();
      blockCount++;
      lastDetectionTime = millis();

      Serial.println(">>> Laser blocked detected! Block count: " + String(blockCount));

      // Send BLE notification about block start
      sendLaserEvent("block_start", 0);
    }
  } else {
    // No block detected
    if (confirmationCounter > 0) {
      confirmationCounter--;
    }

    if (isBlocked && confirmationCounter == 0) {
      // Block ended
      isBlocked = false;
      unsigned long blockDuration = millis() - blockStartTime;
      totalBlockedTime += blockDuration;

      Serial.println(">>> Laser restored to normal! Block duration: " + String(blockDuration) + " ms");

      // Send BLE notification about block end with duration
      sendLaserEvent("block_end", blockDuration);
    }
  }
}

void updateStatusIndicator() {
  if (isBlocked) {
    // When blocked: LED fast blink + buzzer intermittent alarm
    static unsigned long lastBlink = 0;
    static unsigned long lastAlarm = 0;

    // LED fast blink
    if (millis() - lastBlink > 100) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      lastBlink = millis();
    }

    // Buzzer alarm (beep every 1 second)
    if (millis() - lastAlarm > 1000) {
      playTone(800, 300); // 800Hz frequency for 300ms
      lastAlarm = millis();
    }
  } else {
    // Normal: LED steady on
    digitalWrite(LED_PIN, HIGH);
  }
}

void printStatus() {
  static unsigned long lastStatusTime = 0;

  // Print detailed status every 2 seconds
  if (millis() - lastStatusTime >= 2000) {
    int currentAverage = getAverageReading();
    float changePercent = ((float)(baselineValue - currentAverage) / baselineValue) * 100;

    Serial.println("=== Status Report ===");
    Serial.println("LDR Raw Value: " + String(analogRead(LDR_PIN)));
    Serial.println("Average Value: " + String(currentAverage));
    Serial.println("Baseline Value: " + String(baselineValue));
    Serial.println("Change Rate: " + String(changePercent, 1) + "%");
    Serial.println("Block Status: " + String(isBlocked ? "Blocked" : "Normal"));
    Serial.println("Block Count: " + String(blockCount));
    Serial.println("Total Block Time: " + String(totalBlockedTime) + " ms");
    Serial.println("Uptime: " + String(millis() / 1000) + " seconds");
    Serial.println("====================");

    lastStatusTime = millis();
  }
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

void sendLaserEvent(const char* eventType, unsigned long duration) {  // Remove default parameter
  // Create JSON data for laser event
  StaticJsonDocument<200> doc;
  doc["event_type"] = eventType;
  doc["timestamp"] = millis();
  doc["device_id"] = "Arduino_Laser_Receiver";
  doc["block_count"] = blockCount;
  doc["ldr_value"] = getAverageReading();
  doc["baseline"] = baselineValue;

  if (duration > 0) {
    doc["duration_ms"] = duration;
  }

  String jsonString;
  serializeJson(doc, jsonString);

  // Send data via BLE if connected
  BLEDevice central = BLE.central();
  if (central && central.connected()) {
    sensorCharacteristic.writeValue(jsonString);
    Serial.print("→ BLE Sent: ");
    Serial.println(jsonString);
  }
}

void handleCommand(String command) {
  Serial.print("Received command: ");
  Serial.println(command);

  // Parse JSON command
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, command);

  if (error) {
    Serial.print("JSON parsing error: ");
    Serial.println(error.c_str());
    return;
  }

  // Process LED commands
  if (doc.containsKey("led")) {
    String ledCmd = doc["led"];
    if (ledCmd == "on") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED turned on");
    } else if (ledCmd == "off") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED turned off");
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
    } else if (buzzerCmd == "test") {
      playTone(800, 300);
      Serial.println("Buzzer test");
    }
  }

  // Process calibration command
  if (doc.containsKey("calibrate")) {
    if (doc["calibrate"] == true) {
      Serial.println("Starting recalibration...");
      calibrateBaseline();
    }
  }
}