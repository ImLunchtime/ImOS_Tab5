#pragma once

#include "app_manager.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 音频回环状态枚举
typedef enum {
    AUDIO_LOOPBACK_STATE_IDLE,      // 空闲状态
    AUDIO_LOOPBACK_STATE_ACTIVE,    // 激活状态（正在回环）
    AUDIO_LOOPBACK_STATE_ERROR      // 错误状态
} audio_loopback_state_t;

// 音频回环数据结构
typedef struct {
    audio_loopback_state_t state;   // 当前状态
    bool speaker_enabled;           // 扬声器是否启用
    bool button_pressed;            // 按钮是否按下
    uint32_t loopback_start_time;   // 回环开始时间
    uint32_t total_loopback_time;   // 总回环时间
    uint32_t loopback_count;        // 回环次数
} audio_loopback_data_t;

/**
 * @brief 注册音频回环应用
 */
void register_audio_loopback_app(void);

/**
 * @brief 检查扬声器状态
 * 
 * @param data 音频回环数据指针
 * @return true if speaker is disabled (safe to use), false otherwise
 */
bool check_speaker_status(audio_loopback_data_t* data);

/**
 * @brief 开始音频回环
 * 
 * @param data 音频回环数据指针
 * @return true if loopback started successfully
 */
bool start_audio_loopback(audio_loopback_data_t* data);

/**
 * @brief 停止音频回环
 * 
 * @param data 音频回环数据指针
 */
void stop_audio_loopback(audio_loopback_data_t* data);

/**
 * @brief 更新音频回环UI
 * 
 * @param container 应用容器
 * @param data 音频回环数据指针
 */
void update_audio_loopback_ui(lv_obj_t* container, audio_loopback_data_t* data);

#ifdef __cplusplus
}
#endif 