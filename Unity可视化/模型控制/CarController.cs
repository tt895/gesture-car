// Assets/Scripts/ModelController/CarController.cs
using UnityEngine;

public class CarController : MonoBehaviour
{
    [Tooltip("小车模型的根 GameObject")]
    public Transform carModel;

    [Header("轮子参考（可选）")]
    public Transform leftWheel;
    public Transform rightWheel;

    [Header("运动参数")]
    public float maxSpeed = 5f;      // m/s
    public float wheelRadius = 0.1f; // 米

    private Vector3 startPos;

    void Awake()
    {
        startPos = carModel.position;
    }

    void OnEnable()
    {
        SensorDataManager.Instance.OnCarStatus += MoveCar;
    }

    void OnDisable()
    {
        if (SensorDataManager.Instance != null)
            SensorDataManager.Instance.OnCarStatus -= MoveCar;
    }

    void MoveCar(CarStatus status)
    {
        if (carModel == null) return;

        // 将 PWM 值 (-255～255) 映射到物理速度 (m/s)
        float leftSpeed = Mathf.Clamp(status.left_speed / 255f, -1f, 1f) * maxSpeed;
        float rightSpeed = Mathf.Clamp(status.right_speed / 255f, -1f, 1f) * maxSpeed;

        // 计算前进方向（平均速度）
        float forwardSpeed = (leftSpeed + rightSpeed) * 0.5f;
        carModel.Translate(Vector3.forward * forwardSpeed * Time.deltaTime, Space.Self);

        // 计算转向（差速）
        float turnSpeed = (rightSpeed - leftSpeed) * 2f; // 放大转向效果
        carModel.Rotate(Vector3.up, turnSpeed * Time.deltaTime);

        // 【可选】旋转轮子（视觉反馈）
        if (leftWheel != null)
            leftWheel.Rotate(Vector3.right, -leftSpeed / wheelRadius * Time.deltaTime * Mathf.Rad2Deg);
        if (rightWheel != null)
            rightWheel.Rotate(Vector3.right, -rightSpeed / wheelRadius * Time.deltaTime * Mathf.Rad2Deg);
    }

    // 按 R 重置位置（调试用）
    void Update()
    {
        if (Input.GetKeyDown(KeyCode.R))
        {
            carModel.position = startPos;
            carModel.rotation = Quaternion.identity;
        }
    }
}