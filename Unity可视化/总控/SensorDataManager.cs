// Assets/Scripts/Serial/SensorDataManager.cs
using UnityEngine;
using System;

[System.Serializable]
public class GestureData
{
    public float[] acc = new float[3];
    public float[] gyro = new float[3];
}

[System.Serializable]
public class CarStatus
{
    public string source; // 应为 "car"
    public int left_speed;
    public int right_speed;
    public bool is_moving;
    public long timestamp;
}

public class SensorDataManager : MonoBehaviour
{
    public static SensorDataManager Instance;

    // 事件：供其他脚本订阅
    public event Action<GestureData> OnGestureData;
    public event Action<CarStatus> OnCarStatus;

    void Awake()
    {
        if (Instance == null) Instance = this;
        else Destroy(gameObject);
    }

    void OnEnable()
    {
        SerialDataHub.Instance.OnRawDataReceived += HandleRawData;
    }

    void OnDisable()
    {
        if (SerialDataHub.Instance != null)
            SerialDataHub.Instance.OnRawDataReceived -= HandleRawData;
    }

    void HandleRawData(string json)
    {
        try
        {
            // 判断数据类型
            if (json.Contains("\"source\":\"car\""))
            {
                var status = JsonUtility.FromJson<CarStatus>(json);
                OnCarStatus?.Invoke(status);
            }
            else if (json.Contains("acc") && json.Contains("gyro"))
            {
                var gesture = JsonUtility.FromJson<GestureData>(json);
                OnGestureData?.Invoke(gesture);
            }
        }
        catch (Exception e)
        {
            Debug.LogWarning($"JSON 解析失败: {json}\n{e.Message}");
        }
    }
}