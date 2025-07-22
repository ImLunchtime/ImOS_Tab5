#include "apps/settings/app_settings.h"
#include "apps/settings/settings_common.h"
#include "apps/settings/settings_about.h"
#include "apps/settings/settings_display.h"
#include "apps/settings/settings_sound.h"
#include "managers/app_manager.h"
#include "utils/menu_utils.h"
#include "utils/memory_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "hal.h"
#include "managers/content_lock.h"

// 全局状态变量定义
settings_state_t* g_settings_state = NULL;

// 前向声明
static void create_page_on_demand(settings_page_type_t page_type);
static void cleanup_unused_pages(void);
static void page_event_handler(lv_event_t* e);
static void settings_app_create(app_t* app);
static void settings_app_destroy(app_t* app);

// 更新侧边栏高亮
void update_sidebar_highlight(settings_page_type_t active_page) {
    if (!g_settings_state) return;
    
    // 遍历所有页面，更新侧边栏项的样式
    for (int i = 0; i < PAGE_TYPE_COUNT; i++) {
        if (g_settings_state->pages[i].sidebar_item) {
            if (i == active_page) {
                // 高亮当前选中的项（蓝色）
                lv_obj_set_style_bg_color(g_settings_state->pages[i].sidebar_item, lv_color_hex(0x0078D7), 0);
                lv_obj_set_style_bg_opa(g_settings_state->pages[i].sidebar_item, LV_OPA_COVER, 0);
                lv_obj_set_style_text_color(g_settings_state->pages[i].sidebar_item, lv_color_hex(0xFFFFFF), 0);
            } else {
                // 恢复其他项的默认样式
                lv_obj_set_style_bg_opa(g_settings_state->pages[i].sidebar_item, LV_OPA_TRANSP, 0);
                lv_obj_set_style_text_color(g_settings_state->pages[i].sidebar_item, lv_color_hex(0x333333), 0);
            }
        }
    }
}

// 页面事件处理器
static void page_event_handler(lv_event_t* e) {
    if (!g_settings_state) return;
    
    settings_page_type_t page_type = (settings_page_type_t)lv_event_get_user_data(e);
    
    printf("Page event: switching to page type %d\n", page_type);
    
    // 创建页面（如果需要）
    create_page_on_demand(page_type);
    
    // 确保页面创建成功后立即导航
    if (g_settings_state->pages[page_type].is_created && g_settings_state->pages[page_type].page_obj) {
        printf("Navigating to page type %d\n", page_type);
        
        // 使用LVGL菜单的标准导航方法
        lv_menu_set_page(g_settings_state->menu, g_settings_state->pages[page_type].page_obj);
        
        // 更新当前页面
        g_settings_state->current_page = page_type;
        
        // 更新侧边栏高亮
        update_sidebar_highlight(page_type);
        
        // 清理未使用的页面
        cleanup_unused_pages();
        
        printf("Successfully navigated to page type %d\n", page_type);
    } else {
        printf("Failed to create or navigate to page type %d\n", page_type);
    }
}

// 按需创建页面
static void create_page_on_demand(settings_page_type_t page_type) {
    if (!g_settings_state || g_settings_state->pages[page_type].is_created) {
        printf("Page type %d already created or state invalid\n", page_type);
        return;
    }
    
    printf("Creating page on demand: type %d\n", page_type);
    
    // 记录创建前的内存使用
    app_manager_log_memory_usage("Before page creation");
    
    lv_obj_t* page = NULL;
    
    switch (page_type) {
        case PAGE_TYPE_ABOUT:
            page = create_about_page(g_settings_state->menu);
            break;
        case PAGE_TYPE_DISPLAY:
            page = create_display_page(g_settings_state->menu);
            break;
        case PAGE_TYPE_SOUND:
            page = create_sound_page(g_settings_state->menu);
            break;
        default:
            printf("Unknown page type: %d\n", page_type);
            return;
    }
    
    if (page) {
        g_settings_state->pages[page_type].page_obj = page;
        g_settings_state->pages[page_type].is_created = true;
        g_settings_state->pages[page_type].type = page_type;
        printf("Page type %d created successfully\n", page_type);
        
        // 记录创建后的内存使用
        app_manager_log_memory_usage("After page creation");
    } else {
        printf("Failed to create page type %d\n", page_type);
    }
}

// 清理未使用的页面
static void cleanup_unused_pages(void) {
    if (!g_settings_state) return;
    
    int cleaned_count = 0;
    
    for (int i = 0; i < PAGE_TYPE_COUNT; i++) {
        // 跳过主页面和当前页面
        if (i == PAGE_TYPE_MAIN || i == g_settings_state->current_page) {
            continue;
        }
        
        if (g_settings_state->pages[i].is_created) {
            printf("Cleaning up unused page type %d\n", i);
            
            // 标记为未创建，但不手动删除对象
            // 对象会在菜单销毁时自动删除
            g_settings_state->pages[i].page_obj = NULL;
            g_settings_state->pages[i].is_created = false;
            g_settings_state->pages[i].is_active = false;
            cleaned_count++;
        }
    }
    
    if (cleaned_count > 0) {
        printf("Cleaned up %d unused pages\n", cleaned_count);
        app_manager_log_memory_usage("After page cleanup");
    }
}

// 设置应用创建
static void settings_app_create(app_t* app) {
    if (!app || !app->container) {
        return;
    }
    
    printf("Creating settings app with modular structure\n");
    app_manager_log_memory_usage("Before settings app creation");
    
    // 分配状态结构
    g_settings_state = (settings_state_t*)safe_malloc(sizeof(settings_state_t));
    if (!g_settings_state) {
        printf("Failed to allocate memory for settings state\n");
        return;
    }
    
    // 初始化状态
    memset(g_settings_state, 0, sizeof(settings_state_t));
    
    // 创建菜单
    g_settings_state->menu = lv_menu_create(app->container);
    lv_obj_t* menu = g_settings_state->menu;

    lv_color_t bg_color = lv_obj_get_style_bg_color(menu, 0);
    if(lv_color_brightness(bg_color) > 127) {
        lv_obj_set_style_bg_color(menu, lv_color_darken(lv_obj_get_style_bg_color(menu, 0), 10), 0);
    }
    else {
        lv_obj_set_style_bg_color(menu, lv_color_darken(lv_obj_get_style_bg_color(menu, 0), 50), 0);
    }
    lv_menu_set_mode_root_back_button(menu, LV_MENU_ROOT_BACK_BUTTON_ENABLED);
    lv_obj_add_event_cb(menu, menu_back_event_handler, LV_EVENT_CLICKED, menu);
    lv_obj_set_size(menu, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(menu, 0, 0);

    // 创建主页面
    g_settings_state->root_page = lv_menu_page_create(menu, "设置");
    lv_obj_set_style_pad_hor(g_settings_state->root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_obj_t* section = lv_menu_section_create(g_settings_state->root_page);
    
    // 创建第一级菜单项
    lv_obj_t* cont;
    
    // About
    cont = menu_create_text(section, LV_SYMBOL_HOME, "关于本机", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, page_event_handler, LV_EVENT_CLICKED, (void*)PAGE_TYPE_ABOUT);
    g_settings_state->pages[PAGE_TYPE_ABOUT].sidebar_item = cont;
    
    // Display
    cont = menu_create_text(section, LV_SYMBOL_EYE_OPEN, "显示", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, page_event_handler, LV_EVENT_CLICKED, (void*)PAGE_TYPE_DISPLAY);
    g_settings_state->pages[PAGE_TYPE_DISPLAY].sidebar_item = cont;
    
    // Sound
    cont = menu_create_text(section, LV_SYMBOL_VOLUME_MAX, "声音", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, page_event_handler, LV_EVENT_CLICKED, (void*)PAGE_TYPE_SOUND);
    g_settings_state->pages[PAGE_TYPE_SOUND].sidebar_item = cont;

    lv_menu_set_sidebar_page(menu, g_settings_state->root_page);

    // 标记主页面已创建
    g_settings_state->pages[PAGE_TYPE_MAIN].page_obj = g_settings_state->root_page;
    g_settings_state->pages[PAGE_TYPE_MAIN].is_created = true;
    g_settings_state->pages[PAGE_TYPE_MAIN].type = PAGE_TYPE_MAIN;
    g_settings_state->current_page = PAGE_TYPE_MAIN;
    
    // 保存状态到用户数据
    app->user_data = g_settings_state;
    g_settings_state->is_initialized = true;
    
    printf("Settings app created with modular structure\n");
    app_manager_log_memory_usage("After settings app creation");
    
    // 默认打开"关于本机"页面
    lv_obj_send_event(g_settings_state->pages[PAGE_TYPE_ABOUT].sidebar_item, LV_EVENT_CLICKED, (void*)PAGE_TYPE_ABOUT);
}

// 设置应用销毁
static void settings_app_destroy(app_t* app) {
    if (!app) {
        return;
    }
    
    printf("Destroying settings app\n");
    app_manager_log_memory_usage("Before settings app destruction");
    
    if (g_settings_state) {
        // 重置状态标志
        for (int i = 0; i < PAGE_TYPE_COUNT; i++) {
            g_settings_state->pages[i].page_obj = NULL;
            g_settings_state->pages[i].is_created = false;
            g_settings_state->pages[i].is_active = false;
            g_settings_state->pages[i].sidebar_item = NULL;
        }
        
        // 菜单对象会在应用容器销毁时自动删除
        g_settings_state->menu = NULL;
        g_settings_state->root_page = NULL;
        g_settings_state->is_initialized = false;
        
        // 等待LVGL任务完成当前的渲染周期
        lv_refr_now(NULL);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // 释放状态结构
        safe_free(g_settings_state);
        g_settings_state = NULL;
    }
    
    if (app) {
        app->user_data = NULL;
    }
    
    printf("Settings app destroyed\n");
    app_manager_log_memory_usage("After settings app destruction");
}

// 注册设置应用
void register_settings_app(void) {
    app_manager_register_app("设置", LV_SYMBOL_SETTINGS, 
                             settings_app_create, settings_app_destroy);
}