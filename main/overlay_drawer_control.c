#include "overlay_drawer_control.h"
#include "overlay_drawer_events.h"
#include "hal.h"
#include <stdio.h>

// 创建控制面板
void drawer_control_create_panel(drawer_state_t* state) {
    if (!state || !state->drawer_container) return;
    
    // 获取屏幕尺寸
    lv_coord_t screen_width = lv_display_get_horizontal_resolution(NULL);
    lv_coord_t screen_height = lv_display_get_vertical_resolution(NULL);
    
    // 创建控制面板（全屏覆盖）
    state->control_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(state->control_panel, screen_width, screen_height);
    lv_obj_set_pos(state->control_panel, 0, 0);
    lv_obj_set_style_bg_color(state->control_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(state->control_panel, LV_OPA_50, 0);  // 半透明背景
    lv_obj_set_style_border_width(state->control_panel, 0, 0);
    lv_obj_set_style_pad_all(state->control_panel, 0, 0);
    lv_obj_add_flag(state->control_panel, LV_OBJ_FLAG_HIDDEN);  // 初始隐藏
    lv_obj_clear_flag(state->control_panel, LV_OBJ_FLAG_SCROLLABLE);
    
    // 点击背景关闭面板
    lv_obj_add_event_cb(state->control_panel, drawer_events_control_panel_close_cb, LV_EVENT_CLICKED, state);
    
    // 创建控制面板内容容器
    lv_obj_t* panel_content = lv_obj_create(state->control_panel);
    lv_obj_set_size(panel_content, 400, 300);
    lv_obj_align(panel_content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(panel_content, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(panel_content, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel_content, 1, 0);
    lv_obj_set_style_border_color(panel_content, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_radius(panel_content, 12, 0);
    lv_obj_set_style_pad_all(panel_content, 0, 0);  // 移除所有padding
    lv_obj_clear_flag(panel_content, LV_OBJ_FLAG_SCROLLABLE);
    
    // 阻止事件冒泡到背景
    lv_obj_clear_flag(panel_content, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    // 面板标题
    lv_obj_t* title = lv_label_create(panel_content);
    lv_label_set_text(title, "控制中心");
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(title, &simhei_32, 0);
    lv_obj_set_pos(title, 20, 15);  // 直接设置标题位置
    
    // 音量标签
    state->volume_label = lv_label_create(panel_content);
    lv_label_set_text_fmt(state->volume_label, "音量: %d%%", hal_get_speaker_volume());
    lv_obj_set_style_text_color(state->volume_label, lv_color_hex(0xFF6600), 0);
    lv_obj_set_style_text_font(state->volume_label, &simhei_32, 0);
    lv_obj_set_pos(state->volume_label, 20, 70);  // 直接设置位置
    
    // 音量滑块
    state->volume_slider = lv_slider_create(panel_content);
    lv_obj_set_size(state->volume_slider, 200, 18);
    lv_obj_set_pos(state->volume_slider, 20, 105);  // 直接设置位置
    lv_slider_set_range(state->volume_slider, 0, 100);
    lv_slider_set_value(state->volume_slider, hal_get_speaker_volume(), LV_ANIM_OFF);
    
    // 设置音量滑块样式
    lv_obj_set_style_bg_color(state->volume_slider, lv_color_hex(0xFF9966), LV_PART_MAIN);
    lv_obj_set_style_bg_color(state->volume_slider, lv_color_hex(0xFF6600), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(state->volume_slider, lv_color_hex(0xFF4400), LV_PART_KNOB);
    
    lv_obj_add_event_cb(state->volume_slider, drawer_events_volume_slider_cb, LV_EVENT_VALUE_CHANGED, state);
    
    // 扬声器开关
    state->speaker_switch = lv_switch_create(panel_content);
    lv_obj_set_size(state->speaker_switch, 50, 25);
    lv_obj_set_pos(state->speaker_switch, 230, 100);  // 直接设置位置，紧贴滑块右侧
    
    bool speaker_enabled = hal_get_speaker_enable();
    if (speaker_enabled) {
        lv_obj_add_state(state->speaker_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(state->speaker_switch, LV_STATE_CHECKED);
    }
    
    lv_obj_set_style_bg_color(state->speaker_switch, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_bg_color(state->speaker_switch, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(state->speaker_switch, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    
    lv_obj_add_flag(state->speaker_switch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(state->speaker_switch, drawer_events_speaker_switch_cb, LV_EVENT_VALUE_CHANGED, state);
    lv_obj_add_event_cb(state->speaker_switch, drawer_events_speaker_switch_cb, LV_EVENT_CLICKED, state);
    
    // 亮度标签
    state->brightness_label = lv_label_create(panel_content);
    lv_label_set_text_fmt(state->brightness_label, "亮度: %d%%", hal_get_display_brightness());
    lv_obj_set_style_text_color(state->brightness_label, lv_color_hex(0x0066FF), 0);
    lv_obj_set_style_text_font(state->brightness_label, &simhei_32, 0);
    lv_obj_set_pos(state->brightness_label, 20, 160);  // 直接设置位置
    
    // 亮度滑块
    state->brightness_slider = lv_slider_create(panel_content);
    lv_obj_set_size(state->brightness_slider, 280, 18);  // 固定宽度280像素
    lv_obj_set_pos(state->brightness_slider, 20, 195);  // 直接设置位置
    lv_slider_set_range(state->brightness_slider, 20, 100);
    lv_slider_set_value(state->brightness_slider, hal_get_display_brightness(), LV_ANIM_OFF);
    
    lv_obj_set_style_bg_color(state->brightness_slider, lv_color_hex(0x6699FF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(state->brightness_slider, lv_color_hex(0x0066FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(state->brightness_slider, lv_color_hex(0x0044CC), LV_PART_KNOB);
    
    lv_obj_add_event_cb(state->brightness_slider, drawer_events_brightness_slider_cb, LV_EVENT_VALUE_CHANGED, state);
    
    state->panel_open = false;
}