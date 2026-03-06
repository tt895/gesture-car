using System;
using System.Collections;
using System.Collections.Generic;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using UnityEngine;

public class DataReceiver : MonoBehaviour
{
    [Header("网络设置")]
    public string esp32IP = "192.168.10.128";
    public int port = 8888;

    [Header("平滑设置")]
    [Range(0.01f, 0.5f)] public float smoothSpeed = 0.15f;

    // 线程安全数据容器（替代原smoothedADC）XXXX
    private ThreadSafeFingerData fingerData = new ThreadSafeFingerData();

    // 网络线程
    private Thread receiveThread;
    private bool isRunning = false;

    // 粘包处理：使用StringBuilder作为跨帧缓冲区
    private readonly StringBuilder receiveBuffer = new StringBuilder();

    void Start()
    {
        StartTCPThread();
    }

    void StartTCPThread()
    {
        isRunning = true;
        receiveThread = new Thread(ReceiveData);
        receiveThread.IsBackground = true;
        receiveThread.Start();
    }

    void ReceiveData()
    {
        Debug.Log("[DataReceiver] 连接线程启动...");

        while (isRunning)
        {
            TcpClient client = null;
            NetworkStream stream = null;

            try
            {
                client = new TcpClient();
                client.Connect(esp32IP, port);
                stream = client.GetStream();

                // 设置读写超时，便于检测断开
                client.ReceiveTimeout = 5000;
                client.SendTimeout = 5000;

                Debug.Log("[DataReceiver] TCP连接成功");

                byte[] buffer = new byte[256];  // 加大缓冲区应对多指数据

                while (isRunning && client.Connected)
                {
                    int bytes = 0;
                    try
                    {
                        bytes = stream.Read(buffer, 0, buffer.Length);
                    }
                    catch (System.IO.IOException)
                    {
                        // 超时或断开
                        break;
                    }

                    if (bytes == 0) break;  // 正常断开

                    // 追加到缓冲区并处理
                    string received = Encoding.UTF8.GetString(buffer, 0, bytes);
                    lock (receiveBuffer)
                    {
                        receiveBuffer.Append(received);
                        ProcessBuffer();
                    }
                }

                Debug.Log("[DataReceiver] 连接断开，3秒后重连...");
            }
            catch (Exception e)
            {
                Debug.LogError($"[DataReceiver] 网络错误: {e.Message}");
            }
            finally
            {
                stream?.Close();
                client?.Close();
            }

            // 重连延时
            Thread.Sleep(3000);
        }
    }

    void ProcessBuffer()
    {
        string data = receiveBuffer.ToString();
        int newlineIndex;

        // 循环处理所有完整帧（以\n分隔）
        while ((newlineIndex = data.IndexOf('\n')) != -1)
        {
            // 提取一行（不含\n）
            string line = data.Substring(0, newlineIndex).Trim();

            // 从缓冲区移除这行（包括\n）
            receiveBuffer.Remove(0, newlineIndex + 1);

            // 更新data用于下次循环判断
            data = receiveBuffer.ToString();

            if (string.IsNullOrEmpty(line)) continue;

            // 解析多指数据: "thumb:1850:5;index:2100:23;..."
            ParseFingerFrame(line);
        }

        // 防止缓冲区无限增长（异常数据堆积）
        if (receiveBuffer.Length > 4096)
        {
            Debug.LogWarning("[DataReceiver] 缓冲区溢出，清空");
            receiveBuffer.Clear();
        }
    }

    void ParseFingerFrame(string frame)
    {
        // 按;分割手指
        string[] fingers = frame.Split(';');

        foreach (string fingerStr in fingers)
        {
            if (string.IsNullOrEmpty(fingerStr)) continue;

            // 按:分割 name:raw:percent
            string[] parts = fingerStr.Split(':');
            if (parts.Length != 3) continue;  // 格式错误，跳过

            string name = parts[0].Trim().ToLower();  // 统一小写
            if (!int.TryParse(parts[1], out int raw)) continue;
            if (!int.TryParse(parts[2], out int percent)) continue;

            // 限幅
            percent = Mathf.Clamp(percent, 0, 100);

            // 更新线程安全容器
            fingerData.UpdateFinger(name, raw, percent);
        }
    }

    // 对外接口：获取手指平滑后的弯曲百分比
    public float GetFingerPercent(string fingerName)
    {
        return fingerData.GetSmoothedPercent(fingerName.ToLower(), smoothSpeed);
    }

    // 对外接口：获取原始ADC（调试）
    public int GetRawADC(string fingerName)
    {
        return fingerData.GetRawADC(fingerName.ToLower());
    }

    // 对外接口：检查手指是否在线
    public bool IsFingerActive(string fingerName)
    {
        return !fingerData.IsTimeout(fingerName.ToLower());
    }

    // 获取所有已知手指
    public string[] GetAllFingers()
    {
        return fingerData.GetFingerNames();
    }

    void OnGUI()
    {
        bool connected = receiveThread?.IsAlive == true &&
                        receiveThread.ThreadState != ThreadState.WaitSleepJoin;

        GUI.color = connected ? Color.green : Color.red;
        GUI.Label(new Rect(10, 10, 250, 25),
                 $"网络: {(connected ? "已连接" : "未连接")}");

        // 显示各手指状态
        GUI.color = Color.white;
        int y = 35;
        string[] fingers = new[] { "thumb", "index", "middle", "ring", "pinky" };

        foreach (var f in fingers)
        {
            float pct = GetFingerPercent(f);
            bool active = IsFingerActive(f);
            GUI.color = active ? Color.white : Color.gray;
            GUI.Label(new Rect(10, y, 300, 25),
                     $"{f}: {pct:F1}% {(active ? "" : "(超时)")}");
            y += 25;
        }
    }

    void OnDestroy()
    {
        isRunning = false;
        receiveThread?.Join(1000);
    }
}