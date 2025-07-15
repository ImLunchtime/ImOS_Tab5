#include "hal_audio.h"
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 音频HAL使用示例
void audio_example_basic_usage(void)
{
    printf("=== Audio HAL Usage Example ===\n");
    
    // 1. 初始化音频HAL
    hal_audio_init();
    printf("✓ Audio HAL initialized\n");
    
    // 2. 设置扬声器音量
    hal_set_speaker_volume(80);  // 设置音量为80%
    printf("✓ Speaker volume set to 80%%\n");
    
    // 3. 启用扬声器
    hal_set_speaker_enable(true);
    printf("✓ Speaker enabled\n");
    
    // 4. 检查扬声器状态
    bool speaker_enabled = hal_get_speaker_enable();
    uint8_t current_volume = hal_get_speaker_volume();
    printf("✓ Speaker status: enabled=%s, volume=%d%%\n", 
           speaker_enabled ? "true" : "false", current_volume);
    
    // 5. 播放测试音频（如果有的话）
    // 这里可以添加实际的音频播放代码
    
    printf("=== Audio HAL Example Completed ===\n");
}

// 音频控制示例
void audio_example_control_demo(void)
{
    printf("=== Audio Control Demo ===\n");
    
    hal_audio_init();
    
    // 演示音量控制
    printf("Volume control demo:\n");
    for (int vol = 0; vol <= 100; vol += 20) {
        hal_set_speaker_volume(vol);
        printf("  Volume: %d%%\n", hal_get_speaker_volume());
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // 演示扬声器开关控制
    printf("Speaker enable/disable demo:\n");
    for (int i = 0; i < 3; i++) {
        hal_set_speaker_enable(false);
        printf("  Speaker disabled\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        hal_set_speaker_enable(true);
        printf("  Speaker enabled\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    printf("=== Audio Control Demo Completed ===\n");
} 