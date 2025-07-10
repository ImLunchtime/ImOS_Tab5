#include "app_launcher.h"
#include "app_manager.h"
#include "overlay_drawer.h"
#include <stdio.h>

// 测试按钮点击事件处理
static void test_button_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        printf("*** TEST BUTTON CLICKED - App interface is working! ***\n");
        
        // 创建一个临时的反馈标签（不使用定时删除）
        lv_obj_t* feedback = lv_label_create(lv_screen_active());
        lv_label_set_text(feedback, "Button Clicked!");
        lv_obj_set_style_text_color(feedback, lv_color_hex(0xFF0000), 0);
        lv_obj_align(feedback, LV_ALIGN_CENTER, 0, 100);
        
        printf("Created feedback label - check the screen for visual confirmation\n");
    }
}

// 启动器应用创建
static void launcher_app_create(app_t* app) {
    if (!app || !app->container) {
        return;
    }
    
    // 设置纯色背景
    lv_obj_set_style_bg_color(app->container, lv_color_hex(0xF8F8F8), 0);  // 浅色背景
    lv_obj_set_style_bg_opa(app->container, LV_OPA_COVER, 0);
    
    // 创建欢迎标题
    lv_obj_t* title = lv_label_create(app->container);
    lv_label_set_text(title, "M5Tab5 Launcher");
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);  // 深色文字
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -80);
    
    // 创建测试按钮
    lv_obj_t* test_btn = lv_btn_create(app->container);
    lv_obj_set_size(test_btn, 200, 50);
    lv_obj_align(test_btn, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(test_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_add_event_cb(test_btn, test_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* btn_label = lv_label_create(test_btn);
    lv_label_set_text(btn_label, "Test Click");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(btn_label);
    
    // 创建提示文本
    lv_obj_t* hint = lv_label_create(app->container);
    lv_label_set_text(hint, "Swipe from left edge to open app drawer");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);  // 中等深度的灰色
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 50);
}

// 启动器应用销毁
static void launcher_app_destroy(app_t* app) {
    // 关闭应用抽屉
    app_drawer_close();
}

// 注册启动器应用
void register_launcher_app(void) {
    app_manager_register_app("Launcher", LV_SYMBOL_HOME, 
                             launcher_app_create, launcher_app_destroy);
} 