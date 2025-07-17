#include "gui.h"
#include "app_manager.h"
#include "overlay_drawer.h"
#include "app_launcher.h"
#include "app_settings.h"
#include "app_music_player.h"
#include "app_file_manager.h"
#include "app_audio_loopback.h"
#include "app_pwm_servo.h"
#include "gesture_handler.h"
#include "system_test.h"

// 包含自定义字体（只包含一次）
#include "assets/simhei_32.c"

void gui_init(lv_disp_t *disp) 
{
    // 初始化应用管理器
    app_manager_init();
    
    // 注册Overlay（按z_index顺序）
    register_drawer_overlay();      // z_index=50
    
    // 注册应用
    register_launcher_app();
    register_settings_app();
    register_music_player_app();
    register_file_manager_app();
    register_pwm_servo_app();
    //register_audio_loopback_app(); removed due to bugs
    
    // 启动所有auto_start的Overlay
    overlay_t* overlay = app_manager_get_overlay_list();
    while (overlay) {
        if (overlay->auto_start) {
            app_manager_show_overlay(overlay->base.name);
        }
        overlay = overlay->next;
    }
    
    // 初始化手势处理（在overlay之后，确保手势区域在最顶层）
    gesture_handler_init();
    
    // 启动启动器应用
    app_manager_go_to_launcher();
    
    // 运行系统测试（可选，调试时使用）
    #ifdef DEBUG_SYSTEM_TESTS
    run_system_tests();
    #endif
} 