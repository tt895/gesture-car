// Assets/Scripts/UI/GestureCarHUD.cs
using UnityEngine;
using TMPro;
using UnityEngine.UI;

public class GestureCarHUD : MonoBehaviour
{
    // ===== 小车状态 UI =====
    [Header("小车状态")]
    public TMP_Text leftSpeedText;
    public TMP_Text rightSpeedText;
    public TMP_Text movingStatusText;
    public Image movingIndicator; // 圆形指示灯

    // ===== 手势姿态可视化 =====
    [Header("手势姿态指示器")]
    public RectTransform handTiltIndicator;     // 前后倾斜箭头/条
    public RectTransform handRotateIndicator;   // 左右旋转箭头/条
    public TMP_Text accYText;                   // ay 数值
    public TMP_Text gyroYText;                  // gy 数值

    // 可视化参数
    [Header("可视化参数")]
    public float maxTiltAngle = 45f;      // 最大前后倾斜角度（对应 ay = ±1）
    public float maxRotationRate = 200f;  // 最大角速度（对应 gy = ±200 deg/s）

    void OnEnable()
    {
        SensorDataManager.Instance.OnGestureData += UpdateHandVisual;
        SensorDataManager.Instance.OnCarStatus += UpdateCarVisual;
    }

    void OnDisable()
    {
        if (SensorDataManager.Instance != null)
        {
            SensorDataManager.Instance.OnGestureData -= UpdateHandVisual;
            SensorDataManager.Instance.OnCarStatus -= UpdateCarVisual;
        }
    }

    void UpdateHandVisual(GestureData data)
    {
        float ay = data.acc[1];   // Y 轴加速度（-1 ～ +1 g）
        float gy = data.gyro[1];  // Y 轴角速度（deg/s）

        // --- 更新数值文本 ---
        if (accYText) accYText.text = $"Ay: {ay:F2} g";
        if (gyroYText) gyroYText.text = $"Gy: {gy:F0} °/s";

        // --- 前后倾斜指示器（上下移动或旋转）---
        if (handTiltIndicator != null)
        {
            float tiltPercent = Mathf.Clamp(ay, -1f, 1f); // -1 ～ +1
            // 方法1：上下平移（推荐）
            Vector2 tiltPos = new Vector2(0, tiltPercent * 80f); // 80px 移动范围
            handTiltIndicator.anchoredPosition = tiltPos;

            // 方法2（备选）：旋转箭头
            // handTiltIndicator.localEulerAngles = new Vector3(0, 0, -tiltPercent * maxTiltAngle);
        }

        // --- 左右旋转指示器（左右移动）---
        if (handRotateIndicator != null)
        {
            float rotatePercent = Mathf.Clamp(gy / maxRotationRate, -1f, 1f);
            Vector2 rotatePos = new Vector2(rotatePercent * 80f, 0); // 左右移动
            handRotateIndicator.anchoredPosition = rotatePos;
        }
    }

    void UpdateCarVisual(CarStatus status)
    {
        if (leftSpeedText) leftSpeedText.text = $"L: {status.left_speed}";
        if (rightSpeedText) rightSpeedText.text = $"R: {status.right_speed}";
        if (movingStatusText)
        {
            movingStatusText.text = status.is_moving ? "🚗 移动中" : "🛑 停止";
            movingStatusText.color = status.is_moving ? Color.green : Color.red;
        }
        if (movingIndicator)
        {
            movingIndicator.color = status.is_moving ? Color.green : new Color(0.3f, 0.3f, 0.3f);
        }
    }
}