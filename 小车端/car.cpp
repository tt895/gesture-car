/*
 * 小车端 ESP32 —— 支持状态上报 + 正确 PWM 输出
 * 功能：
 *   - 连接 "GestureCar_AP"
 *   - 接收手势控制指令
 *   - 控制电机（使用 ledc PWM）
 *   - 每 200ms 上报运动状态到监控中心 → Unity 显示
 */

#include <WiFi.h>//arduino-esp32 WiFi 库

// ===== Wi-Fi 配置 =====
const char* ssid = "GestureCar_AP";
const char* password = "12345678";
const char* monitorIP = "192.168.4.1";
const uint16_t monitorPort = 8080;

// ===== 电机引脚（推荐使用支持 PWM 的引脚）=====
#define ENA 18  // 左轮 PWM
#define IN1 17  // 左轮方向
#define IN2 16
#define ENB 7   // 右轮 PWM
#define IN3 6   // 右轮方向
#define IN4 5

// ===== LEDC 通道配置（ESP32 PWM）=====
#define CH_A 0
#define CH_B 1
#define PWM_FREQ 5000
#define PWM_RES 8  // 0-255 范围

// ===== 全局变量 =====
WiFiClient client;
float currentLeftSpeed = 0;   // -255 ～ +255
float currentRightSpeed = 0;
bool isMoving = false;

// ===== 初始化 PWM =====
void setupMotorPWM() {
  ledcSetup(CH_A, PWM_FREQ, PWM_RES);
  ledcSetup(CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA, CH_A);
  ledcAttachPin(ENB, CH_B);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
}

// ===== 电机控制（使用 ledcWrite）=====
void setMotor(int motor, int speed) {
  int absSpeed = abs(speed);
  if (motor == 1) {
    ledcWrite(CH_A, absSpeed);
    digitalWrite(IN1, speed > 0 ? HIGH : LOW);
    digitalWrite(IN2, speed > 0 ? LOW : HIGH);
  } else if (motor == 2) {
    ledcWrite(CH_B, absSpeed);
    digitalWrite(IN3, speed > 0 ? HIGH : LOW);
    digitalWrite(IN4, speed > 0 ? LOW : HIGH);
  }
}

void drive(float left, float right) {
  left = constrain(left, -255, 255);
  right = constrain(right, -255, 255);
  setMotor(1, (int)left);
  setMotor(2, (int)right);

  currentLeftSpeed = left;
  currentRightSpeed = right;
  isMoving = (abs(left) > 5 || abs(right) > 5);
}

// ===== 初始化 =====
void setup() {
  Serial.begin(115200);
  setupMotorPWM();
  drive(0, 0); // 初始停止

  Serial.println("📶 正在连接热点: " + String(ssid));
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi 已连接。本机 IP: " + WiFi.localIP().toString());

  Serial.print("🔌 正在连接监控中心... ");
  while (!client.connect(monitorIP, monitorPort)) {
    delay(2000);
    Serial.print(".");
  }
  Serial.println("✅ 成功连接！");
}

// ===== 主循环 =====
void loop() {
  // 自动重连
  if (!client.connected()) {
    Serial.println("⚠️ 连接断开，尝试重连...");
    delay(2000);
    client.connect(monitorIP, monitorPort);
    return;
  }

  // === 接收控制指令 ===
  while (client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();

    if (line.indexOf("acc") >= 0 && line.indexOf("gyro") >= 0) {
      // 安全解析：查找 "acc":[-0.2,0.8,9.6]
      int ayStart = line.indexOf("acc") + 5; // 跳过 "acc":[
      int ayEnd = line.indexOf(",", ayStart);
      if (ayEnd > ayStart) {
        float ay = line.substring(ayStart, ayEnd).toFloat();

        // 查找 gy（第一个 gyro 值）
        int gyStart = line.indexOf("gyro") + 6;
        int gyEnd = line.indexOf(",", gyStart);
        if (gyEnd > gyStart) {
          float gy = line.substring(gyStart, gyEnd).toFloat();

          // 控制逻辑
          float forward = 0;
          if (ay < -0.4) {
            forward = (-ay - 0.4) * 200;
          } else if (ay > 0.6) {
            forward = -(ay - 0.6) * 150;
          }
          float turn = -gy * 1.8;

          drive(forward + turn, forward - turn);
        }
      }
    }
  }

  // === 上报小车状态（每 200ms）===
  static unsigned long lastReport = 0;
  if (millis() - lastReport >= 200) {
    // 构造标准 JSON
    String json = "{\"source\":\"car\","
                  "\"left_speed\":" + String((int)currentLeftSpeed) + ","
                  "\"right_speed\":" + String((int)currentRightSpeed) + ","
                  "\"is_moving\":" + String(isMoving ? "true" : "false") + ","
                  "\"timestamp\":" + String(millis()) +
                  "}\n";
    
    client.print(json); // 发送给监控中心
    lastReport = millis();

    // 可选：本地调试
    // Serial.print("📤 上报: "); Serial.print(json);
  }

  delay(5);
}