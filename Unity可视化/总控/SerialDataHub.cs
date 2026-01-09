// Assets/Scripts/Serial/SerialDataHub.cs
using UnityEngine;
using System.IO.Ports;
using System;

public class SerialDataHub : MonoBehaviour
{
    public static SerialDataHub Instance;

    [Header("串口设置")]
    public string comPort = "COM5"; // 👈 根据你的 ESP32 修改！
    public int baudRate = 115200;

    private SerialPort serialPort;

    // 所有原始 JSON 数据从此事件广播
    public event Action<string> OnRawDataReceived;

    void Awake()
    {
        if (Instance == null) Instance = this;
        else Destroy(gameObject);
        DontDestroyOnLoad(gameObject);
    }

    void Start()
    {
        OpenSerialPort();
    }

    void Update()
    {
        if (serialPort?.IsOpen == true)
        {
            try
            {
                while (serialPort.BytesToRead > 0)
                {
                    string line = serialPort.ReadLine().Trim();
                    if (!string.IsNullOrEmpty(line))
                    {
                        OnRawDataReceived?.Invoke(line);
                    }
                }
            }
            catch (Exception e)
            {
                Debug.LogError($"串口读取错误: {e.Message}");
            }
        }
    }

    void OnDestroy()
    {
        if (serialPort != null && serialPort.IsOpen)
        {
            serialPort.Close();
            serialPort.Dispose();
        }
    }

    void OpenSerialPort()
    {
        try
        {
            serialPort = new SerialPort(comPort, baudRate)
            {
                ReadTimeout = 50
            };
            serialPort.Open();
            Debug.Log($"✅ 成功连接监控端串口: {comPort}");
        }
        catch (Exception e)
        {
            Debug.LogError($"❌ 无法打开串口 {comPort}: {e.Message}");
        }
    }
}