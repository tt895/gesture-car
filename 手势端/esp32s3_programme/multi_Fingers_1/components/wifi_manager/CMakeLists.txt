# WiFi管理器组件
idf_component_register(SRCS "wifi_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_netif lwip freertos)