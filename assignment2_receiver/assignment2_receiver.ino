// Assignment 2 - Arduino接收端
// 使用LDR光敏电阻传感器 (ARD2-2218) 检测激光遮挡

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

#define LDR_PIN A2               // LDR传感器连接的模拟引脚 (改用A2测试，官方示例用A2)
#define LED_PIN 13               // 状态指示LED
#define BUZZER_PIN 5             // 蜂鸣器引脚
#define SAMPLE_SIZE 5            // 滑动平均样本数量
#define DETECTION_THRESHOLD 25   // 检测阈值百分比(25%)
#define CONFIRMATION_COUNT 3     // 确认次数防误报
#define CALIBRATION_SAMPLES 50   // 校准时采样次数

// 滑动平均数组
int ldrReadings[SAMPLE_SIZE];
int readingIndex = 0;
int totalReading = 0;

// 状态变量
int baselineValue = 0;           // 基线值(正常激光照射时的值)
bool isBlocked = false;          // 当前是否被遮挡
int confirmationCounter = 0;     // 确认计数器
unsigned long lastDetectionTime = 0;
unsigned long blockStartTime = 0;
unsigned long totalBlockedTime = 0;
int blockCount = 0;

// BLE connection state tracking
bool previousConnectionState = false;

void setup() {
  // 初始化引脚
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // === Arduino Nano 33 IoT ADC配置 ===
  analogReadResolution(10);      // 设置ADC为10位分辨率(0-1023)
  analogReference(AR_DEFAULT);   // 使用默认3.3V参考电压

  // 初始化串口通信
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Arduino激光接收端启动!");
  Serial.println("平台: Arduino Nano 33 IoT (3.3V)");
  Serial.println("LDR传感器: 引脚 A2");
  Serial.println("状态LED: 引脚 13");
  Serial.println("蜂鸣器: 引脚 5");

  // Initialize BLE
  if (!BLE.begin()) {
    Serial.println("BLE启动失败!");
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

  Serial.println("BLE服务已启动，等待连接...");
  
  // 初始化滑动平均数组
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    ldrReadings[i] = 0;
  }
  
  // LED闪烁表示启动
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  
  // 校准基线值
  calibrateBaseline();
  
  Serial.println("系统准备就绪，开始监测激光...");
}

void loop() {
  // Check BLE connection state
  BLEDevice central = BLE.central();
  bool currentConnectionState = (central != NULL);

  // Check for connection state changes
  if (currentConnectionState != previousConnectionState) {
    if (currentConnectionState) {
      // Device just connected - beep once
      boFangYinDiao(1000, 300);
      Serial.println("BLE设备已连接!");
    } else {
      // Device just disconnected - beep three times
      for (int i = 0; i < 3; i++) {
        boFangYinDiao(600, 200);
        delay(150);
      }
      Serial.println("BLE设备已断开!");
    }
    previousConnectionState = currentConnectionState;
  }

  // Handle incoming commands if connected
  if (central && commandCharacteristic.written()) {
    chuLiMingLing(commandCharacteristic.value());
  }

  // 读取LDR传感器值
  updateLDRReading();

  // 检测激光遮挡
  detectLaserBlock();

  // 更新状态指示
  updateStatusIndicator();

  // 输出状态信息
  printStatus();

  delay(50); // 采样间隔
}

bool checkLDRConnection() {
  // 检测LDR连接的方法：
  // 1. 读取多个样本，检查是否有稳定的变化
  // 2. 浮空引脚通常会有很大的随机波动
  // 3. 正确连接的LDR会有相对稳定的读数
  
  int readings[10];
  int minVal = 1023, maxVal = 0;
  
  // 采集10个样本
  for (int i = 0; i < 10; i++) {
    readings[i] = analogRead(LDR_PIN);
    if (readings[i] < minVal) minVal = readings[i];
    if (readings[i] > maxVal) maxVal = readings[i];
    delay(10);
  }
  
  // 计算波动范围
  int variation = maxVal - minVal;
  
  // 如果波动过大(>200)或读数异常(全是0或全是1023)，认为未连接
  if (variation > 200 || minVal == maxVal) {
    Serial.println("检测结果: 最小值=" + String(minVal) + " 最大值=" + String(maxVal) + " 波动=" + String(variation));
    return false;
  }
  
  return true;
}

void calibrateBaseline() {
  Serial.println("正在检测LDR传感器连接...");
  
  // 检测LDR是否连接
  if (!checkLDRConnection()) {
    Serial.println("错误: 未检测到LDR传感器!");
    Serial.println("请检查LDR传感器是否正确连接到A0引脚");
    
    // 错误提示音
    for (int i = 0; i < 5; i++) {
      boFangYinDiao(600, 200);
      delay(300);
    }
    
    // 停止执行，等待传感器连接
    while (!checkLDRConnection()) {
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
      Serial.println("等待LDR传感器连接...");
    }
  }
  
  Serial.println("LDR传感器连接正常!");
  Serial.println("正在校准基线值...");
  Serial.println("请确保激光正常照射到LDR传感器");
  
  // 等待3秒让用户准备
  for (int i = 3; i > 0; i--) {
    Serial.println("倒计时: " + String(i) + "秒");
    delay(1000);
  }
  
  long sum = 0;
  // 采集多个样本计算平均值
  for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
    sum += analogRead(LDR_PIN);
    delay(20);
  }
  
  baselineValue = sum / CALIBRATION_SAMPLES;
  Serial.println("基线值校准完成: " + String(baselineValue));
  
  // 蜂鸣器提示校准完成
  boFangYinDiao(1000, 200);
}

void updateLDRReading() {
  // 移除最旧的读数
  totalReading -= ldrReadings[readingIndex];
  
  // 读取新的LDR值
  ldrReadings[readingIndex] = analogRead(LDR_PIN);
  
  // 添加新读数到总和
  totalReading += ldrReadings[readingIndex];
  
  // 移动到下一个位置
  readingIndex = (readingIndex + 1) % SAMPLE_SIZE;
}

int getAverageReading() {
  return totalReading / SAMPLE_SIZE;
}

void detectLaserBlock() {
  int currentAverage = getAverageReading();

  // 计算相对于基线的变化百分比
  float changePercent = ((float)(baselineValue - currentAverage) / baselineValue) * 100;

  if (changePercent >= DETECTION_THRESHOLD) {
    // 检测到可能的遮挡
    confirmationCounter++;

    if (confirmationCounter >= CONFIRMATION_COUNT && !isBlocked) {
      // 确认遮挡
      isBlocked = true;
      blockStartTime = millis();
      blockCount++;
      lastDetectionTime = millis();

      Serial.println(">>> 检测到激光遮挡! 遮挡次数: " + String(blockCount));

      // Send BLE notification about block start
      sendLaserEvent("block_start");
    }
  } else {
    // 未检测到遮挡
    if (confirmationCounter > 0) {
      confirmationCounter--;
    }

    if (isBlocked && confirmationCounter == 0) {
      // 遮挡结束
      isBlocked = false;
      unsigned long blockDuration = millis() - blockStartTime;
      totalBlockedTime += blockDuration;

      Serial.println(">>> 激光恢复正常! 遮挡持续时间: " + String(blockDuration) + "毫秒");

      // Send BLE notification about block end with duration
      sendLaserEvent("block_end", blockDuration);
    }
  }
}

void updateStatusIndicator() {
  if (isBlocked) {
    // 遮挡时LED快闪 + 蜂鸣器间歇报警
    static unsigned long lastBlink = 0;
    static unsigned long lastAlarm = 0;
    
    // LED快闪
    if (millis() - lastBlink > 100) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      lastBlink = millis();
    }
    
    // 蜂鸣器报警声 (每1秒响一次)
    if (millis() - lastAlarm > 1000) {
      boFangYinDiao(800, 300); // 800Hz频率响300毫秒
      lastAlarm = millis();
    }
  } else {
    // 正常时LED常亮
    digitalWrite(LED_PIN, HIGH);
  }
}

void printStatus() {
  static unsigned long lastStatusTime = 0;
  
  // 每2秒输出一次详细状态
  if (millis() - lastStatusTime >= 2000) {
    int currentAverage = getAverageReading();
    float changePercent = ((float)(baselineValue - currentAverage) / baselineValue) * 100;
    
    Serial.println("=== 状态报告 ===");
    Serial.println("LDR原始值: " + String(analogRead(LDR_PIN)));
    Serial.println("平均值: " + String(currentAverage));
    Serial.println("基线值: " + String(baselineValue));
    Serial.println("变化率: " + String(changePercent, 1) + "%");
    Serial.println("遮挡状态: " + String(isBlocked ? "遮挡中" : "正常"));
    Serial.println("遮挡次数: " + String(blockCount));
    Serial.println("总遮挡时间: " + String(totalBlockedTime) + "毫秒");
    Serial.println("运行时间: " + String(millis() / 1000) + "秒");
    Serial.println("===============");
    
    lastStatusTime = millis();
  }
}

void boFangYinDiao(int frequency, int duration) {
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

void sendLaserEvent(const char* eventType, unsigned long duration = 0) {
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
    Serial.print("→ BLE发送: ");
    Serial.println(jsonString);
  }
}

void chuLiMingLing(String command) {
  Serial.print("收到命令: ");
  Serial.println(command);

  // Parse JSON command
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, command);

  if (error) {
    Serial.print("JSON解析错误: ");
    Serial.println(error.c_str());
    return;
  }

  // Process LED commands
  if (doc.containsKey("led")) {
    String ledCmd = doc["led"];
    if (ledCmd == "on") {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED已打开");
    } else if (ledCmd == "off") {
      digitalWrite(LED_PIN, LOW);
      Serial.println("LED已关闭");
    } else if (ledCmd == "toggle") {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      Serial.println("LED已切换");
    }
  }

  // Process buzzer commands
  if (doc.containsKey("buzzer")) {
    String buzzerCmd = doc["buzzer"];
    if (buzzerCmd == "beep") {
      boFangYinDiao(1000, 200);
      Serial.println("蜂鸣器已响");
    } else if (buzzerCmd == "test") {
      boFangYinDiao(800, 300);
      Serial.println("蜂鸣器测试");
    }
  }

  // Process calibration command
  if (doc.containsKey("calibrate")) {
    if (doc["calibrate"] == true) {
      Serial.println("开始重新校准...");
      calibrateBaseline();
    }
  }
}