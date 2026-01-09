// Assets/Scripts/ModelController/HandController.cs
using UnityEngine;

public class HandController : MonoBehaviour
{
    [Tooltip("手模型的根 GameObject")]
    public Transform handModel;

    [Header("灵敏度调节")]
    public float tiltSensitivity = 30f;   // 前后倾斜
    public float rotateSensitivity = 50f; // 左右旋转

    void OnEnable()
    {
        SensorDataManager.Instance.OnGestureData += UpdateHandPose;
    }

    void OnDisable()
    {
        if (SensorDataManager.Instance != null)
            SensorDataManager.Instance.OnGestureData -= UpdateHandPose;
    }

    void UpdateHandPose(GestureData data)
    {
        if (handModel == null) return;

        // 使用 acc_y 控制前后倾斜（俯仰）
        float pitch = Mathf.Clamp(data.acc[1], -1f, 1f); // ay
        float tiltAngle = pitch * tiltSensitivity;

        // 使用 gyro_y 控制左右旋转（偏航）
        float yawRate = data.gyro[1]; // gy
        float currentYaw = handModel.localEulerAngles.y;
        float newYaw = currentYaw + yawRate * rotateSensitivity * Time.deltaTime;

        // 应用旋转（绕 X 轴倾斜，绕 Y 轴旋转）
        handModel.localRotation = Quaternion.Euler(tiltAngle, newYaw, 0);
    }
}