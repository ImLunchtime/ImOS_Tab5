#pragma once

#include "overlay_drawer_types.h"
#include "managers/app_manager.h"

// UI创建函数
void drawer_ui_create_container(drawer_state_t* state, app_t* app);
void drawer_ui_create_app_item(lv_obj_t* parent, app_t* app);
void drawer_ui_refresh_app_list(lv_obj_t* list, bool force_refresh);

// 颜色和样式函数
lv_color_t drawer_ui_get_app_color(const char* app_name);

// 清理函数
void drawer_ui_cleanup_app_item(lv_obj_t* item);