#ifndef SETTINGS_COMMON_H
#define SETTINGS_COMMON_H

#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    lv_obj_t* sidebar_item; // 侧边栏项引用
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

// 全局状态变量声明
extern settings_state_t* g_settings_state;

// 页面创建函数声明
lv_obj_t* create_about_page(lv_obj_t* menu);
lv_obj_t* create_display_page(lv_obj_t* menu);
lv_obj_t* create_sound_page(lv_obj_t* menu);

// 工具函数声明
void update_sidebar_highlight(settings_page_type_t active_page);

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_COMMON_H