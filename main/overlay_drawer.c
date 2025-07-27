#include "overlay_drawer.h"
#include "overlay_drawer_types.h"
#include "overlay_drawer_ui.h"
#include "overlay_drawer_control.h"
#include "overlay_drawer_events.h"
#include "overlay_drawer_memory.h"
#include "managers/app_manager.h"
#include "managers/gesture_handler.h"
#include "hal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 创建应用抽屉Overlay
static void drawer_overlay_create(app_t* app) {
    if (!app || !app->container) {
        return;
    }
    
    printf("Creating app drawer overlay with smart memory management\n");
    app_manager_log_memory_usage("Before drawer creation");
    
    // 创建抽屉状态
    drawer_state_t* state = (drawer_state_t*)malloc(sizeof(drawer_state_t));
    if (!state) {
        printf("Failed to allocate drawer state\n");
        return;
    }
    memset(state, 0, sizeof(drawer_state_t));
    
    // 初始化内存管理参数
    state->idle_cleanup_threshold = 30000; // 30秒后清理
    state->last_open_time = 0;
    state->deep_cleaned = false;
    state->panel_open = false;
    
    // 创建抽屉容器和基本UI
    drawer_ui_create_container(state, app);
    
    // 创建控制面板
    drawer_control_create_panel(state);
    
    // 保存状态到用户数据
    app->user_data = state;
}

// 销毁应用抽屉Overlay
static void drawer_overlay_destroy(app_t* app) {
    if (app && app->user_data) {
        drawer_state_t* state = (drawer_state_t*)app->user_data;
        
        // 清理应用列表中的内存
        if (state->app_list) {
            uint32_t child_count = lv_obj_get_child_count(state->app_list);
            for (uint32_t i = 0; i < child_count; i++) {
                lv_obj_t* child = lv_obj_get_child(state->app_list, i);
                drawer_ui_cleanup_app_item(child);
            }
        }
        
        // 确保LVGL任务完成
        lv_refr_now(NULL);
        vTaskDelay(pdMS_TO_TICKS(5));
        
        free(state);
        app->user_data = NULL;
    }
}

// 打开抽屉
// 打开抽屉
void app_drawer_open(void) {
    printf("Opening app drawer...\n");
    overlay_t* overlay = app_manager_get_overlay("AppDrawer");
    if (!overlay || !overlay->base.user_data) {
        printf("Error: AppDrawer overlay not found or no user data\n");
        return;
    }
    
    drawer_state_t* state = (drawer_state_t*)overlay->base.user_data;
    if (state->is_open) {
        printf("App drawer is already open\n");
        return;
    }
    
    // 记录打开时间
    state->last_open_time = esp_timer_get_time() / 1000;
    
    // 延迟加载：只在第一次打开或被清理后重新创建应用列表
    if (!state->is_initialized || state->deep_cleaned) {
        printf("Creating/recreating app list (initialized: %s, deep_cleaned: %s)\n",
               state->is_initialized ? "true" : "false",
               state->deep_cleaned ? "true" : "false");
        app_manager_log_memory_usage("Before app list creation");
        
        drawer_ui_refresh_app_list(state->app_list, true);
        state->is_initialized = true;
        state->deep_cleaned = false;
        
        app_manager_log_memory_usage("After app list creation");
    }
    
    // 确保抽屉容器可见
    lv_obj_clear_flag(state->drawer_container, LV_OBJ_FLAG_HIDDEN);
    
    // 强制刷新显示
    lv_obj_invalidate(state->drawer_container);
    
    // Liquid Glass设计：计算动画位置（考虑缩小的padding）
    lv_coord_t padding_left = 10;  // 更新为新的padding值
    lv_coord_t drawer_width = lv_obj_get_width(state->drawer_container);
    lv_coord_t start_pos = -drawer_width - padding_left;  // 起始位置（完全隐藏）
    lv_coord_t end_pos = padding_left;  // 结束位置（显示时的左边距）
    
    // 创建滑动动画 - 适配Liquid Glass设计
    lv_anim_init(&state->slide_anim);
    lv_anim_set_var(&state->slide_anim, state->drawer_container);
    lv_anim_set_values(&state->slide_anim, start_pos, end_pos);
    lv_anim_set_time(&state->slide_anim, 400);  // 稍微增加动画时间，更优雅
    lv_anim_set_exec_cb(&state->slide_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_path_cb(&state->slide_anim, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&state->slide_anim, drawer_events_slide_anim_ready_cb);
    lv_anim_set_user_data(&state->slide_anim, state);
    lv_anim_start(&state->slide_anim);
    
    state->is_open = true;
    
    // 临时禁用手势处理，避免事件冲突
    gesture_handler_set_enabled(false);
    
    // 调试：检查扬声器开关状态
    if (state->speaker_switch) {
        bool switch_checked = lv_obj_has_state(state->speaker_switch, LV_STATE_CHECKED);
        bool speaker_enabled = hal_get_speaker_enable();
        printf("Drawer opened - Switch checked: %s, Speaker enabled: %s\n", 
               switch_checked ? "true" : "false", 
               speaker_enabled ? "true" : "false");
    }
    
    printf("App drawer opened successfully\n");
}

// 关闭抽屉
void app_drawer_close(void) {
    overlay_t* overlay = app_manager_get_overlay("AppDrawer");
    if (!overlay || !overlay->base.user_data) {
        return;
    }
    
    drawer_state_t* state = (drawer_state_t*)overlay->base.user_data;
    if (!state->is_open) {
        return;
    }
    
    // Liquid Glass设计：计算动画位置（考虑缩小的padding）
    lv_coord_t padding_left = 10;  // 更新为新的padding值
    lv_coord_t drawer_width = lv_obj_get_width(state->drawer_container);
    lv_coord_t start_pos = padding_left;  // 起始位置（当前显示位置）
    lv_coord_t end_pos = -drawer_width - padding_left;  // 结束位置（完全隐藏）
    
    // 创建滑动动画 - 适配Liquid Glass设计
    lv_anim_init(&state->slide_anim);
    lv_anim_set_var(&state->slide_anim, state->drawer_container);
    lv_anim_set_values(&state->slide_anim, start_pos, end_pos);
    lv_anim_set_time(&state->slide_anim, 350);  // 关闭动画稍快一些
    lv_anim_set_exec_cb(&state->slide_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_path_cb(&state->slide_anim, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&state->slide_anim, drawer_events_slide_anim_ready_cb);
    lv_anim_set_user_data(&state->slide_anim, state);
    lv_anim_start(&state->slide_anim);
    
    state->is_open = false;
    
    // 重新启用手势处理
    gesture_handler_set_enabled(true);
}

// 切换抽屉状态
void app_drawer_toggle(void) {
    overlay_t* overlay = app_manager_get_overlay("AppDrawer");
    if (!overlay || !overlay->base.user_data) {
        return;
    }
    
    drawer_state_t* state = (drawer_state_t*)overlay->base.user_data;
    if (state->is_open) {
        app_drawer_close();
    } else {
        app_drawer_open();
    }
}

// 注册应用抽屉Overlay
void register_drawer_overlay(void) {
    app_manager_register_overlay("AppDrawer", LV_SYMBOL_LIST, 
                                 drawer_overlay_create, drawer_overlay_destroy,
                                 50, true);  // z_index=50, auto_start=true
}

// 兼容性函数 - 重定向到内存管理模块
void app_drawer_cleanup_list(void) {
    drawer_memory_cleanup_list();
}

void app_drawer_check_cleanup(void) {
    drawer_memory_check_cleanup();
}

