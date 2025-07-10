#include "system_test.h"
#include "app_manager.h"
#include <stdio.h>

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
    test_app_manager_basic();
    printf("=== Tests Complete ===\n");
} 