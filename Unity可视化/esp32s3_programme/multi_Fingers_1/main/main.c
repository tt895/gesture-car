#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "wifi_manager.h"

// ==================== 配置参数 ====================
#define WIFI_SSID           "tang."
#define WIFI_PASS           "13983039985"
#define TCP_PORT            8888
#define ADC_UNIT            ADC_UNIT_1

// 手指数量与通道配置（避开GPIO0/STRAPPING引脚）
#define FINGER_COUNT        5
// 降噪与发送参数
#define ADC_THRESHOLD       15      // 死区阈值
#define SEND_INTERVAL_MS    80      // 基础发送间隔

static const char *TAG = "MultiFinger";

// ADC句柄
adc_oneshot_unit_handle_t adc1_handle = NULL;

// 手指数据结构
typedef struct {
    const char *name;
    adc_channel_t channel;
    int min_val;        // 伸直校准值（需根据实际调整）
    int max_val;        // 弯曲校准值（需根据实际调整）
    int last_sent;      // 上次发送的原始值
} finger_data_t;

// 手指配置（名称和校准值）
static finger_data_t fingers[FINGER_COUNT] = {
    {"thumb",  ADC_CHANNEL_1, 1800, 3000, -1},
    {"index",  ADC_CHANNEL_2, 1800, 3000, -1},
    {"middle", ADC_CHANNEL_3, 1800, 3000, -1},
    {"ring",   ADC_CHANNEL_4, 1800, 3000, -1},
    {"pinky",  ADC_CHANNEL_5, 1800, 3000, -1},
};

// ==================== 工具函数 ====================

// 计算弯曲百分比（带限幅）
static int calc_bend_percent(int raw_val, int min_val, int max_val)
{
    if (raw_val <= min_val) return 0;
    
    int percent = (raw_val - min_val) * 100 / (max_val - min_val);
    if (percent > 100) percent = 100;
    if (percent < 5) percent = 0;  // 消除微小抖动
    return percent;
}

// 确保完整发送数据
static int send_all(int sock, const char *data, int len)
{
    int total_sent = 0;
    while (total_sent < len) {
        int sent = wifi_manager_send_tcp_data(sock, data + total_sent, len - total_sent);
        if (sent < 0) {
            return -1;  // 发送失败，连接断开
        }
        total_sent += sent;
    }
    return total_sent;
}

// ==================== 传感器任务 ====================

void sensor_task(void *pvParameters)
{
    int client_sock = (intptr_t)pvParameters;
    int raw_values[FINGER_COUNT];
    int bend_percents[FINGER_COUNT];
    
    ESP_LOGI(TAG, "多指传感器任务启动，socket=%d", client_sock);
    
    while (1) {
        bool has_significant_change = false;
        
        // 1. 读取所有手指ADC值
        for (int i = 0; i < FINGER_COUNT; i++) {
            esp_err_t ret = adc_oneshot_read(adc1_handle, fingers[i].channel, &raw_values[i]);
            
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "%s 读取失败: %s", fingers[i].name, esp_err_to_name(ret));
                raw_values[i] = fingers[i].last_sent;  // 保持上次值
                continue;
            }
            
            // 检查死区
            if (abs(raw_values[i] - fingers[i].last_sent) >= ADC_THRESHOLD || 
                fingers[i].last_sent == -1) {
                has_significant_change = true;
            }
        }
        
        // 2. 无显著变化时快速轮询，不发送
        if (!has_significant_change) {
            vTaskDelay(30 / portTICK_PERIOD_MS);
            continue;
        }
        
        // 3. 计算所有手指弯曲度（即使某指变化小也一起发，保持数据同步）
        char packet[256] = {0};
        int offset = 0;
        
        for (int i = 0; i < FINGER_COUNT; i++) {
            // 更新last_sent（只更新变化大的，但计算时都用当前值）
            if (abs(raw_values[i] - fingers[i].last_sent) >= ADC_THRESHOLD ||
                fingers[i].last_sent == -1) {
                fingers[i].last_sent = raw_values[i];
            }
            
            bend_percents[i] = calc_bend_percent(raw_values[i], fingers[i].min_val, fingers[i].max_val);
            
            // 格式: "name:raw:percent;" （最后一个分号后面接换行）
            offset += snprintf(packet + offset, sizeof(packet) - offset,
                             "%s:%d:%d;", 
                             fingers[i].name, raw_values[i], bend_percents[i]);
        }
        
        // 追加换行符作为帧结束标记
        strcat(packet, "\n");
        
        // 4. 发送数据包
        if (send_all(client_sock, packet, strlen(packet)) < 0) {
            ESP_LOGW(TAG, "发送失败，客户端断开");
            goto cleanup;
        }
        
        ESP_LOGI(TAG, "发送: %s", packet);
        
        // 5. 延时控制发送频率
        vTaskDelay(SEND_INTERVAL_MS / portTICK_PERIOD_MS);
    }
    
cleanup:
    wifi_manager_close_socket(client_sock);
    // 重置last_sent，下次连接重新校准基准
    for (int i = 0; i < FINGER_COUNT; i++) {
        fingers[i].last_sent = -1;
    }
    vTaskDelete(NULL);
}

// ==================== 主函数 ====================

void app_main(void)
{
    ESP_LOGI(TAG, "多指弯曲检测系统启动");
    
    // 1. 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 2. 初始化ADC单元
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));
    
    // 3. 配置所有手指通道
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    
    for (int i = 0; i < FINGER_COUNT; i++) {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, fingers[i].channel, &chan_cfg));
        ESP_LOGI(TAG, "配置通道: %s -> ADC1_CH%d", 
                 fingers[i].name, fingers[i].channel);
    }
    
    ESP_LOGI(TAG, "ADC初始化完成，死区阈值=%d", ADC_THRESHOLD);
    
    // 4. 初始化WiFi
    if (!wifi_manager_init_sta(WIFI_SSID, WIFI_PASS)) {
        ESP_LOGE(TAG, "WiFi初始化失败");
        return;
    }
    
    ESP_LOGI(TAG, "等待WiFi连接...");
    if (!wifi_manager_wait_connected(10000)) {
        ESP_LOGE(TAG, "WiFi连接超时");
        return;
    }
    
    const char* ip = wifi_manager_get_ip();
    if (ip) {
        ESP_LOGI(TAG, "IP地址: %s", ip);
    }
    
    // 5. 创建TCP服务器
    int server_sock = wifi_manager_create_tcp_server(TCP_PORT);
    if (server_sock < 0) {
        ESP_LOGE(TAG, "创建TCP服务器失败");
        return;
    }
    
    ESP_LOGI(TAG, "等待客户端连接...");
    
    // 6. 主循环：接受客户端连接
    while (1) {
        int client_sock = wifi_manager_wait_tcp_client(server_sock, 0);
        
        if (client_sock >= 0) {
            ESP_LOGI(TAG, "客户端已连接，启动传感器任务");
            
            intptr_t sock_param = client_sock;
            
            if (xTaskCreate(sensor_task, "sensor_task", 4096, 
                           (void*)sock_param, 5, NULL) != pdPASS) {
                ESP_LOGE(TAG, "创建传感器任务失败");
                wifi_manager_close_socket(client_sock);
            }
        }
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    // 清理（理论上不会执行到这里）
    wifi_manager_close_socket(server_sock);
    if (adc1_handle != NULL) {
        adc_oneshot_del_unit(adc1_handle);
    }
}