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
const char* ssid = "GestureCar_AP";// 热点名称
const char* password = "12345678";// 热点密码
const char* monitorIP = "192.168.4.1";// 监控中心 IP 地址
const uint16_t monitorPort = 8080;// 监控中心端口

// ===== 电机引脚（推荐使用支持 PWM 的引脚）=====
#define ENA 18  // 左轮 PWM
#define IN1 17  // 左轮正转
#define IN2 16  // 左轮反转
#define ENB 7   // 右轮 PWM
#define IN3 6   // 右轮正转
#define IN4 5   // 右轮反转

// ===== LEDC 通道配置（ESP32 PWM）=====
#define CH_A 0// 左轮通道
#define CH_B 1// 右轮通道
#define PWM_FREQ 5000// 5 kHz PWM 频率
#define PWM_RES 8  // 0-255 范围

// ===== 全局变量 =====
WiFiClient client;// TCP 客户端
float currentLeftSpeed = 0;   // -255 ～ +255
float currentRightSpeed = 0;
bool isMoving = false;

// ===== 初始化 PWM =====
void setupMotorPWM() {// 功能：配置ESP32的LEDC PWM功能
  ledcSetup(CH_A, PWM_FREQ, PWM_RES);//设置左轮PWM通道频率和分辨率
  ledcSetup(CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA, CH_A);//绑定左轮引脚连接到PWM通道
  ledcAttachPin(ENB, CH_B);

  pinMode(IN1, OUTPUT);// 设置电机控制引脚为输出模式
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
}

// ===== 电机控制（使用 ledcWrite）=====
void setMotor(int motor, int speed) {// 功能：控制单个电机的速度和方向
  int absSpeed = abs(speed);
  if (motor == 1) {// 左电机
    ledcWrite(CH_A, absSpeed);
    digitalWrite(IN1, speed > 0 ? HIGH : LOW);
    digitalWrite(IN2, speed > 0 ? LOW : HIGH);
  } else if (motor == 2) {// 右电机
    ledcWrite(CH_B, absSpeed);
    digitalWrite(IN3, speed > 0 ? HIGH : LOW);
    digitalWrite(IN4, speed > 0 ? LOW : HIGH);
  }
}

void drive(float left, float right) {//同时控制左右电机
  left = constrain(left, -255, 255);//用constrain限制速度范围
  right = constrain(right, -255, 255);
  setMotor(1, (int)left);//调用setMotor()设置左右电机
  setMotor(2, (int)right);

  currentLeftSpeed = left;
  currentRightSpeed = right;
  isMoving = (abs(left) > 5 || abs(right) > 5);// 判断是否在运动，v 》5即认为在运动
}

// ===== 初始化 =====
void setup() {
  Serial.begin(115200);// 初始化串口，建立与PC的串口通信，用于调试信息输出
  setupMotorPWM();// 初始化电机 PWM
  drive(0, 0); // 初始为停止

  Serial.println("📶 正在连接热点: " + String(ssid));
  WiFi.begin(ssid, password);// 连接 Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi 已连接。本机 IP: " + WiFi.localIP().toString());// 输出本机 IP 地址

  Serial.print("🔌 正在连接监控中心... ");
  while (!client.connect(monitorIP, monitorPort)) {// 连接监控中心
    delay(2000);// 每 2 秒重试一次
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

  // === 接收手势控制指令 ===
  while (client.available()) {// 有数据可读
    String line = client.readStringUntil('\n');// 读取一行数据
    line.trim();// 清理数据（去除首尾空格/换行）

    if (line.indexOf("acc") >= 0 && line.indexOf("gyro") >= 0) {//检查是否包含 "acc" 和 "gyro" 字段
      // 安全解析：查找 "acc":[-0.2,0.8,9.6]
      // 定位 ay 值的开始和结束位置
      int ayStart = line.indexOf("acc") + 5; // 跳过 "acc":[ 这五个字节
      int ayEnd = line.indexOf(",", ayStart);// 找到第一个逗号
      if (ayEnd > ayStart) {
        float ay = line.substring(ayStart, ayEnd).toFloat();// 提取 ay 值
        // 查找 gy（第一个 gyro 值）
        int gyStart = line.indexOf("gyro") + 6;
        int gyEnd = line.indexOf(",", gyStart);
        if (gyEnd > gyStart) {
          float gy = line.substring(gyStart, gyEnd).toFloat();

          // 控制逻辑
          float forward = 0;
          if (ay < -0.4) {// 设备前倾
            forward = (-ay - 0.4) * 200;// 前进 //速度 = (倾斜程度 - 阈值) × 增益
          } else if (ay > 0.6) {// 设备后倾
            forward = -(ay - 0.6) * 150;// 后退
          }
          float turn = -gy * 1.8;// 设备左倾时 gy 为正，右转

          drive(forward + turn, forward - turn);// 计算左右轮速度并驱动 //左轮速度 = 前进速度 + 转向速度  右轮速度 = 前进速度 - 转向速度

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
                  "\"is_moving\":" + String(isMoving ? "true" : "false") + ","//运动状态标志
                  "\"timestamp\":" + String(millis()) + // 时间戳，监控端计算数据延迟，判断数据新鲜度


                  "}\n";
    
    client.print(json); // 发送给监控中心
    lastReport = millis();

    // 可选：本地调试
    // Serial.print("📤 上报: "); Serial.print(json);
  }

  delay(5);// 稍作延迟，避免占用过多 CPU
}

//Wi-Fi通信模块 - 连接热点并通信
//电机控制模块 - 驱动小车运动
//指令解析模块 - 解析手势控制数据
//状态上报模块 - 定期发送状态到监控中心