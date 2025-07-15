#include "app_audio_loopback.h"
#include "hal_audio.h"
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 测试音频回环App的基本功能
void test_audio_loopback_basic(void)
{
    printf("=== Audio Loopback App Test ===\n");
    
    // 1. 初始化音频HAL
    hal_audio_init();
    printf("✓ Audio HAL initialized\n");
    
    // 2. 创建测试数据
    audio_loopback_data_t test_data = {
        .state = AUDIO_LOOPBACK_STATE_IDLE,
        .speaker_enabled = false,
        .button_pressed = false,
        .loopback_start_time = 0,
        .total_loopback_time = 0,
        .loopback_count = 0
    };
    
    // 3. 检查扬声器状态
    printf("Testing speaker status check...\n");
    bool speaker_ok = check_speaker_status(&test_data);
    printf("  Speaker status: %s (expected: disabled)\n", 
           test_data.speaker_enabled ? "enabled" : "disabled");
    printf("  Safe to use: %s\n", speaker_ok ? "yes" : "no");
    
    // 4. 模拟扬声器启用状态
    printf("Testing with speaker enabled...\n");
    test_data.speaker_enabled = true;
    bool speaker_ok_enabled = check_speaker_status(&test_data);
    printf("  Speaker status: %s (expected: enabled)\n", 
           test_data.speaker_enabled ? "enabled" : "disabled");
    printf("  Safe to use: %s (expected: no)\n", speaker_ok_enabled ? "yes" : "no");
    
    // 5. 测试状态转换
    printf("Testing state transitions...\n");
    test_data.state = AUDIO_LOOPBACK_STATE_IDLE;
    printf("  Initial state: IDLE\n");
    
    // 模拟开始回环
    test_data.loopback_start_time = (uint32_t)(esp_timer_get_time() / 1000000);
    test_data.loopback_count++;
    printf("  Started loopback, count: %lu\n", (unsigned long)test_data.loopback_count);
    
    // 模拟停止回环
    uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000000);
    uint32_t duration = current_time - test_data.loopback_start_time;
    test_data.total_loopback_time += duration;
    printf("  Stopped loopback, duration: %lu seconds\n", (unsigned long)duration);
    printf("  Total loopback time: %lu seconds\n", (unsigned long)test_data.total_loopback_time);
    
    printf("✓ Audio Loopback App test completed successfully!\n");
}

// 测试音频回环的安全检查
void test_audio_loopback_safety(void)
{
    printf("=== Audio Loopback Safety Test ===\n");
    
    hal_audio_init();
    
    audio_loopback_data_t test_data = {
        .state = AUDIO_LOOPBACK_STATE_IDLE,
        .speaker_enabled = false,
        .button_pressed = false,
        .loopback_start_time = 0,
        .total_loopback_time = 0,
        .loopback_count = 0
    };
    
    // 测试扬声器禁用时的安全检查
    printf("Testing safety check with speaker disabled...\n");
    test_data.speaker_enabled = false;
    bool safe_disabled = check_speaker_status(&test_data);
    printf("  Safe to use: %s (expected: yes)\n", safe_disabled ? "yes" : "no");
    
    // 测试扬声器启用时的安全检查
    printf("Testing safety check with speaker enabled...\n");
    test_data.speaker_enabled = true;
    bool safe_enabled = check_speaker_status(&test_data);
    printf("  Safe to use: %s (expected: no)\n", safe_enabled ? "yes" : "no");
    
    // 测试状态变化
    printf("Testing state changes...\n");
    printf("  State with speaker disabled: %d\n", (int)test_data.state);
    
    test_data.speaker_enabled = false;
    check_speaker_status(&test_data);
    printf("  State with speaker disabled: %d\n", (int)test_data.state);
    
    test_data.speaker_enabled = true;
    check_speaker_status(&test_data);
    printf("  State with speaker enabled: %d\n", (int)test_data.state);
    
    printf("✓ Audio Loopback safety test completed!\n");
} 