// Assignment 2 - Arduino发射端 (Nano 33 IoT - 3.3V)
// 控制激光发射器模块 (ARD2-2065)
// 激光持续开启模式
//
// === 接线说明 (3.3V系统) ===
// 激光模块 S (信号)  -> Arduino A0
// 激光模块 VCC (电源) -> Arduino 3.3V (不是VIN!)
// 激光模块 GND (地)   -> Arduino GND
// 注意: Nano 33 IoT是3.3V逻辑，激光模块需使用3.3V供电

#define LASER_PIN 2         // 激光发射器连接的模拟引脚
#define LASER_POWER 255      // 激光功率 (0-255, 255=最大)

void setup() {
  // 初始化引脚
  pinMode(LASER_PIN, OUTPUT);

  // 初始化串口通信
  Serial.begin(9600);
  Serial.println("Arduino激光发射端启动!");
  Serial.println("========== 激光模式 ==========");
  Serial.println("激光引脚: " + String(LASER_PIN));
  Serial.println("功率: " + String((LASER_POWER * 100) / 255) + "%");
  Serial.println("=============================");

  // 启动延迟
  Serial.println("准备开始激光发射...");
  delay(2000);

  // 激光持续开启 - 使用PWM最大功率
  analogWrite(LASER_PIN, LASER_POWER);
  Serial.println(">>> 激光发射器已开启 (最大功率)");
}

void loop() {
  // 串口监控状态输出
  printStatus();

  delay(1000); // 每秒输出一次状态
}

void printStatus() {
  static unsigned long lastStatusTime = 0;
  unsigned long currentTime = millis();

  // 每5秒输出一次状态
  if (currentTime - lastStatusTime >= 5000) {
    Serial.println("=== 状态报告 ===");
    Serial.println("激光发射器: 开启 (持续模式)");
    Serial.println("运行时间: " + String(currentTime / 1000) + "秒");
    Serial.println("================");
    lastStatusTime = currentTime;
  }
}