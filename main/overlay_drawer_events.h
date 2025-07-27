#pragma once

#include "overlay_drawer_types.h"
#include "managers/app_manager.h"

// 事件回调函数
void drawer_events_app_item_cb(lv_event_t* e);
void drawer_events_background_cb(lv_event_t* e);
void drawer_events_volume_slider_cb(lv_event_t* e);
void drawer_events_brightness_slider_cb(lv_event_t* e);
void drawer_events_speaker_switch_cb(lv_event_t* e);
void drawer_events_control_center_btn_cb(lv_event_t* e);
void drawer_events_control_panel_close_cb(lv_event_t* e);

// 动画回调函数
void drawer_events_slide_anim_ready_cb(lv_anim_t* a);