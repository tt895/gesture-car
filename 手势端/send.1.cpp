/*
 * 【发送端：体感遥控器】
 * 对应开发板：COM6 (MAC: F4:2D:C9:71:EB:A6)
 * 硬件：普通 ESP32 + MPU6050 + 弯曲传感器
 * 关键修改：弯曲传感器移至 GPIO 34 (ADC1)，避免蓝牙冲突
 */

#include <Wire.h>  // I2C 库（MPU6050 需要）
#include <Adafruit_MPU6050.h>  // MPU6050 驱动库
#include <Adafruit_Sensor.h>  // Adafruit 传感器库
#include "BluetoothSerial.h"  // ESP32 蓝牙串口库

// ===== 1. 目标小车 MAC 地址 (COM5) =====
uint8_t address[6] = {0xD8, 0xBC, 0x38, 0xFB, 0x1E, 0xEE};

// ===== 2. 传感器引脚 =====
// 警告：开启蓝牙后，只能使用 ADC1 引脚 (GPIO 32-39)
// 不要使用 GPIO 15, 4, 2 等
#define FLEX_PIN 34 

// 弯曲传感器阈值 (需要根据串口监视器读数微调)
// 伸直时读数 (停止)
int flexMin = 1800; 
// 弯曲时读数 (全速)
int flexMax = 3000; 

// 对象初始化
BluetoothSerial SerialBT;  // 蓝牙串口对象
Adafruit_MPU6050 mpu;  // MPU6050 传感器对象
bool connected = false;  // 连接状态标志

void setup() {
  //用于调试的初始化串口监视器
  Serial.begin(115200);

  // 初始化 MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 初始化失败！请检查 SDA(21)/SCL(22)");
    while (1) delay(10);  //卡死在这里就可能是硬件问题
  }
  // 配置 MPU6050 参数
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G); // ±8g
  mpu.setGyroRange(MPU6050_RANGE_500_DEG); // 陀螺仪±500°/s
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); // 低通滤波器 21Hz（小车端能识别的阈值，可以防止轻微抖动的干扰）

  // 初始化蓝牙 (主模式)
  SerialBT.begin("ESP32-Remote", true); // 第二个参数 true 表示主机模式，可主动连接其他设备
  Serial.println(">>> 遥控器蓝牙启动 (COM6) <<<");
  
  // 连接小车
  connectToCar();
}

// 连接小车函数
void connectToCar() {
  if (connected) return; // 已连接则跳过

  Serial.print("正在连接小车 MAC: D8:BC:38:FB:1E:EE ... ");
  // 通过 MAC 地址定向连接，速度快且稳定
  connected = SerialBT.connect(address);
  
  if(connected) {
    Serial.println("✅ 连接成功！");
  } else {
    Serial.println("❌ 连接失败，请确保小车已通电，5秒后重试");
    delay(5000);
  }
}

void loop() {
  // 断线重连
  if (!connected) {
    connectToCar();
    if (!connected) return; // 连接失败则跳过本次循环
  }

  // ===== 1. 读取传感器 =====
  sensors_event_t a, g, temp; // MPU6050 传感器数据容器
  mpu.getEvent(&a, &g, &temp); // 读取 MPU6050 数据

  // 读取弯曲度
  int flexValue = analogRead(FLEX_PIN);

  // ===== 2. 姿态解算 (简易版) =====
  // 俯仰角 (前后)
  float pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 57.3;
  // 横滚角 (左右)
  float roll = atan2(a.acceleration.y, a.acceleration.z) * 57.3;

  // ===== 3. 控制逻辑融合 =====
  
  // 步骤A: 计算速度 (由弯曲传感器控制)
  // 如果传感器数值反了(弯曲变小)，交换 flexMin 和 flexMax 的位置
  int speed = map(flexValue, flexMin, flexMax, 0, 255);
  speed = constrain(speed, 0, 255);

  // 设置死区：如果弯曲度很小，就当做停止
  if (speed < 40) speed = 0;

  // 步骤B: 判断方向 (由 MPU6050 控制)
  char cmd = 'F'; // 默认停止

  if (speed > 0) { // 只有手弯曲了(有速度)才判断方向
    if (pitch < -20) {
      cmd = 'F'; // 手掌前倾 -> 前进
    } else if (pitch > 20) {
      cmd = 'B'; // 手掌后仰 -> 后退
    } else if (roll > 20) {
      cmd = 'R'; // 手掌右翻 -> 右转
    } else if (roll < -20) {
      cmd = 'L'; // 手掌左翻 -> 左转
    } else {
      cmd = 'S'; // 手平放 -> 停止 (或者你可以设为 'F' 直行，看习惯)
    }
  }

  // ===== 4. 发送指令 =====
  // 格式: "Cmd,Speed\n"
  // 注意：发送频率不要太快，100ms 一次足够流畅
  SerialBT.printf("%c,%d\n", cmd, speed);

  // 调试打印 (建议在调整阈值时观察)
  Serial.printf("Flex:%d (Spd:%d) | Pitch:%.1f Roll:%.1f -> Cmd:%c\n", flexValue, speed, pitch, roll, cmd);

  delay(100); // 控制发送频率（10Hz）,避免数据过载
}