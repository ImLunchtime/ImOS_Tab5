#include "overlay_drawer_events.h"
#include "overlay_drawer.h"
#include "managers/gesture_handler.h"
#include "hal.h"
#include <stdio.h>

// 应用项点击事件
void drawer_events_app_item_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    app_t* app = (app_t*)lv_event_get_user_data(e);
    
    if (code == LV_EVENT_CLICKED && app) {
        printf("*** APP ITEM CLICKED: %s ***\n", app->name);
        
        // 启动应用
        bool success = app_manager_launch_app(app->name);
        printf("App launch %s: %s\n", app->name, success ? "success" : "failed");
        
        // 关闭抽屉
        app_drawer_close();
    }
}

// 背景点击事件（关闭抽屉）
void drawer_events_background_cb(lv_event_t* e) {
    printf("Background clicked - closing drawer\n");
    app_drawer_close();
}

// 动画结束回调
void drawer_events_slide_anim_ready_cb(lv_anim_t* a) {
    drawer_state_t* state = (drawer_state_t*)a->user_data;
    if (!state->is_open) {
        // 抽屉关闭后隐藏背景
        // 同时隐藏抽屉容器，确保不会拦截事件
        lv_obj_add_flag(state->drawer_container, LV_OBJ_FLAG_HIDDEN);
        printf("Drawer completely closed and hidden\n");
        
        // 标记可以安全清理（但不立即清理）
        state->last_open_time = esp_timer_get_time() / 1000;
    }
}

// 音量滑块事件回调
void drawer_events_volume_slider_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* slider = lv_event_get_target(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        drawer_state_t* state = (drawer_state_t*)lv_event_get_user_data(e);
        if (state && state->volume_label) {
            int32_t value = lv_slider_get_value(slider);
            hal_set_speaker_volume((uint8_t)value);
            
            // 更新标签显示
            lv_label_set_text_fmt(state->volume_label, "音量: %d%%", (int)value);
            printf("Volume changed to: %d%%\n", (int)value);
        }
    }
}

// 亮度滑块事件回调
void drawer_events_brightness_slider_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* slider = lv_event_get_target(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        drawer_state_t* state = (drawer_state_t*)lv_event_get_user_data(e);
        if (state && state->brightness_label) {
            int32_t value = lv_slider_get_value(slider);
            hal_set_display_brightness((uint8_t)value);
            
            // 更新标签显示
            lv_label_set_text_fmt(state->brightness_label, "亮度: %d%%", (int)value);
            printf("Brightness changed to: %d%%\n", (int)value);
        }
    }
}

// 扬声器开关事件回调
void drawer_events_speaker_switch_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* switch_obj = lv_event_get_target(e);
    
    printf("Speaker switch event: code=%d\n", code);
    
    if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_CLICKED) {
        drawer_state_t* state = (drawer_state_t*)lv_event_get_user_data(e);
        if (state) {
            // 获取开关的当前状态
            bool enabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
            printf("Switch state: %s\n", enabled ? "checked" : "unchecked");
            
            // 调用HAL函数设置扬声器状态
            hal_set_speaker_enable(enabled);
            
            printf("Speaker %s\n", enabled ? "enabled" : "disabled");
            
            // 强制刷新显示
            lv_obj_invalidate(switch_obj);
        }
    }
}

// 控制中心按钮点击事件
void drawer_events_control_center_btn_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        drawer_state_t* state = (drawer_state_t*)lv_event_get_user_data(e);
        if (state) {
            if (state->panel_open) {
                // 关闭面板
                lv_obj_add_flag(state->control_panel, LV_OBJ_FLAG_HIDDEN);
                state->panel_open = false;
                printf("Control panel closed\n");
            } else {
                // 打开面板
                lv_obj_clear_flag(state->control_panel, LV_OBJ_FLAG_HIDDEN);
                state->panel_open = true;
                
                // 更新滑块值
                lv_slider_set_value(state->volume_slider, hal_get_speaker_volume(), LV_ANIM_OFF);
                lv_slider_set_value(state->brightness_slider, hal_get_display_brightness(), LV_ANIM_OFF);
                lv_label_set_text_fmt(state->volume_label, "音量: %d%%", hal_get_speaker_volume());
                lv_label_set_text_fmt(state->brightness_label, "亮度: %d%%", hal_get_display_brightness());
                
                // 更新开关状态
                bool speaker_enabled = hal_get_speaker_enable();
                if (speaker_enabled) {
                    lv_obj_add_state(state->speaker_switch, LV_STATE_CHECKED);
                } else {
                    lv_obj_clear_state(state->speaker_switch, LV_STATE_CHECKED);
                }
                
                printf("Control panel opened\n");
            }
        }
    }
}

// 控制面板关闭事件回调
void drawer_events_control_panel_close_cb(lv_event_t* e) {
    drawer_state_t* state = (drawer_state_t*)lv_event_get_user_data(e);
    if (state && state->control_panel) {
        lv_obj_add_flag(state->control_panel, LV_OBJ_FLAG_HIDDEN);
        state->panel_open = false;
        printf("Control panel closed\n");
    }
}