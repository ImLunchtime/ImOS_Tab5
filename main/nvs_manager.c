#include "nvs_manager.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>
#include <stdio.h>

#define NVS_NAMESPACE "system"
#define KEY_UNLOCKED "unlocked"

static bool g_nvs_initialized = false;

esp_err_t nvs_manager_init(void) {
    if (g_nvs_initialized) {
        return ESP_OK;
    }
    
    // 初始化NVS flash
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret == ESP_OK) {
        g_nvs_initialized = true;
        printf("NVS manager initialized\n");
    } else {
        printf("Failed to initialize NVS: %s\n", esp_err_to_name(ret));
    }
    
    return ret;
}

bool nvs_manager_get_unlocked(void) {
    if (!g_nvs_initialized) {
        return false; // 默认锁定状态
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return false;
    }
    
    uint8_t unlocked = 0;
    size_t required_size = sizeof(unlocked);
    ret = nvs_get_blob(handle, KEY_UNLOCKED, &unlocked, &required_size);
    nvs_close(handle);
    
    return (ret == ESP_OK) ? (bool)unlocked : false;
}

esp_err_t nvs_manager_set_unlocked(bool unlocked) {
    if (!g_nvs_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint8_t value = unlocked ? 1 : 0;
    ret = nvs_set_blob(handle, KEY_UNLOCKED, &value, sizeof(value));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    
    nvs_close(handle);
    return ret;
}

bool nvs_manager_is_hidden(const char* key) {
    if (!g_nvs_initialized || !key) {
        return false;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return false;
    }
    
    uint8_t hidden = 0;
    size_t required_size = sizeof(hidden);
    ret = nvs_get_blob(handle, key, &hidden, &required_size);
    nvs_close(handle);
    
    return (ret == ESP_OK) ? (bool)hidden : false;
}

esp_err_t nvs_manager_set_hidden(const char* key, bool hidden) {
    if (!g_nvs_initialized || !key) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    uint8_t value = hidden ? 1 : 0;
    ret = nvs_set_blob(handle, key, &value, sizeof(value));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    
    nvs_close(handle);
    return ret;
}