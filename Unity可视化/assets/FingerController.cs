using UnityEngine;

public class FingerController : MonoBehaviour
{
    [Header("手指标识")]
    [Tooltip("必须匹配ESP32发送的名称: thumb/index/middle/ring/pinky")]
    public string fingerName = "index";

    [Header("骨骼节点")]
    public Transform joint1;  // 近端（拇指只有2个，其他3个）
    public Transform joint2;  // 中端，XXXX
    public Transform joint3;  // 远端（拇指留空）

    [Header("弯曲设置")]
    public Vector3 bendAxis = Vector3.right;
    [Range(0, 120)] public float maxBendAngle = 90f;

    [Header("弯曲比例分配")]
    [Tooltip("各关节弯曲比例，总和应为1")]
    public float[] bendRatios = new float[] { 0.33f, 0.45f, 0.22f };

    // 内部
    private Quaternion[] initialRotations;
    private Transform[] joints;
    private DataReceiver dataReceiver;
    private bool initialized = false;

    void Start()
    {
        Initialize();
    }

    void Initialize()
    {
        // 查找DataReceiver
        dataReceiver = FindObjectOfType<DataReceiver>();
        if (dataReceiver == null)
        {
            Debug.LogError($"[FingerController:{fingerName}] 找不到DataReceiver！");
            enabled = false;
            return;
        }

        // 标准化手指名称
        fingerName = fingerName.ToLower().Trim();

        // 自动查找骨骼（如果未手动指定）
        if (joint1 == null) AutoFindBones();

        // 收集有效关节
        var jointList = new System.Collections.Generic.List<Transform>();
        if (joint1 != null) jointList.Add(joint1);
        if (joint2 != null) jointList.Add(joint2);
        if (joint3 != null) jointList.Add(joint3);
        joints = jointList.ToArray();

        if (joints.Length == 0)
        {
            Debug.LogError($"[FingerController:{fingerName}] 没有有效骨骼！");
            enabled = false;
            return;
        }

        // 调整bendRatios长度匹配关节数
        if (bendRatios.Length != joints.Length)
        {
            Debug.LogWarning($"[FingerController:{fingerName}] 弯曲比例长度({bendRatios.Length})与关节数({joints.Length})不匹配，自动均分");
            bendRatios = new float[joints.Length];
            for (int i = 0; i < joints.Length; i++) bendRatios[i] = 1f / joints.Length;
        }

        // 保存初始旋转
        initialRotations = new Quaternion[joints.Length];
        for (int i = 0; i < joints.Length; i++)
        {
            initialRotations[i] = joints[i].localRotation;
        }

        initialized = true;
        Debug.Log($"[FingerController:{fingerName}] 初始化完成，{joints.Length}个关节");
    }

    void AutoFindBones()
    {
        // 向上查找手部根节点
        Transform handRoot = transform;
        while (handRoot.parent != null && !handRoot.name.ToLower().Contains("hand"))
        {
            handRoot = handRoot.parent;
        }

        // 在子物体中查找匹配名称
        Transform[] allChildren = handRoot.GetComponentsInChildren<Transform>(true);

        foreach (var child in allChildren)
        {
            string n = child.name.ToLower();

            // 匹配规则：包含手指名 + 数字
            if (n.Contains(fingerName))
            {
                if (n.Contains("1") || n.Contains("01")) joint1 = child;
                else if (n.Contains("2") || n.Contains("02")) joint2 = child;
                else if (n.Contains("3") || n.Contains("03")) joint3 = child;
            }
        }

        if (joint1 == null)
            Debug.LogWarning($"[FingerController:{fingerName}] 自动查找失败，请手动指定骨骼");
    }

    void Update()
    {
        if (!initialized || dataReceiver == null) return;

        // 从DataReceiver获取平滑百分比
        float bendPercent = dataReceiver.GetFingerPercent(fingerName);

        // 应用旋转
        ApplyBend(bendPercent);
    }

    void ApplyBend(float percent)
    {
        // percent: 0-100 -> 0-1
        float t = Mathf.Clamp01(percent / 100f);
        float totalAngle = maxBendAngle * t;

        for (int i = 0; i < joints.Length; i++)
        {
            if (joints[i] == null) continue;

            float angle = bendRatios[i] * totalAngle;
            Quaternion targetRot = initialRotations[i] * Quaternion.AngleAxis(angle, bendAxis);

            joints[i].localRotation = targetRot;
        }
    }

    // 手动测试用
    [ContextMenu("测试弯曲50%")]
    void TestBend50()
    {
        if (!initialized) Initialize();
        ApplyBend(50f);
    }

    [ContextMenu("复位")]
    void ResetPose()
    {
        if (!initialized) return;
        for (int i = 0; i < joints.Length; i++)
        {
            if (joints[i] != null) joints[i].localRotation = initialRotations[i];
        }
    }

    void OnValidate()
    {
        // 编辑器验证：确保fingerName有效
        fingerName = fingerName.ToLower().Trim();
        if (System.Array.IndexOf(new[] { "thumb", "index", "middle", "ring", "pinky" }, fingerName) < 0)
        {
            Debug.LogWarning($"[FingerController] fingerName '{fingerName}' 不是标准名称，ESP32可能无法识别");
        }
    }
}
