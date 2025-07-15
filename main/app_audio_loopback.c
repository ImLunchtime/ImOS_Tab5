#include "app_audio_loopback.h"
#include "app_manager.h"
#include "hal_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

// 声明自定义字体
LV_FONT_DECLARE(simhei_32);

// 高DPI屏幕尺寸 (1280x720)
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// UI 元素尺寸 (为高DPI屏幕优化)
#define TITLE_HEIGHT 80
#define BUTTON_HEIGHT 80
#define BUTTON_WIDTH 300
#define STATUS_HEIGHT 60
#define LARGE_FONT_SIZE 24
#define MEDIUM_FONT_SIZE 18
#define SMALL_FONT_SIZE 14

// 音频回环配置
#define AUDIO_LOOPBACK_BUFFER_SIZE (1024 * 4)  // 4KB缓冲区
#define AUDIO_LOOPBACK_SAMPLE_RATE 48000
#define AUDIO_LOOPBACK_CHANNELS 2  // 立体声输出

// 全局音频回环数据
static audio_loopback_data_t g_loopback_data = {
    .state = AUDIO_LOOPBACK_STATE_IDLE,
    .speaker_enabled = false,
    .button_pressed = false,
    .loopback_start_time = 0,
    .total_loopback_time = 0,
    .loopback_count = 0
};

// 音频回环缓冲区
static int16_t g_audio_buffer[AUDIO_LOOPBACK_BUFFER_SIZE / sizeof(int16_t)];

// UI 元素指针
static lv_obj_t* g_loopback_button = NULL;
static lv_obj_t* g_status_label = NULL;
static lv_obj_t* g_info_label = NULL;
static lv_obj_t* g_warning_label = NULL;

// 音频回环任务句柄
static TaskHandle_t g_loopback_task_handle = NULL;

// UI更新定时器
static lv_timer_t* g_ui_update_timer = NULL;

// 状态检查标志
static bool g_initial_status_check_done = false;

// 前向声明
static void loopback_button_event_cb(lv_event_t* e);
static void audio_loopback_task(void* pvParameters);
static void audio_loopback_app_create(app_t* app);
static void audio_loopback_app_destroy(app_t* app);
static void ui_update_timer_cb(lv_timer_t* timer);

// 检查扬声器状态
bool check_speaker_status(audio_loopback_data_t* data) {
    if (!data) return false;
    
    // 检查扬声器是否启用
    data->speaker_enabled = hal_get_speaker_enable();
    
    if (data->speaker_enabled) {
        printf("Warning: Speaker is enabled, audio loopback cannot work safely\n");
        data->state = AUDIO_LOOPBACK_STATE_ERROR;
        return false;
    }
    
    printf("Speaker is disabled, audio loopback can work safely\n");
    data->state = AUDIO_LOOPBACK_STATE_IDLE;
    return true;
}

// 开始音频回环
bool start_audio_loopback(audio_loopback_data_t* data) {
    if (!data) return false;
    
    // 检查扬声器状态
    if (!check_speaker_status(data)) {
        printf("Cannot start audio loopback: speaker is enabled\n");
        return false;
    }
    
    // 检查是否已经在运行
    if (data->state == AUDIO_LOOPBACK_STATE_ACTIVE) {
        printf("Audio loopback already active\n");
        return true;
    }
    
    // 初始化音频HAL（如果还没有初始化）
    hal_audio_init();
    
    // 创建音频回环任务
    if (g_loopback_task_handle == NULL) {
        BaseType_t ret = xTaskCreate(
            audio_loopback_task,
            "audio_loopback",
            4096,
            data,
            5,
            &g_loopback_task_handle
        );
        
        if (ret != pdPASS) {
            printf("Failed to create audio loopback task\n");
            return false;
        }
    }
    
    // 更新状态
    data->state = AUDIO_LOOPBACK_STATE_ACTIVE;
    data->loopback_start_time = (uint32_t)(esp_timer_get_time() / 1000000);
    data->loopback_count++;
    
    printf("Audio loopback started\n");
    return true;
}

// 停止音频回环
void stop_audio_loopback(audio_loopback_data_t* data) {
    if (!data) return;
    
    if (data->state == AUDIO_LOOPBACK_STATE_ACTIVE) {
        // 计算回环时间
        uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000000);
        uint32_t loopback_duration = current_time - data->loopback_start_time;
        data->total_loopback_time += loopback_duration;
        
        printf("Audio loopback stopped, duration: %lu seconds\n", (unsigned long)loopback_duration);
    }
    
    // 停止任务
    if (g_loopback_task_handle != NULL) {
        vTaskDelete(g_loopback_task_handle);
        g_loopback_task_handle = NULL;
    }
    
    data->state = AUDIO_LOOPBACK_STATE_IDLE;
    data->button_pressed = false;
}

// 音频回环任务
static void audio_loopback_task(void* pvParameters) {
    audio_loopback_data_t* data = (audio_loopback_data_t*)pvParameters;
    
    printf("Audio loopback task started\n");
    
    while (data->state == AUDIO_LOOPBACK_STATE_ACTIVE) {
        // 录制音频数据
        size_t bytes_read = hal_audio_record(
            g_audio_buffer, 
            sizeof(g_audio_buffer), 
            100,  // 100ms录制
            80.0f  // 增益
        );
        
        if (bytes_read > 0) {
            // 将录制的数据播放到输出（双声道）
            size_t samples = bytes_read / sizeof(int16_t);
            bool success = hal_audio_play_pcm(
                g_audio_buffer,
                samples,
                AUDIO_LOOPBACK_SAMPLE_RATE,
                true  // 立体声
            );
            
            if (!success) {
                printf("Failed to play audio loopback data\n");
                break;
            }
        } else {
            printf("Failed to record audio data\n");
            break;
        }
        
        // 检查按钮状态
        if (!data->button_pressed) {
            break;
        }
        
        // 短暂延时
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    printf("Audio loopback task ended\n");
    vTaskDelete(NULL);
}

// 按钮事件回调
static void loopback_button_event_cb(lv_event_t* e) {
    audio_loopback_data_t* data = &g_loopback_data;
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED) {
        // 按钮按下
        data->button_pressed = true;
        printf("Loopback button pressed\n");
        
        // 检查扬声器状态
        if (!check_speaker_status(data)) {
            printf("Cannot start loopback: speaker is enabled\n");
            return;
        }
        
        // 开始音频回环
        if (start_audio_loopback(data)) {
            printf("Audio loopback started successfully\n");
        } else {
            printf("Failed to start audio loopback\n");
        }
        
    } else if (code == LV_EVENT_RELEASED) {
        // 按钮释放
        data->button_pressed = false;
        printf("Loopback button released\n");
        
        // 停止音频回环
        stop_audio_loopback(data);
        printf("Audio loopback stopped\n");
    }
}

// UI更新定时器回调
static void ui_update_timer_cb(lv_timer_t* timer) {
    (void)timer; // 避免未使用参数警告
    
    // 延迟进行初始状态检查
    if (!g_initial_status_check_done) {
        check_speaker_status(&g_loopback_data);
        g_initial_status_check_done = true;
        printf("Initial speaker status check completed\n");
    }
    
    update_audio_loopback_ui(NULL, &g_loopback_data);
}

// 更新音频回环UI
void update_audio_loopback_ui(lv_obj_t* container, audio_loopback_data_t* data) {
    if (!data) return;
    
    // 更新状态标签
    if (g_status_label) {
        switch (data->state) {
            case AUDIO_LOOPBACK_STATE_IDLE:
                lv_label_set_text(g_status_label, "Status: Idle");
                break;
            case AUDIO_LOOPBACK_STATE_ACTIVE:
                lv_label_set_text(g_status_label, "Status: Active");
                break;
            case AUDIO_LOOPBACK_STATE_ERROR:
                lv_label_set_text(g_status_label, "Status: Error");
                break;
        }
    }
    
    // 更新信息标签
    if (g_info_label) {
        char info_text[256];
        snprintf(info_text, sizeof(info_text), 
                 "Loopback Count: %lu\nTotal Time: %lu sec\nSpeaker: %s",
                 (unsigned long)data->loopback_count,
                 (unsigned long)data->total_loopback_time,
                 data->speaker_enabled ? "Enabled" : "Disabled");
        lv_label_set_text(g_info_label, info_text);
    }
    
    // 更新警告标签
    if (g_warning_label) {
        if (data->speaker_enabled) {
            lv_label_set_text(g_warning_label, "WARNING: Speaker enabled, disable first");
            lv_obj_set_style_text_color(g_warning_label, lv_color_make(255, 0, 0), 0);
            lv_obj_clear_flag(g_warning_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_warning_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // 更新按钮状态
    if (g_loopback_button) {
        lv_obj_t* btn_label = lv_obj_get_child(g_loopback_button, 0);
        if (btn_label) {
            if (data->state == AUDIO_LOOPBACK_STATE_ACTIVE && data->button_pressed) {
                lv_label_set_text(btn_label, "Release to Stop");
                lv_obj_set_style_bg_color(g_loopback_button, lv_color_make(255, 0, 0), 0);
            } else {
                lv_label_set_text(btn_label, "Hold to Start");
                lv_obj_set_style_bg_color(g_loopback_button, lv_color_make(0, 128, 0), 0);
            }
        }
        
        // 根据扬声器状态启用/禁用按钮
        if (data->speaker_enabled) {
            lv_obj_add_state(g_loopback_button, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(g_loopback_button, LV_STATE_DISABLED);
        }
    }
}

// 创建音频回环应用
static void audio_loopback_app_create(app_t* app) {
    printf("Creating audio loopback app UI\n");
    
    lv_obj_t* container = app->container;
    if (!container) {
        printf("No container for audio loopback app\n");
        return;
    }
    
    // 设置容器样式
    lv_obj_set_style_bg_color(container, lv_color_make(240, 240, 240), 0);
    lv_obj_set_style_pad_all(container, 20, 0);
    
    // 创建标题
    lv_obj_t* title_label = lv_label_create(container);
    lv_label_set_text(title_label, "Audio Loopback Test");
    lv_obj_set_style_text_color(title_label, lv_color_make(0, 0, 0), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 20);
    
    // 创建说明文本
    lv_obj_t* desc_label = lv_label_create(container);
    lv_label_set_text(desc_label, "Map ES7210 IN1 to ES8388 output\nHold button to start, release to stop");
    lv_obj_set_style_text_color(desc_label, lv_color_make(100, 100, 100), 0);
    lv_obj_align(desc_label, LV_ALIGN_TOP_MID, 0, 80);
    
    // 创建回环按钮
    g_loopback_button = lv_btn_create(container);
    lv_obj_set_size(g_loopback_button, BUTTON_WIDTH, BUTTON_HEIGHT);
    lv_obj_align(g_loopback_button, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_loopback_button, lv_color_make(0, 128, 0), 0);
    lv_obj_set_style_text_color(g_loopback_button, lv_color_make(255, 255, 255), 0);
    
    // 按钮标签
    lv_obj_t* btn_label = lv_label_create(g_loopback_button);
    lv_label_set_text(btn_label, "Hold to Start");
    lv_obj_center(btn_label);
    
    // 添加按钮事件
    lv_obj_add_event_cb(g_loopback_button, loopback_button_event_cb, LV_EVENT_ALL, NULL);
    
    // 创建状态标签
    g_status_label = lv_label_create(container);
    lv_label_set_text(g_status_label, "Status: Idle");
    lv_obj_set_style_text_color(g_status_label, lv_color_make(0, 0, 0), 0);
    lv_obj_align(g_status_label, LV_ALIGN_CENTER, 0, 100);
    
    // 创建信息标签
    g_info_label = lv_label_create(container);
    lv_label_set_text(g_info_label, "Loopback Count: 0\nTotal Time: 0 sec\nSpeaker: Checking");
    lv_obj_set_style_text_color(g_info_label, lv_color_make(0, 0, 0), 0);
    lv_obj_align(g_info_label, LV_ALIGN_CENTER, 0, 180);
    
    // 创建警告标签
    g_warning_label = lv_label_create(container);
    lv_label_set_text(g_warning_label, "WARNING: Speaker enabled, disable first");
    lv_obj_set_style_text_color(g_warning_label, lv_color_make(255, 0, 0), 0);
    lv_obj_align(g_warning_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_flag(g_warning_label, LV_OBJ_FLAG_HIDDEN);
    
    // 初始化数据
    memset(&g_loopback_data, 0, sizeof(g_loopback_data));
    g_loopback_data.state = AUDIO_LOOPBACK_STATE_IDLE;
    g_initial_status_check_done = false;
    
    // 创建UI更新定时器
    g_ui_update_timer = lv_timer_create(ui_update_timer_cb, 500, NULL);  // 每500ms更新一次
    
    // 更新UI
    update_audio_loopback_ui(container, &g_loopback_data);
    
    // 设置用户数据
    app->user_data = &g_loopback_data;
    
    printf("Audio loopback app UI created successfully\n");
}

// 销毁音频回环应用
static void audio_loopback_app_destroy(app_t* app) {
    printf("Destroying audio loopback app\n");
    
    // 停止音频回环
    stop_audio_loopback(&g_loopback_data);
    
    // 删除UI更新定时器
    if (g_ui_update_timer) {
        lv_timer_del(g_ui_update_timer);
        g_ui_update_timer = NULL;
    }
    
    // 清空全局UI指针
    g_loopback_button = NULL;
    g_status_label = NULL;
    g_info_label = NULL;
    g_warning_label = NULL;
    
    // 重置状态检查标志
    g_initial_status_check_done = false;
    
    // 清空用户数据
    if (app) {
        app->user_data = NULL;
    }
    
    printf("Audio loopback app destroyed\n");
}

// 注册音频回环应用
void register_audio_loopback_app(void) {
    app_manager_register_app("Audio Loopback", LV_SYMBOL_AUDIO, 
                             audio_loopback_app_create, audio_loopback_app_destroy);
} 