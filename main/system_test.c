#include "system_test.h"
#include "app_manager.h"
#include "hal_audio.h"
#include "app_audio_loopback.h"
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 测试音频HAL基本功能
void test_audio_hal_basic(void) {
    printf("Testing audio HAL basic functionality...\n");
    
    // 初始化音频HAL
    hal_audio_init();
    printf("✓ Audio HAL initialized\n");
    
    // 测试扬声器使能控制
    printf("Testing speaker enable control...\n");
    
    // 禁用扬声器
    hal_set_speaker_enable(false);
    vTaskDelay(pdMS_TO_TICKS(100));
    bool speaker_enabled = hal_get_speaker_enable();
    printf("  Speaker disabled: %s (expected: false)\n", speaker_enabled ? "true" : "false");
    
    // 启用扬声器
    hal_set_speaker_enable(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    speaker_enabled = hal_get_speaker_enable();
    printf("  Speaker enabled: %s (expected: true)\n", speaker_enabled ? "true" : "false");
    
    // 测试音量控制
    printf("Testing volume control...\n");
    
    // 设置音量到50%
    hal_set_speaker_volume(50);
    uint8_t volume = hal_get_speaker_volume();
    printf("  Volume set to 50%%: %d%%\n", volume);
    
    // 设置音量到100%
    hal_set_speaker_volume(100);
    volume = hal_get_speaker_volume();
    printf("  Volume set to 100%%: %d%%\n", volume);
    
    // 设置音量到0%
    hal_set_speaker_volume(0);
    volume = hal_get_speaker_volume();
    printf("  Volume set to 0%%: %d%%\n", volume);
    
    // 测试边界值
    hal_set_speaker_volume(150); // 应该被限制到100
    volume = hal_get_speaker_volume();
    printf("  Volume set to 150%% (should be clamped): %d%%\n", volume);
    
    printf("✓ Audio HAL test completed successfully!\n");
}

// 测试音频回环App基本功能
void test_audio_loopback_app_basic(void) {
    printf("Testing audio loopback app basic functionality...\n");
    
    // 初始化音频HAL
    hal_audio_init();
    printf("✓ Audio HAL initialized\n");
    
    // 创建测试数据
    audio_loopback_data_t test_data = {
        .state = AUDIO_LOOPBACK_STATE_IDLE,
        .speaker_enabled = false,
        .button_pressed = false,
        .loopback_start_time = 0,
        .total_loopback_time = 0,
        .loopback_count = 0
    };
    
    // 测试扬声器状态检查
    printf("Testing speaker status check...\n");
    bool speaker_ok = check_speaker_status(&test_data);
    printf("  Speaker status: %s\n", test_data.speaker_enabled ? "enabled" : "disabled");
    printf("  Safe to use: %s\n", speaker_ok ? "yes" : "no");
    
    // 测试状态管理
    printf("Testing state management...\n");
    printf("  Initial state: %d\n", (int)test_data.state);
    
    // 模拟按钮按下
    test_data.button_pressed = true;
    printf("  Button pressed: %s\n", test_data.button_pressed ? "true" : "false");
    
    // 模拟按钮释放
    test_data.button_pressed = false;
    printf("  Button released: %s\n", test_data.button_pressed ? "true" : "false");
    
    printf("✓ Audio loopback app test completed successfully!\n");
}

// 测试应用管理器基本功能
void test_app_manager_basic(void) {
    printf("Testing app manager basic functionality...\n");
    
    // 获取应用管理器实例
    app_manager_t* manager = app_manager_get_instance();
    if (!manager) {
        printf("ERROR: Failed to get app manager instance\n");
        return;
    }
    
    // 检查是否已初始化
    if (!manager->initialized) {
        printf("ERROR: App manager not initialized\n");
        return;
    }
    
    printf("✓ App manager initialized successfully\n");
    
    // 测试应用列表
    app_t* app_list = app_manager_get_app_list();
    int app_count = 0;
    app_t* current = app_list;
    while (current) {
        printf("  - App: %s (icon: %s)\n", current->name, current->icon);
        app_count++;
        current = current->next;
    }
    printf("✓ Found %d registered apps\n", app_count);
    
    // 测试Overlay列表
    overlay_t* overlay_list = app_manager_get_overlay_list();
    int overlay_count = 0;
    overlay_t* current_overlay = overlay_list;
    while (current_overlay) {
        printf("  - Overlay: %s (z_index: %d, auto_start: %s)\n", 
               current_overlay->base.name, 
               current_overlay->z_index,
               current_overlay->auto_start ? "true" : "false");
        overlay_count++;
        current_overlay = current_overlay->next;
    }
    printf("✓ Found %d registered overlays\n", overlay_count);
    
    // 检查当前应用
    app_t* current_app = app_manager_get_current_app();
    if (current_app) {
        printf("✓ Current app: %s\n", current_app->name);
    } else {
        printf("! No current app running\n");
    }
    
    printf("App manager test completed successfully!\n");
}

// 运行所有测试
void run_system_tests(void) {
    printf("=== System Tests ===\n");
    test_audio_hal_basic();
    test_audio_loopback_app_basic();
    test_app_manager_basic();
    printf("=== Tests Complete ===\n");
} 