#include "settings_sound.h"
#include "hal.h"
#include "utils/menu_utils.h"
#include <stdio.h>

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

// Sound页面创建函数
lv_obj_t* create_sound_page(lv_obj_t* menu) {
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