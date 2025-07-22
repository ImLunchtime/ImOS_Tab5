#include "settings_about.h"
#include "managers/content_lock.h"
#include "hal.h"
#include "utils/menu_utils.h"
#include <stdio.h>
#include <esp_system.h>

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
        snprintf(msg, sizeof(msg), "请点击电源键重启以应用更改，当前解锁状态为 %s", unlocked ? "已解锁" : "未解锁");
        lv_msgbox_add_text(dialog, msg);
        
        // 居中显示
        lv_obj_center(dialog);
    }
}

// About页面创建函数
lv_obj_t* create_about_page(lv_obj_t* menu) {
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