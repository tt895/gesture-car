using System;
using System.Collections.Generic;

[Serializable]
public class FingerData
{
    public string name;//
    public int rawADC;
    public int bendPercent;  // 0-100
    public long timestamp;   // 毫秒时间戳，用于超时检测

    public FingerData(string name)
    {
        this.name = name;
        this.rawADC = 0;
        this.bendPercent = 0;
        this.timestamp = 0;
    }
}

// 线程安全的手指数据容器
public class ThreadSafeFingerData
{
    private readonly object lockObj = new object();
    private Dictionary<string, FingerData> data = new Dictionary<string, FingerData>();
    private Dictionary<string, float> smoothedPercent = new Dictionary<string, float>();

    // 写入（仅接收线程调用）
    public void UpdateFinger(string name, int raw, int percent)
    {
        lock (lockObj)
        {
            if (!data.ContainsKey(name))
                data[name] = new FingerData(name);

            data[name].rawADC = raw;
            data[name].bendPercent = percent;
            data[name].timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
        }
    }

    // 读取平滑值（仅主线程调用）
    public float GetSmoothedPercent(string name, float smoothSpeed)
    {
        lock (lockObj)
        {
            if (!data.ContainsKey(name)) return 0f;

            int target = data[name].bendPercent;

            if (!smoothedPercent.ContainsKey(name))
                smoothedPercent[name] = target;

            // Lerp计算
            smoothedPercent[name] = smoothedPercent[name] * (1 - smoothSpeed) + target * smoothSpeed;

            return smoothedPercent[name];
        }
    }

    // 获取原始值（调试用）
    public int GetRawADC(string name)
    {
        lock (lockObj)
        {
            return data.ContainsKey(name) ? data[name].rawADC : 0;
        }
    }

    // 检查是否超时（超过500ms无数据）
    public bool IsTimeout(string name)
    {
        lock (lockObj)
        {
            if (!data.ContainsKey(name)) return true;
            long now = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
            return (now - data[name].timestamp) > 500;
        }
    }

    // 获取所有手指名称
    public string[] GetFingerNames()
    {
        lock (lockObj)
        {
            string[] names = new string[data.Count];
            data.Keys.CopyTo(names, 0);
            return names;
        }
    }
}
