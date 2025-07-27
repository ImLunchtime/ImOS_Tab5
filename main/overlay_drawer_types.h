#pragma once

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

// 预定义的颜色数组，为应用图标提供不同的颜色
extern const uint32_t app_color_hex[];
extern const size_t app_color_hex_count;

// 抽屉状态结构体
typedef struct {
    lv_obj_t* drawer_container;
    lv_obj_t* app_list;
    lv_obj_t* volume_slider;
    lv_obj_t* brightness_slider;
    lv_obj_t* volume_label;
    lv_obj_t* brightness_label;
    lv_obj_t* speaker_switch;      // 扬声器开关
    lv_obj_t* control_center_btn;  // 控制中心按钮
    lv_obj_t* control_panel;       // 控制面板
    bool is_open;
    bool is_initialized;           // 初始化标志
    bool deep_cleaned;            // 是否进行了深度清理
    bool panel_open;              // 控制面板是否打开
    uint32_t last_open_time;      // 上次打开时间
    uint32_t idle_cleanup_threshold;  // 空闲清理阈值（毫秒）
    lv_anim_t slide_anim;
} drawer_state_t;

// 声明自定义字体
LV_FONT_DECLARE(simhei_32);