#include "overlay_drawer_ui.h"
#include "overlay_drawer_events.h"
#include "managers/content_lock.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 预定义的颜色数组，为应用图标提供不同的颜色
const uint32_t app_color_hex[] = {
    0xFF6B6B,  // 红色
    0x4ECDC4,  // 青色
    0x45B7D1,  // 蓝色
    0x96CEB4,  // 绿色
    0xFFEAA7,  // 黄色
    0xDDA0DD,  // 紫色
    0xFFB347,  // 橙色
    0x87CEEB,  // 天蓝色
    0xFF69B4,  // 粉色
    0x20B2AA,  // 海绿色
    0x9370DB,  // 中紫色
    0x32CD32,  // 酸橙绿
};

const size_t app_color_hex_count = sizeof(app_color_hex) / sizeof(app_color_hex[0]);

// 获取应用的颜色（基于应用名称的哈希值）
lv_color_t drawer_ui_get_app_color(const char* app_name) {
    if (!app_name) {
        return lv_color_hex(app_color_hex[0]);
    }
    
    // 简单的哈希算法
    uint32_t hash = 0;
    for (int i = 0; app_name[i] != '\0'; i++) {
        hash = ((hash << 5) + hash) + app_name[i];
    }
    
    uint32_t color_index = hash % app_color_hex_count;
    return lv_color_hex(app_color_hex[color_index]);
}

// 创建应用项 - 新的按钮样式设计
void drawer_ui_create_app_item(lv_obj_t* parent, app_t* app) {
    if (!parent || !app) {
        return;
    }
    
    // 创建应用按钮容器
    lv_obj_t* button_container = lv_obj_create(parent);
    lv_obj_set_size(button_container, LV_PCT(100), 70);  // 增加高度适配高分辨率
    
    // 设置按钮样式：无背景、无边框、无阴影
    lv_obj_set_style_bg_opa(button_container, LV_OPA_TRANSP, 0);  // 透明背景
    lv_obj_set_style_border_width(button_container, 0, 0);  // 无边框
    lv_obj_set_style_shadow_width(button_container, 0, 0);  // 无阴影
    lv_obj_set_style_pad_all(button_container, 0, 0);
    
    // 按压效果：淡灰色背景
    lv_obj_set_style_bg_color(button_container, lv_color_hex(0xDDDDDD), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button_container, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_radius(button_container, 8, LV_STATE_PRESSED);  // 按压时圆角
    
    // 让容器可以接收点击事件
    lv_obj_add_flag(button_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(button_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(button_container, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    // 获取应用的颜色
    lv_color_t app_color = drawer_ui_get_app_color(app->name);
    
    // 计算颜色索引用于打印
    uint32_t hash = 0;
    for (int i = 0; app->name[i] != '\0'; i++) {
        hash = ((hash << 5) + hash) + app->name[i];
    }
    uint32_t color_index = hash % app_color_hex_count;
    
    // 创建图标容器（圆形背景）
    lv_obj_t* icon_container = lv_obj_create(button_container);
    lv_obj_set_size(icon_container, 50, 50);  // 50x50像素的圆形
    lv_obj_align(icon_container, LV_ALIGN_LEFT_MID, 16, 0);
    
    // 设置圆形背景
    lv_obj_set_style_radius(icon_container, 25, 0);  // 25像素半径，形成圆形
    lv_obj_set_style_bg_color(icon_container, app_color, 0);  // 使用应用颜色
    lv_obj_set_style_bg_opa(icon_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(icon_container, 0, 0);
    lv_obj_set_style_pad_all(icon_container, 0, 0);
    
    // 创建应用图标
    lv_obj_t* icon = lv_label_create(icon_container);
    if (app->icon[0] != '\0') {
        lv_label_set_text(icon, app->icon);
    } else {
        // 如果没有图标，使用应用名称的第一个字符
        char first_char[2] = {app->name[0], '\0'};
        lv_label_set_text(icon, first_char);
    }
    
    // 设置图标样式：白色文字，居中显示
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFFFFFF), 0);  // 白色图标
    lv_obj_set_style_text_font(icon, &simhei_32, 0);  // 大图标字体
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(icon, 0, 0);
    
    // 创建应用名称标签
    lv_obj_t* name_label = lv_label_create(button_container);
    lv_label_set_text(name_label, app->name);
    lv_obj_set_style_text_color(name_label, lv_color_hex(0x333333), 0);  // 深色文字
    lv_obj_set_style_text_font(name_label, &simhei_32, 0);  // 使用中文字体
    lv_obj_set_style_pad_all(name_label, 0, 0);
    
    // 将文字放在图标右侧
    lv_obj_align_to(name_label, icon_container, LV_ALIGN_OUT_RIGHT_MID, 16, 0);
    
    // 添加点击事件到按钮容器
    lv_obj_add_event_cb(button_container, drawer_events_app_item_cb, LV_EVENT_CLICKED, app);
    
    printf("Created app button: %s with color 0x%06lX\n", app->name, app_color_hex[color_index]);
}

// 清理应用项内存（现在已经不需要清理字符串了）
void drawer_ui_cleanup_app_item(lv_obj_t* item) {
    // 现在使用应用指针作为用户数据，不需要释放内存
    (void)item; // 标记参数未使用
}

// 检查应用是否需要内容锁
static bool app_requires_content_lock(const char* app_name) {
    if (!app_name) {
        return false;
    }
    
    // 检查是否是需要内容锁的应用
    if (strcmp(app_name, "Ark") == 0) {
        return true;
    }
    
    // 可以在这里添加其他需要内容锁的应用
    
    return false;
}

// 智能刷新应用列表 - 只在需要时创建/更新
void drawer_ui_refresh_app_list(lv_obj_t* list, bool force_refresh) {
    if (!list) {
        printf("Error: app list container is NULL\n");
        return;
    }
    
    // 验证对象有效性
    if (!lv_obj_is_valid(list)) {
        printf("Error: app list container is invalid\n");
        return;
    }
    
    // 如果不是强制刷新且已有子项，跳过创建
    if (!force_refresh && lv_obj_get_child_count(list) > 0) {
        printf("App list already populated, skipping refresh\n");
        return;
    }
    
    printf("Refreshing app list...\n");
    
    // 暂停LVGL渲染，避免在创建过程中触发布局更新
    lv_disp_t* disp = lv_obj_get_disp(list);
    if (disp) {
        lv_disp_enable_invalidation(disp, false);
    }
    
    // 清理现有应用项的内存（如果有的话）
    uint32_t child_count = lv_obj_get_child_count(list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(list, i);
        drawer_ui_cleanup_app_item(child);
    }
    
    // 清空现有列表
    lv_obj_clean(list);
    
    // 确保列表对象仍然有效
    if (!lv_obj_is_valid(list)) {
        printf("Error: app list became invalid after cleaning\n");
        if (disp) {
            lv_disp_enable_invalidation(disp, true);
        }
        return;
    }
    
    // 检查内容锁状态
    bool content_unlocked = content_lock_is_unlocked();
    printf("Content lock status: %s\n", content_unlocked ? "unlocked" : "locked");
    
    // 添加所有应用
    app_t* app = app_manager_get_app_list();
    int app_count = 0;
    while (app) {
        // 验证应用数据有效性 - 修复：name是数组，只检查第一个字符
        if (app->name[0] == '\0') {
            printf("Warning: skipping app with empty name\n");
            app = app->next;
            continue;
        }
        
        // 检查应用是否需要内容锁
        bool requires_lock = app_requires_content_lock(app->name);
        
        // 如果应用需要内容锁但内容锁未解锁，则跳过该应用
        if (requires_lock && !content_unlocked) {
            printf("Skipping locked app: %s (content lock required)\n", app->name);
            app = app->next;
            continue;
        }
        
        printf("Adding app to list: %s\n", app->name);
        
        // 创建应用项前再次验证列表有效性
        if (!lv_obj_is_valid(list)) {
            printf("Error: app list became invalid during creation\n");
            break;
        }
        
        drawer_ui_create_app_item(list, app);
        app_count++;
        app = app->next;
        
        // 在每个应用项创建后添加小延迟，避免过快创建导致内存访问冲突
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    // 恢复LVGL渲染
    if (disp) {
        lv_disp_enable_invalidation(disp, true);
    }
    
    // 强制刷新显示
    lv_obj_invalidate(list);
    
    printf("Total apps added to list: %d\n", app_count);
}

// 创建抽屉容器和基本UI
void drawer_ui_create_container(drawer_state_t* state, app_t* app) {
    if (!state || !app || !app->container) {
        return;
    }
    
    // 获取屏幕尺寸
    lv_coord_t screen_width = lv_display_get_horizontal_resolution(NULL);
    lv_coord_t screen_height = lv_display_get_vertical_resolution(NULL);
    lv_coord_t drawer_width = screen_width / 4;  // 1/4屏幕宽度
    
    // 创建抽屉容器
    state->drawer_container = lv_obj_create(app->container);
    lv_obj_set_size(state->drawer_container, drawer_width, screen_height);
    lv_obj_set_pos(state->drawer_container, -drawer_width, 0);  // 初始位置在屏幕左侧外
    lv_obj_set_style_bg_color(state->drawer_container, lv_color_hex(0xF5F5F5), 0);  // 浅色主题
    lv_obj_set_style_bg_opa(state->drawer_container, LV_OPA_COVER, 0);  // 完全不透明
    lv_obj_set_style_border_width(state->drawer_container, 1, 0);
    lv_obj_set_style_border_color(state->drawer_container, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_pad_all(state->drawer_container, 0, 0);
    lv_obj_add_flag(state->drawer_container, LV_OBJ_FLAG_HIDDEN);  // 初始隐藏
    
    // 确保抽屉容器不会传播点击事件到背景
    lv_obj_clear_flag(state->drawer_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(state->drawer_container, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    // 创建标题
    lv_obj_t* title = lv_label_create(state->drawer_container);
    lv_label_set_text(title, "应用");
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);  // 深色文字适配浅色主题
    lv_obj_set_style_text_font(title, &simhei_32, 0);  // 使用中文字体
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, 0);  // 左对齐
    lv_obj_set_style_pad_all(title, 20, 0);  // 增加内边距
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 创建应用列表 (为底部控制中心按钮留出空间)
    state->app_list = lv_obj_create(state->drawer_container);
    lv_obj_set_size(state->app_list, LV_PCT(100), screen_height - 150);  // 为控制中心按钮留出空间
    lv_obj_align_to(state->app_list, title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_obj_set_style_bg_opa(state->app_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(state->app_list, 0, 0);
    lv_obj_set_style_pad_all(state->app_list, 15, 0);
    
    // 确保列表容器不阻挡事件传播
    lv_obj_clear_flag(state->app_list, LV_OBJ_FLAG_EVENT_BUBBLE);
    // 启用滚动功能
    lv_obj_add_flag(state->app_list, LV_OBJ_FLAG_SCROLLABLE);
    // 设置滚动方向仅为垂直方向
    lv_obj_set_scroll_dir(state->app_list, LV_DIR_VER);
    // 设置滚动条样式
    lv_obj_set_style_bg_opa(state->app_list, LV_OPA_10, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(state->app_list, 8, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(state->app_list, 4, LV_PART_SCROLLBAR);
    
    // 设置列表布局
    lv_obj_set_layout(state->app_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(state->app_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(state->app_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(state->app_list, 16, 0);  // 增加项目间距适配新的按钮设计
    
    // 创建控制中心按钮
    state->control_center_btn = lv_btn_create(state->drawer_container);
    lv_obj_set_size(state->control_center_btn, LV_PCT(90), 50);
    lv_obj_align_to(state->control_center_btn, state->app_list, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_set_style_bg_color(state->control_center_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_radius(state->control_center_btn, 8, 0);
    
    // 控制中心按钮标签
    lv_obj_t* btn_label = lv_label_create(state->control_center_btn);
    lv_label_set_text(btn_label, "控制中心");
    lv_obj_set_style_text_color(btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(btn_label, &simhei_32, 0);
    lv_obj_center(btn_label);
    
    // 添加按钮点击事件
    lv_obj_add_event_cb(state->control_center_btn, drawer_events_control_center_btn_cb, LV_EVENT_CLICKED, state);
}