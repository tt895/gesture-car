/*
 * 【接收端：小车】
 * 对应开发板：COM5 (MAC: D8:BC:38:FB:1E:EE)
 * 硬件：普通 ESP32 + L298N
 * 功能：接收蓝牙指令控制电机
 */

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error 蓝牙未启用！请检查开发板配置
#endif

BluetoothSerial SerialBT;

// ===== 电机引脚定义 (L298N) =====
#define ENA 14
#define IN1 27
#define IN2 26
#define IN3 25
#define IN4 33
#define ENB 32

// 全局变量
long lastCmdTime = 0;
bool isConnected = false;  // 连接状态标志

void setup() {
  Serial.begin(115200);

  // 初始化电机引脚
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  stopMotors();

  // 开启蓝牙 (设备名)
  SerialBT.begin("ESP32-Car"); 
  
  // 设置蓝牙连接状态回调
  SerialBT.register_callback(bluetoothCallback);
  
  Serial.println(">>> 小车端蓝牙已启动 (COM5) <<<");
  Serial.println("设备名称: ESP32-Car");
  Serial.println("等待遥控器连接...");
}

// 蓝牙连接状态回调函数
void bluetoothCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVENT) {
    isConnected = true;
    Serial.println(">>> 蓝牙已连接!");
    Serial.println(">>> 连接成功，等待指令...");
    
    // 发送连接成功回执信息给遥控器
    SerialBT.println("[Car] CONNECTED");
    delay(50);  // 短暂延迟确保消息发送
    SerialBT.println("[Car] 小车就绪，开始遥控!");
    
  } else if (event == ESP_SPP_CLOSE_EVENT) {
    isConnected = false;
    Serial.println(">>> 蓝牙已断开连接!");
    Serial.println(">>> 等待重新连接...");
  }
}

void loop() {
  // 检测连接状态并发送心跳包（可选）
  static unsigned long lastHeartbeat = 0;
  if (isConnected && millis() - lastHeartbeat > 3000) {
    SerialBT.println("[Car] HEARTBEAT");
    lastHeartbeat = millis();
  }
  
  if (SerialBT.available()) {
    // 读取数据格式: "F,200" (指令,速度)
    String packet = SerialBT.readStringUntil('\n');
    packet.trim(); // 去除多余空格和换行

    if (packet.length() > 0) {
      int commaIndex = packet.indexOf(',');
      if (commaIndex != -1) {
        char cmd = packet.charAt(0);
        int speed = packet.substring(commaIndex + 1).toInt();
        
        // 执行
        executeCommand(cmd, speed);
        lastCmdTime = millis();
        
        // 可选：发送确认回执
        if (isConnected) {
          String ack = String("[Car] CMD:") + cmd + ",SPD:" + speed;
          SerialBT.println(ack);
        }
      }
    }
  }

  // 【安全保护】如果 500ms 没收到指令，自动停车
  // 防止信号丢失后小车一直跑
  if (millis() - lastCmdTime > 500) {
    stopMotors();
  }
}

// ===== 电机动作 =====
void executeCommand(char cmd, int speed) {
  speed = constrain(speed, 0, 255); // 限制 PWM 范围

  switch (cmd) {
    case 'F': // 前进
      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
      analogWrite(ENA, speed); analogWrite(ENB, speed);
      Serial.print("前进 - 速度: "); Serial.println(speed);
      break;
    case 'B': // 后退
      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
      analogWrite(ENA, speed); analogWrite(ENB, speed);
      Serial.print("后退 - 速度: "); Serial.println(speed);
      break;
    case 'L': // 左转 (原地)
      digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
      digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
      analogWrite(ENA, speed); analogWrite(ENB, speed);
      Serial.print("左转 - 速度: "); Serial.println(speed);
      break;
    case 'R': // 右转 (原地)
      digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
      analogWrite(ENA, speed); analogWrite(ENB, speed);
      Serial.print("右转 - 速度: "); Serial.println(speed);
      break;
    case 'S': // 停止
      stopMotors();
      Serial.println("停止");
      break;
  }
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENA, 0); analogWrite(ENB, 0);
}