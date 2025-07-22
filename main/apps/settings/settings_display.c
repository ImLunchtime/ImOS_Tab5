#include "settings_display.h"
#include "hal.h"
#include "utils/menu_utils.h"
#include <stdio.h>

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

// Display页面创建函数
lv_obj_t* create_display_page(lv_obj_t* menu) {
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