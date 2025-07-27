#include "overlay_drawer_memory.h"
#include "overlay_drawer_ui.h"
#include "managers/app_manager.h"
#include <stdio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 深度清理抽屉内存
void drawer_memory_deep_clean(drawer_state_t* state) {
    if (!state || state->is_open || state->deep_cleaned) {
        return;
    }
    
    printf("=== DEEP CLEANING DRAWER ===\n");
    app_manager_log_memory_usage("Before drawer deep clean");
    
    // 安全清理应用列表
    if (state->app_list && lv_obj_is_valid(state->app_list)) {
        // 检查对象是否仍然有效
        if (lv_obj_get_parent(state->app_list) != NULL) {
            // 先停止所有可能的动画
            lv_anim_del(state->app_list, NULL);
            
            // 安全获取子对象数量
            uint32_t child_count = lv_obj_get_child_count(state->app_list);
            
            // 从后往前删除子对象，避免索引问题
            for (int32_t i = (int32_t)child_count - 1; i >= 0; i--) {
                lv_obj_t* child = lv_obj_get_child(state->app_list, i);
                if (child && lv_obj_is_valid(child)) {
                    drawer_ui_cleanup_app_item(child);
                    lv_obj_del(child);
                }
            }
            
            printf("Cleaned %lu app items from drawer\n", (unsigned long)child_count);
        } else {
            printf("App list parent is null, skipping cleanup\n");
        }
    } else {
        printf("App list is invalid or null, skipping cleanup\n");
    }
    
    // 重置初始化标志，下次打开时重新创建
    state->is_initialized = false;
    state->deep_cleaned = true;
    
    printf("App drawer deep cleaned\n");
    app_manager_log_memory_usage("After drawer deep clean");
}

// 检查是否需要进行空闲清理
bool drawer_memory_should_idle_cleanup(drawer_state_t* state) {
    if (!state || state->is_open || state->deep_cleaned) {
        return false;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    return (current_time - state->last_open_time) > state->idle_cleanup_threshold;
}

// 强制清理应用列表以释放内存
void drawer_memory_cleanup_list(void) {
    overlay_t* overlay = app_manager_get_overlay("AppDrawer");
    if (!overlay || !overlay->base.user_data) {
        return;
    }
    
    drawer_state_t* state = (drawer_state_t*)overlay->base.user_data;
    if (state->is_open) {
        // 抽屉打开时不清理
        printf("Drawer is open, skipping cleanup\n");
        return;
    }
    
    // 检查是否有正在进行的动画
    if (lv_anim_count_running()) {
        printf("Animations running, skipping drawer cleanup\n");
        return;
    }
    
    // 确保抽屉已经关闭足够长时间
    uint32_t current_time = esp_timer_get_time() / 1000;
    if (state->last_open_time > 0 && (current_time - state->last_open_time) < 1000) {
        printf("Drawer recently closed, skipping cleanup\n");
        return;
    }
    
    printf("Force cleaning app drawer list to free memory\n");
    
    // 执行深度清理
    drawer_memory_deep_clean(state);
    
    printf("App drawer list cleaned\n");
}

// 智能内存管理 - 检查是否需要清理
void drawer_memory_check_cleanup(void) {
    overlay_t* overlay = app_manager_get_overlay("AppDrawer");
    if (!overlay || !overlay->base.user_data) {
        return;
    }
    
    drawer_state_t* state = (drawer_state_t*)overlay->base.user_data;
    
    // 检查是否需要空闲清理
    if (drawer_memory_should_idle_cleanup(state)) {
        printf("Idle cleanup triggered for app drawer\n");
        drawer_memory_deep_clean(state);
    }
}