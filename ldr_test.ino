// LDR模块诊断测试程序
// 用于检测ARD2-2218是否正常工作

#define LDR_PIN A0

void setup() {
  Serial.begin(9600);
  Serial.println("=== LDR模块诊断测试 ===");
  Serial.println("引脚: A0");
  Serial.println("开始持续读取...");
  Serial.println();
}

void loop() {
  int rawValue = analogRead(LDR_PIN);

  Serial.print("A0原始值: ");
  Serial.print(rawValue);
  Serial.print(" / 1023   (");
  Serial.print((rawValue * 100) / 1023);
  Serial.println("%)");

  delay(500); // 每0.5秒读取一次
}
