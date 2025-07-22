#pragma once

#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// NVS管理器初始化
esp_err_t nvs_manager_init(void);

// 获取解锁状态
bool nvs_manager_get_unlocked(void);

// 设置解锁状态
esp_err_t nvs_manager_set_unlocked(bool unlocked);

// 通用隐藏状态管理
bool nvs_manager_is_hidden(const char* key);
esp_err_t nvs_manager_set_hidden(const char* key, bool hidden);

#ifdef __cplusplus
}
#endif