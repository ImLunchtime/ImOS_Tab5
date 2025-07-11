#include "app_settings.h"
#include "app_manager.h"
#include "menu_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>

// 页面类型枚举
typedef enum {
    PAGE_TYPE_MAIN,
    PAGE_TYPE_MECHANICS,
    PAGE_TYPE_SOUND,
    PAGE_TYPE_DISPLAY,
    PAGE_TYPE_SOFTWARE_INFO,
    PAGE_TYPE_LEGAL_INFO,
    PAGE_TYPE_ABOUT,
    PAGE_TYPE_MENU_MODE,
    PAGE_TYPE_COUNT
} settings_page_type_t;

// 页面状态结构
typedef struct {
    lv_obj_t* page_obj;
    bool is_created;
    bool is_active;
    settings_page_type_t type;
} settings_page_t;

// 设置应用状态
typedef struct {
    lv_obj_t* menu;
    lv_obj_t* root_page;
    settings_page_t pages[PAGE_TYPE_COUNT];
    settings_page_type_t current_page;
    bool is_initialized;
} settings_state_t;

// 全局状态变量 - 使用PSRAM来存储状态
static settings_state_t* g_settings_state = NULL;

// 前向声明
static void create_page_on_demand(settings_page_type_t page_type);
static void cleanup_unused_pages(void);
static void page_event_handler(lv_event_t* e);
static lv_obj_t* create_mechanics_page(lv_obj_t* menu);
static lv_obj_t* create_sound_page(lv_obj_t* menu);
static lv_obj_t* create_display_page(lv_obj_t* menu);
static lv_obj_t* create_software_info_page(lv_obj_t* menu);
static lv_obj_t* create_legal_info_page(lv_obj_t* menu);
static lv_obj_t* create_about_page(lv_obj_t* menu);
static lv_obj_t* create_menu_mode_page(lv_obj_t* menu);

// 安全的内存分配函数 - 优先使用PSRAM
static void* safe_malloc(size_t size) {
    void* ptr = NULL;
    
    // 首先尝试使用PSRAM
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >= size) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (ptr) {
            printf("Allocated %zu bytes from PSRAM\n", size);
            return ptr;
        }
    }
    
    // 如果PSRAM不够，使用常规内存
    ptr = malloc(size);
    if (ptr) {
        printf("Allocated %zu bytes from regular heap\n", size);
    } else {
        printf("Failed to allocate %zu bytes\n", size);
    }
    
    return ptr;
}

// 安全的内存释放函数
static void safe_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

// 页面创建函数
static lv_obj_t* create_mechanics_page(lv_obj_t* menu) {
    printf("Creating mechanics page\n");
    
    lv_obj_t* page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(page);
    lv_obj_t* section = lv_menu_section_create(page);
    
    menu_create_slider(section, LV_SYMBOL_SETTINGS, "Velocity", 0, 150, 120);
    menu_create_slider(section, LV_SYMBOL_SETTINGS, "Acceleration", 0, 150, 50);
    menu_create_slider(section, LV_SYMBOL_SETTINGS, "Weight limit", 0, 150, 80);
    
    return page;
}

static lv_obj_t* create_sound_page(lv_obj_t* menu) {
    printf("Creating sound page\n");
    
    lv_obj_t* page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(page);
    lv_obj_t* section = lv_menu_section_create(page);
    
    menu_create_switch(section, LV_SYMBOL_AUDIO, "Sound", false);
    
    return page;
}

static lv_obj_t* create_display_page(lv_obj_t* menu) {
    printf("Creating display page\n");
    
    lv_obj_t* page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(page);
    lv_obj_t* section = lv_menu_section_create(page);
    
    menu_create_slider(section, LV_SYMBOL_SETTINGS, "Brightness", 0, 150, 100);
    
    return page;
}

static lv_obj_t* create_software_info_page(lv_obj_t* menu) {
    printf("Creating software info page\n");
    
    lv_obj_t* page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_obj_t* section = lv_menu_section_create(page);
    
    menu_create_text(section, NULL, "Version 1.0", LV_MENU_ITEM_BUILDER_VARIANT_1);
    
    return page;
}

static lv_obj_t* create_legal_info_page(lv_obj_t* menu) {
    printf("Creating legal info page\n");
    
    lv_obj_t* page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_obj_t* section = lv_menu_section_create(page);
    
    for(uint32_t i = 0; i < 5; i++) {  // 减少文本数量以节省内存
        menu_create_text(section, NULL,
                    "This is a text item for legal information.",
                    LV_MENU_ITEM_BUILDER_VARIANT_1);
    }
    
    return page;
}

static lv_obj_t* create_about_page(lv_obj_t* menu) {
    printf("Creating about page\n");
    
    lv_obj_t* page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(page);
    lv_obj_t* section = lv_menu_section_create(page);
    
    lv_obj_t* cont = menu_create_text(section, NULL, "Software information", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, page_event_handler, LV_EVENT_CLICKED, (void*)PAGE_TYPE_SOFTWARE_INFO);
    
    cont = menu_create_text(section, NULL, "Legal information", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, page_event_handler, LV_EVENT_CLICKED, (void*)PAGE_TYPE_LEGAL_INFO);
    
    return page;
}

static lv_obj_t* create_menu_mode_page(lv_obj_t* menu) {
    printf("Creating menu mode page\n");
    
    lv_obj_t* page = lv_menu_page_create(menu, NULL);
    lv_obj_set_style_pad_hor(page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(page);
    lv_obj_t* section = lv_menu_section_create(page);
    
    lv_obj_t* cont = menu_create_switch(section, LV_SYMBOL_AUDIO, "Sidebar enable", true);
    lv_obj_add_event_cb(lv_obj_get_child(cont, 2), menu_switch_handler, LV_EVENT_VALUE_CHANGED, menu);
    
    return page;
}

// 页面事件处理器
static void page_event_handler(lv_event_t* e) {
    if (!g_settings_state) return;
    
    settings_page_type_t page_type = (settings_page_type_t)lv_event_get_user_data(e);
    
    printf("Page event: switching to page type %d\n", page_type);
    
    // 创建页面（如果需要）
    create_page_on_demand(page_type);
    
    // 设置页面加载事件
    if (g_settings_state->pages[page_type].is_created) {
        lv_menu_set_load_page_event(g_settings_state->menu, lv_event_get_target(e), 
                                   g_settings_state->pages[page_type].page_obj);
    }
    
    // 更新当前页面
    g_settings_state->current_page = page_type;
    
    // 清理未使用的页面
    cleanup_unused_pages();
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
        case PAGE_TYPE_MECHANICS:
            page = create_mechanics_page(g_settings_state->menu);
            break;
        case PAGE_TYPE_SOUND:
            page = create_sound_page(g_settings_state->menu);
            break;
        case PAGE_TYPE_DISPLAY:
            page = create_display_page(g_settings_state->menu);
            break;
        case PAGE_TYPE_SOFTWARE_INFO:
            page = create_software_info_page(g_settings_state->menu);
            break;
        case PAGE_TYPE_LEGAL_INFO:
            page = create_legal_info_page(g_settings_state->menu);
            break;
        case PAGE_TYPE_ABOUT:
            page = create_about_page(g_settings_state->menu);
            break;
        case PAGE_TYPE_MENU_MODE:
            page = create_menu_mode_page(g_settings_state->menu);
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
    
    printf("Creating settings app with lazy loading\n");
    app_manager_log_memory_usage("Before settings app creation");
    
    // 分配状态结构 - 优先使用PSRAM
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

    // 创建主页面（总是创建）
    g_settings_state->root_page = lv_menu_page_create(menu, "Settings");
    lv_obj_set_style_pad_hor(g_settings_state->root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_obj_t* section = lv_menu_section_create(g_settings_state->root_page);
    
    // 创建主页面的菜单项（但不创建子页面）
    lv_obj_t* cont = menu_create_text(section, LV_SYMBOL_SETTINGS, "Mechanics", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, page_event_handler, LV_EVENT_CLICKED, (void*)PAGE_TYPE_MECHANICS);
    
    cont = menu_create_text(section, LV_SYMBOL_AUDIO, "Sound", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, page_event_handler, LV_EVENT_CLICKED, (void*)PAGE_TYPE_SOUND);
    
    cont = menu_create_text(section, LV_SYMBOL_SETTINGS, "Display", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, page_event_handler, LV_EVENT_CLICKED, (void*)PAGE_TYPE_DISPLAY);

    menu_create_text(g_settings_state->root_page, NULL, "Others", LV_MENU_ITEM_BUILDER_VARIANT_1);
    section = lv_menu_section_create(g_settings_state->root_page);
    
    cont = menu_create_text(section, NULL, "About", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, page_event_handler, LV_EVENT_CLICKED, (void*)PAGE_TYPE_ABOUT);
    
    cont = menu_create_text(section, LV_SYMBOL_SETTINGS, "Menu mode", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_event_cb(cont, page_event_handler, LV_EVENT_CLICKED, (void*)PAGE_TYPE_MENU_MODE);

    lv_menu_set_sidebar_page(menu, g_settings_state->root_page);

    // 标记主页面已创建
    g_settings_state->pages[PAGE_TYPE_MAIN].page_obj = g_settings_state->root_page;
    g_settings_state->pages[PAGE_TYPE_MAIN].is_created = true;
    g_settings_state->pages[PAGE_TYPE_MAIN].type = PAGE_TYPE_MAIN;
    g_settings_state->current_page = PAGE_TYPE_MAIN;
    
    lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED, NULL);
    
    // 保存状态到用户数据
    app->user_data = g_settings_state;
    g_settings_state->is_initialized = true;
    
    printf("Settings app created with lazy loading enabled\n");
    app_manager_log_memory_usage("After settings app creation");
}

// 设置应用销毁
static void settings_app_destroy(app_t* app) {
    if (!app) {
        return;
    }
    
    printf("Destroying settings app\n");
    app_manager_log_memory_usage("Before settings app destruction");
    
    if (g_settings_state) {
        // 重要：不要手动删除页面对象！
        // 它们会在菜单销毁时自动删除，手动删除会导致双重释放
        
        // 只需要重置状态标志
        for (int i = 0; i < PAGE_TYPE_COUNT; i++) {
            g_settings_state->pages[i].page_obj = NULL;
            g_settings_state->pages[i].is_created = false;
            g_settings_state->pages[i].is_active = false;
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
    app_manager_register_app("Settings", LV_SYMBOL_SETTINGS, 
                             settings_app_create, settings_app_destroy);
} 