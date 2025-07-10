#pragma once

#include <lvgl.h>
#include <stdbool.h>

typedef enum {
    LV_MENU_ITEM_BUILDER_VARIANT_1,
    LV_MENU_ITEM_BUILDER_VARIANT_2
} lv_menu_builder_variant_t;

// 全局变量声明
extern lv_obj_t * root_page;

// 事件处理函数
void menu_back_event_handler(lv_event_t * e);
void menu_switch_handler(lv_event_t * e);

// 菜单项创建函数
lv_obj_t * menu_create_text(lv_obj_t * parent, const char * icon, const char * txt,
                              lv_menu_builder_variant_t builder_variant);
lv_obj_t * menu_create_slider(lv_obj_t * parent, const char * icon, const char * txt, 
                                int32_t min, int32_t max, int32_t val);
lv_obj_t * menu_create_switch(lv_obj_t * parent, const char * icon, const char * txt, bool chk); 