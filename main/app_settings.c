#include "app_settings.h"
#include "app_manager.h"
#include "menu_utils.h"
#include "hal.h"
#include "content_lock.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

// 声明自定义字体
LV_FONT_DECLARE(simhei_32);

// 页面类型枚举
typedef enum {
    PAGE_TYPE_MAIN,
    PAGE_TYPE_ABOUT,
    PAGE_TYPE_DISPLAY,
    PAGE_TYPE_SOUND,
    PAGE_TYPE_COUNT
} settings_page_type_t;

// 页面状态结构
typedef struct {
    lv_obj_t* page_obj;
    bool is_created;
    bool is_active;
    settings_page_type_t type;
    lv_obj_t* sidebar_item; // 添加侧边栏项引用
} settings_page_t;

// 设置应用状态
typedef struct {
    lv_obj_t* menu;
    lv_obj_t* root_page;
    settings_page_t pages[PAGE_TYPE_COUNT];
    settings_page_type_t current_page;
    bool is_initialized;
    
    // 版本号点击计数
    uint32_t version_click_count;
    uint32_t last_click_time;
} settings_state_t;

// 全局状态变量
static settings_state_t* g_settings_state = NULL;

// 前向声明
static void create_page_on_demand(settings_page_type_t page_type);
static void cleanup_unused_pages(void);
static void page_event_handler(lv_event_t* e);
static lv_obj_t* create_about_page(lv_obj_t* menu);
static lv_obj_t* create_display_page(lv_obj_t* menu);
static lv_obj_t* create_sound_page(lv_obj_t* menu);
static void update_sidebar_highlight(settings_page_type_t active_page);
static void brightness_slider_event_cb(lv_event_t* e);
static void volume_slider_event_cb(lv_event_t* e);
static void speaker_switch_event_cb(lv_event_t* e);
static void version_click_event_cb(lv_event_t* e);
static void unlock_dialog_event_cb(lv_event_t* e);

// 安全的内存分配函数
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

// 亮度滑块事件回调
static void brightness_slider_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* slider = lv_event_get_target(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(slider);
        hal_set_display_brightness((uint8_t)value);
        
        // 更新标签显示
        lv_obj_t* label = lv_event_get_user_data(e);
        if (label) {
            lv_label_set_text_fmt(label, "亮度: %d%%", (int)value);
        }
        printf("Brightness changed to: %d%%\n", (int)value);
    }
}

// 音量滑块事件回调
static void volume_slider_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* slider = lv_event_get_target(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        int32_t value = lv_slider_get_value(slider);
        hal_set_speaker_volume((uint8_t)value);
        
        // 更新标签显示
        lv_obj_t* label = lv_event_get_user_data(e);
        if (label) {
            lv_label_set_text_fmt(label, "音量: %d%%", (int)value);
        }
        printf("Volume changed to: %d%%\n", (int)value);
    }
}

// 扬声器开关事件回调
static void speaker_switch_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* switch_obj = lv_event_get_target(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        // 获取开关的当前状态
        bool enabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
        
        // 调用HAL函数设置扬声器状态
        hal_set_speaker_enable(enabled);
        
        printf("Speaker %s\n", enabled ? "enabled" : "disabled");
    }
}

// 更新侧边栏高亮
static void update_sidebar_highlight(settings_page_type_t active_page) {
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

// 版本号点击事件回调
static void version_click_event_cb(lv_event_t* e) {
    if (!g_settings_state) return;
    
    uint32_t current_time = hal_get_uptime_ms();
    
    // 如果距离上次点击超过3秒，重置计数
    if (current_time - g_settings_state->last_click_time > 3000) {
        g_settings_state->version_click_count = 0;
    }
    
    g_settings_state->version_click_count++;
    g_settings_state->last_click_time = current_time;
    
    printf("Version clicked %lu times\n", (unsigned long)g_settings_state->version_click_count);
    
    // 点击5次后切换解锁状态
    if (g_settings_state->version_click_count >= 5) {
        g_settings_state->version_click_count = 0;
        
        // 切换解锁状态
        esp_err_t ret = content_lock_toggle();
        if (ret != ESP_OK) {
            printf("Failed to toggle content lock: %s\n", esp_err_to_name(ret));
            return;
        }
        
        // 获取新的解锁状态
        bool unlocked = content_lock_is_unlocked();
        
        // 创建对话框 - 修复LVGL 9.x的API
        lv_obj_t* dialog = lv_msgbox_create(NULL);
        lv_msgbox_add_title(dialog, "内容锁状态");
        lv_obj_set_style_text_font(dialog, &simhei_32, 0);
        
        // 设置对话框内容
        char msg[128];
        snprintf(msg, sizeof(msg), "Current unlock status, please reboot to apply the changes: %s", unlocked ? "true" : "false");
        lv_msgbox_add_text(dialog, msg);
        
        // 添加确定按钮
        // lv_msgbox_add_footer_button(dialog, "确定");
        
        // 添加事件处理
        // lv_obj_add_event_cb(dialog, unlock_dialog_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        
        // 居中显示
        lv_obj_center(dialog);
    }
}

// 解锁对话框事件回调
static void unlock_dialog_event_cb(lv_event_t* e) {
    lv_obj_t* dialog = lv_event_get_target(e);
    
    // 关闭对话框
    lv_msgbox_close(dialog);
    
    // 不重启了，有bug
}

// About页面创建函数 - 重新设计
static lv_obj_t* create_about_page(lv_obj_t* menu) {
    printf("Creating About page\n");
    
    lv_obj_t* page = lv_menu_page_create(menu, "关于本机");
    lv_obj_set_style_pad_hor(page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(page);
    
    // 创建上半部分容器
    lv_obj_t* top_section = lv_obj_create(page);
    lv_obj_set_size(top_section, LV_PCT(100), 200);
    lv_obj_set_style_pad_all(top_section, 10, 0);
    lv_obj_set_style_bg_opa(top_section, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_section, 0, 0);
    lv_obj_set_flex_flow(top_section, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_section, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 创建左侧设备信息卡片（蓝色渐变）
    lv_obj_t* device_card = lv_obj_create(top_section);
    lv_obj_set_size(device_card, LV_PCT(48), 180);
    lv_obj_set_style_radius(device_card, 10, 0);
    lv_obj_set_style_bg_color(device_card, lv_color_hex(0x0078D7), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(device_card, lv_color_hex(0x00BFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(device_card, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(device_card, lv_color_hex(0x9370DB), 0);
    lv_obj_set_style_border_width(device_card, 2, 0);
    lv_obj_set_style_shadow_color(device_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_width(device_card, 15, 0);
    lv_obj_set_style_shadow_ofs_x(device_card, 5, 0);
    lv_obj_set_style_shadow_ofs_y(device_card, 5, 0);
    lv_obj_set_style_shadow_opa(device_card, LV_OPA_30, 0);
    
    // 设备信息标题
    lv_obj_t* device_title = lv_label_create(device_card);
    lv_label_set_text(device_title, "设备信息");
    lv_obj_set_style_text_font(device_title, &simhei_32, 0);
    lv_obj_set_style_text_color(device_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(device_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // 设备信息内容
    lv_obj_t* device_info = lv_label_create(device_card);
    lv_label_set_text(device_info, "设备: M5Tab5\n芯片: ESP32P4\n内存: 512KB\nPSRAM: 32MB\n闪存: 16MB");
    lv_obj_set_style_text_font(device_info, &simhei_32, 0);
    lv_obj_set_style_text_color(device_info, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(device_info, LV_ALIGN_TOP_LEFT, 15, 50);
    
    // 创建右侧系统信息卡片（红色渐变）
    lv_obj_t* system_card = lv_obj_create(top_section);
    lv_obj_set_size(system_card, LV_PCT(48), 180);
    lv_obj_set_style_radius(system_card, 10, 0);
    lv_obj_set_style_bg_color(system_card, lv_color_hex(0xFF4500), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(system_card, lv_color_hex(0xFF6347), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(system_card, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(system_card, lv_color_hex(0x9370DB), 0);
    lv_obj_set_style_border_width(system_card, 2, 0);
    lv_obj_set_style_shadow_color(system_card, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_width(system_card, 15, 0);
    lv_obj_set_style_shadow_ofs_x(system_card, 5, 0);
    lv_obj_set_style_shadow_ofs_y(system_card, 5, 0);
    lv_obj_set_style_shadow_opa(system_card, LV_OPA_30, 0);
    
    // 系统信息标题
    lv_obj_t* system_title = lv_label_create(system_card);
    lv_label_set_text(system_title, "系统信息");
    lv_obj_set_style_text_font(system_title, &simhei_32, 0);
    lv_obj_set_style_text_color(system_title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(system_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // 系统信息内容
    lv_obj_t* system_info = lv_label_create(system_card);
    lv_label_set_text(system_info, "ImOS beta0.1\nbuild 239\n\nKiwiOS Framework: V3\n\n");
    lv_obj_set_style_text_font(system_info, &simhei_32, 0);
    lv_obj_set_style_text_color(system_info, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(system_info, LV_ALIGN_TOP_LEFT, 15, 50);
    
    // 创建下半部分列表
    lv_obj_t* bottom_section = lv_menu_section_create(page);
    
    // 芯片详细信息
    menu_create_text(bottom_section, LV_SYMBOL_SETTINGS, "芯片型号: ESP32-P4", LV_MENU_ITEM_BUILDER_VARIANT_1);
    menu_create_text(bottom_section, LV_SYMBOL_SETTINGS, "CPU: 单核 P4 @ 400MHz", LV_MENU_ITEM_BUILDER_VARIANT_1);
    
    // 可点击的系统版本 - 添加点击事件
    lv_obj_t* version_item = menu_create_text(bottom_section, LV_SYMBOL_SETTINGS, "系统版本: 0.1 build 239", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_obj_add_flag(version_item, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(version_item, version_click_event_cb, LV_EVENT_CLICKED, NULL);
    
    menu_create_text(bottom_section, LV_SYMBOL_SETTINGS, "LVGL版本: 9.2.2", LV_MENU_ITEM_BUILDER_VARIANT_1);
    menu_create_text(bottom_section, LV_SYMBOL_SETTINGS, "ESP-IDF版本: v5.4.1", LV_MENU_ITEM_BUILDER_VARIANT_1);
    
    return page;
}

// Display页面创建函数 - 添加实际功能
static lv_obj_t* create_display_page(lv_obj_t* menu) {
    printf("Creating Display page\n");
    
    lv_obj_t* page = lv_menu_page_create(menu, "显示");
    lv_obj_set_style_pad_hor(page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(page);
    lv_obj_t* section = lv_menu_section_create(page);
    
    // 创建亮度标签
    lv_obj_t* brightness_label = lv_label_create(section);
    lv_label_set_text_fmt(brightness_label, "亮度: %d%%", hal_get_display_brightness());
    lv_obj_set_style_text_font(brightness_label, &simhei_32, 0);
    lv_obj_set_style_pad_all(brightness_label, 10, 0);
    
    // 创建亮度滑块容器
    lv_obj_t* slider_cont = lv_obj_create(section);
    lv_obj_set_size(slider_cont, LV_PCT(100), 60);
    lv_obj_set_style_pad_all(slider_cont, 10, 0);
    lv_obj_set_style_bg_opa(slider_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(slider_cont, 0, 0);
    
    // 创建亮度滑块
    lv_obj_t* brightness_slider = lv_slider_create(slider_cont);
    lv_obj_set_size(brightness_slider, LV_PCT(100), 20);
    lv_slider_set_range(brightness_slider, 20, 100);  // 最低亮度20%
    lv_slider_set_value(brightness_slider, hal_get_display_brightness(), LV_ANIM_OFF);
    
    // 设置亮度滑块样式 (蓝色主题)
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x6699FF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x0066FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x0044CC), LV_PART_KNOB);
    
    // 添加亮度滑块事件
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, brightness_label);
    
    // 添加说明文本
    lv_obj_t* note = lv_label_create(section);
    lv_label_set_text(note, "亮度设置将同步到控制中心");
    lv_obj_set_style_text_font(note, &simhei_32, 0);
    lv_obj_set_style_text_color(note, lv_color_hex(0x888888), 0);
    lv_obj_set_style_pad_all(note, 10, 0);
    
    return page;
}

// Sound页面创建函数 - 添加实际功能
static lv_obj_t* create_sound_page(lv_obj_t* menu) {
    printf("Creating Sound page\n");
    
    lv_obj_t* page = lv_menu_page_create(menu, "声音");
    lv_obj_set_style_pad_hor(page, lv_obj_get_style_pad_left(lv_menu_get_main_header(menu), 0), 0);
    lv_menu_separator_create(page);
    lv_obj_t* section = lv_menu_section_create(page);
    
    // 创建音量标签
    lv_obj_t* volume_label = lv_label_create(section);
    lv_label_set_text_fmt(volume_label, "音量: %d%%", hal_get_speaker_volume());
    lv_obj_set_style_text_font(volume_label, &simhei_32, 0);
    lv_obj_set_style_pad_all(volume_label, 10, 0);
    
    // 创建音量滑块容器
    lv_obj_t* slider_cont = lv_obj_create(section);
    lv_obj_set_size(slider_cont, LV_PCT(100), 60);
    lv_obj_set_style_pad_all(slider_cont, 10, 0);
    lv_obj_set_style_bg_opa(slider_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(slider_cont, 0, 0);
    
    // 创建音量滑块
    lv_obj_t* volume_slider = lv_slider_create(slider_cont);
    lv_obj_set_size(volume_slider, LV_PCT(100), 20);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, hal_get_speaker_volume(), LV_ANIM_OFF);
    
    // 设置音量滑块样式 (橙色主题)
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0xFF9966), LV_PART_MAIN);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0xFF6600), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0xFF4400), LV_PART_KNOB);
    
    // 添加音量滑块事件
    lv_obj_add_event_cb(volume_slider, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, volume_label);
    
    // 创建静音开关容器
    lv_obj_t* switch_cont = lv_obj_create(section);
    lv_obj_set_size(switch_cont, LV_PCT(100), 60);
    lv_obj_set_style_pad_all(switch_cont, 10, 0);
    lv_obj_set_style_bg_opa(switch_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(switch_cont, 0, 0);
    lv_obj_set_flex_flow(switch_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(switch_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 创建静音标签
    lv_obj_t* mute_label = lv_label_create(switch_cont);
    lv_label_set_text(mute_label, "静音");
    lv_obj_set_style_text_font(mute_label, &simhei_32, 0);
    
    // 创建静音开关
    lv_obj_t* speaker_switch = lv_switch_create(switch_cont);
    lv_obj_set_style_pad_left(speaker_switch, 20, 0);
    
    // 设置开关状态
    bool speaker_enabled = hal_get_speaker_enable();
    if (!speaker_enabled) {
        lv_obj_add_state(speaker_switch, LV_STATE_CHECKED);
    }
    
    // 设置开关样式 (绿色主题)
    lv_obj_set_style_bg_color(speaker_switch, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_bg_color(speaker_switch, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(speaker_switch, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    
    // 添加扬声器开关事件
    lv_obj_add_event_cb(speaker_switch, speaker_switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // 添加说明文本
    lv_obj_t* note = lv_label_create(section);
    lv_label_set_text(note, "声音设置将同步到控制中心");
    lv_obj_set_style_text_font(note, &simhei_32, 0);
    lv_obj_set_style_text_color(note, lv_color_hex(0x888888), 0);
    lv_obj_set_style_pad_all(note, 10, 0);
    
    return page;
}

// 页面事件处理器
static void page_event_handler(lv_event_t* e) {
    if (!g_settings_state) return;
    
    // 删除未使用的变量或添加(void)obj;
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
    
    printf("Creating settings app with new menu structure\n");
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
    
    // 创建第一级菜单项 - 使用正确的方法
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
    
    printf("Settings app created with new menu structure\n");
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