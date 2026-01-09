/*
 * 监控中心 ESP32 —— 串口中心 + 双向数据中转
 * 功能：
 *   - 创建热点 "GestureCar_AP"
 *   - 接收手势端控制数据 & 小车状态数据
 *   - 所有有效 JSON 数据均通过 USB 串口输出给 Unity
 *   - 控制指令（含 acc/gyro）会转发给小车（避免小车收到自己的状态）
 */

#include <WiFi.h>

const char* ap_ssid = "GestureCar_AP";
const char* ap_password = "12345678";
const uint16_t SERVER_PORT = 8080;
const int MAX_CLIENTS = 2; // 手势端 + 小车

WiFiServer server(SERVER_PORT);
WiFiClient clients[MAX_CLIENTS];
bool clientConnected[MAX_CLIENTS] = {false};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== 监控中心（双向串口模式）启动 ===");

  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("✅ 热点 IP: ");
  Serial.println(WiFi.softAPIP());

  server.begin();
  Serial.println("📡 等待设备连接...");
}

int findFreeSlot() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!clientConnected[i]) return i;
  }
  return -1;
}

void cleanupClients() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clientConnected[i] && !clients[i].connected()) {
      Serial.println("📤 客户端 " + String(i) + " 断开");
      clientConnected[i] = false;
      clients[i].stop();
    }
  }
}

void loop() {
  if (server.hasClient()) {
    int slot = findFreeSlot();
    if (slot >= 0) {
      clients[slot] = server.available();
      clientConnected[slot] = true;
      Serial.println("✅ 新客户端连接 (" + String(slot) + ")");
    } else {
      WiFiClient rejected = server.available();
      rejected.stop();
      Serial.println("⚠️ 客户端数已达上限");
    }
  }

  cleanupClients();

  // 遍历所有客户端，读取数据
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clientConnected[i] && clients[i].available()) {
      String data = clients[i].readStringUntil('\n');
      data.trim();

      if (data.length() > 0 && data.startsWith("{") && data.endsWith("}")) {
        // 👇 所有合法 JSON 数据都通过串口发给 Unity
        Serial.println(data);

        // 判断是否为控制指令（来自手势端）
        if (data.indexOf("acc") >= 0 || data.indexOf("gyro") >= 0) {
          // 转发给小车（假设小车是另一个客户端）
          for (int j = 0; j < MAX_CLIENTS; j++) {
            if (clientConnected[j] && j != i) {
              clients[j].println(data);
            }
          }
        }
        // 如果是小车状态（{"source":"car",...}），不广播，只串口输出
      }
    }
  }

  delay(10);
}