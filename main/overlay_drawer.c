#include "overlay_drawer.h"
#include "app_manager.h"
#include "gesture_handler.h"
#include "hal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 声明自定义字体
LV_FONT_DECLARE(simhei_32);

// 预定义的颜色数组，为应用图标提供不同的颜色
static const uint32_t app_color_hex[] = {
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

// 获取应用的颜色（基于应用名称的哈希值）
static lv_color_t get_app_color(const char* app_name) {
    if (!app_name) {
        return lv_color_hex(app_color_hex[0]);
    }
    
    // 简单的哈希算法
    uint32_t hash = 0;
    for (int i = 0; app_name[i] != '\0'; i++) {
        hash = ((hash << 5) + hash) + app_name[i];
    }
    
    uint32_t color_index = hash % (sizeof(app_color_hex) / sizeof(app_color_hex[0]));
    return lv_color_hex(app_color_hex[color_index]);
}

// 抽屉状态
typedef struct {
    lv_obj_t* drawer_container;
    lv_obj_t* app_list;
    //lv_obj_t* background;
    lv_obj_t* volume_slider;
    lv_obj_t* brightness_slider;
    lv_obj_t* volume_label;
    lv_obj_t* brightness_label;
    lv_obj_t* speaker_switch;  // 扬声器开关
    bool is_open;
    bool is_initialized;  // 添加初始化标志
    bool deep_cleaned;    // 是否进行了深度清理
    uint32_t last_open_time;  // 上次打开时间
    uint32_t idle_cleanup_threshold;  // 空闲清理阈值（毫秒）
    lv_anim_t slide_anim;
} drawer_state_t;

// 应用项点击事件
static void app_item_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    app_t* app = (app_t*)lv_event_get_user_data(e);
    
    if (code == LV_EVENT_CLICKED && app) {
        printf("*** APP ITEM CLICKED: %s ***\n", app->name);
        
        // 启动应用
        bool success = app_manager_launch_app(app->name);
        printf("App launch %s: %s\n", app->name, success ? "success" : "failed");
        
        // 关闭抽屉
        app_drawer_close();
    }
}

// 背景点击事件（关闭抽屉）
__attribute__((unused)) static void background_event_cb(lv_event_t* e) {
    printf("Background clicked - closing drawer\n");
    app_drawer_close();
}

// 动画结束回调
static void slide_anim_ready_cb(lv_anim_t* a) {
    drawer_state_t* state = (drawer_state_t*)a->user_data;
    if (!state->is_open) {
        // 抽屉关闭后隐藏背景
        //lv_obj_add_flag(state->background, LV_OBJ_FLAG_HIDDEN);
        // 同时隐藏抽屉容器，确保不会拦截事件
        lv_obj_add_flag(state->drawer_container, LV_OBJ_FLAG_HIDDEN);
        printf("Drawer completely closed and hidden\n");
        
        // 标记可以安全清理（但不立即清理）
        state->last_open_time = esp_timer_get_time() / 1000;
    }
}

// 音量滑块事件回调
static void volume_slider_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* slider = lv_event_get_target(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        drawer_state_t* state = (drawer_state_t*)lv_event_get_user_data(e);
        if (state && state->volume_label) {
            int32_t value = lv_slider_get_value(slider);
            hal_set_speaker_volume((uint8_t)value);
            
            // 更新标签显示
            lv_label_set_text_fmt(state->volume_label, "音量: %d%%", (int)value);
            printf("Volume changed to: %d%%\n", (int)value);
        }
    }
}

// 亮度滑块事件回调
static void brightness_slider_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* slider = lv_event_get_target(e);
    
    if (code == LV_EVENT_VALUE_CHANGED) {
        drawer_state_t* state = (drawer_state_t*)lv_event_get_user_data(e);
        if (state && state->brightness_label) {
            int32_t value = lv_slider_get_value(slider);
            hal_set_display_brightness((uint8_t)value);
            
            // 更新标签显示
            lv_label_set_text_fmt(state->brightness_label, "亮度: %d%%", (int)value);
            printf("Brightness changed to: %d%%\n", (int)value);
        }
    }
}

// 扬声器开关事件回调
static void speaker_switch_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* switch_obj = lv_event_get_target(e);
    
    printf("Speaker switch event: code=%d\n", code);
    
    if (code == LV_EVENT_VALUE_CHANGED || code == LV_EVENT_CLICKED) {
        drawer_state_t* state = (drawer_state_t*)lv_event_get_user_data(e);
        if (state) {
            // 获取开关的当前状态
            bool enabled = lv_obj_has_state(switch_obj, LV_STATE_CHECKED);
            printf("Switch state: %s\n", enabled ? "checked" : "unchecked");
            
            // 调用HAL函数设置扬声器状态
            hal_set_speaker_enable(enabled);
            
            printf("Speaker %s\n", enabled ? "enabled" : "disabled");
            
            // 强制刷新显示
            lv_obj_invalidate(switch_obj);
        }
    }
}

// 创建应用项 - 新的按钮样式设计
static void create_app_item(lv_obj_t* parent, app_t* app) {
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
    lv_color_t app_color = get_app_color(app->name);
    
    // 计算颜色索引用于打印
    uint32_t hash = 0;
    for (int i = 0; app->name[i] != '\0'; i++) {
        hash = ((hash << 5) + hash) + app->name[i];
    }
    uint32_t color_index = hash % (sizeof(app_color_hex) / sizeof(app_color_hex[0]));
    
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
    lv_obj_add_event_cb(button_container, app_item_event_cb, LV_EVENT_CLICKED, app);
    
    printf("Created app button: %s with color 0x%06lX\n", app->name, app_color_hex[color_index]);
}

// 清理应用项内存（现在已经不需要清理字符串了）
static void cleanup_app_item(lv_obj_t* item) {
    // 现在使用应用指针作为用户数据，不需要释放内存
    (void)item; // 标记参数未使用
}

// 智能刷新应用列表 - 只在需要时创建/更新
static void refresh_app_list(lv_obj_t* list, bool force_refresh) {
    if (!list) {
        printf("Error: app list container is NULL\n");
        return;
    }
    
    // 如果不是强制刷新且已有子项，跳过创建
    if (!force_refresh && lv_obj_get_child_count(list) > 0) {
        printf("App list already populated, skipping refresh\n");
        return;
    }
    
    printf("Refreshing app list...\n");
    
    // 清理现有应用项的内存（如果有的话）
    uint32_t child_count = lv_obj_get_child_count(list);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t* child = lv_obj_get_child(list, i);
        cleanup_app_item(child);
    }
    
    // 清空现有列表
    lv_obj_clean(list);
    
    // 添加所有应用
    app_t* app = app_manager_get_app_list();
    int app_count = 0;
    while (app) {
        printf("Adding app to list: %s\n", app->name);
        create_app_item(list, app);
        app_count++;
        app = app->next;
    }
    
    printf("Total apps added to list: %d\n", app_count);
}

// 创建应用抽屉Overlay
static void drawer_overlay_create(app_t* app) {
    if (!app || !app->container) {
        return;
    }
    
    printf("Creating app drawer overlay with smart memory management\n");
    app_manager_log_memory_usage("Before drawer creation");
    
    // 创建抽屉状态
    drawer_state_t* state = (drawer_state_t*)malloc(sizeof(drawer_state_t));
    if (!state) {
        printf("Failed to allocate drawer state\n");
        return;
    }
    memset(state, 0, sizeof(drawer_state_t));
    
    // 初始化内存管理参数
    state->idle_cleanup_threshold = 30000; // 30秒后清理
    state->last_open_time = 0;
    state->deep_cleaned = false;
    
    // 获取屏幕尺寸
    lv_coord_t screen_width = lv_display_get_horizontal_resolution(NULL);
    lv_coord_t screen_height = lv_display_get_vertical_resolution(NULL);
    lv_coord_t drawer_width = screen_width / 4;  // 1/4屏幕宽度
    
    // 先创建简化的半透明背景
    // state->background = lv_obj_create(app->container);
    // lv_obj_set_size(state->background, LV_PCT(100), LV_PCT(100));
    // lv_obj_set_pos(state->background, 0, 0);
    // lv_obj_set_style_bg_color(state->background, lv_color_hex(0x000000), 0);
    // lv_obj_set_style_bg_opa(state->background, LV_OPA_30, 0);  // 降低透明度减少GPU负担
    // lv_obj_set_style_border_width(state->background, 0, 0);
    // lv_obj_set_style_pad_all(state->background, 0, 0);
    // lv_obj_add_flag(state->background, LV_OBJ_FLAG_HIDDEN);  // 初始隐藏
    
    // 确保背景能接收点击事件
    //lv_obj_clear_flag(state->background, LV_OBJ_FLAG_SCROLLABLE);
    //lv_obj_add_event_cb(state->background, background_event_cb, LV_EVENT_CLICKED, NULL);
    
    // 创建抽屉容器（在背景之后创建，确保在上层）
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
    
    // 创建应用列表 (为底部滑块留出更多空间)
    state->app_list = lv_obj_create(state->drawer_container);
    lv_obj_set_size(state->app_list, LV_PCT(100), screen_height - 250);  // 增加底部空间到250像素
    lv_obj_align_to(state->app_list, title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);  // 增加与标题的间距
    lv_obj_set_style_bg_opa(state->app_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(state->app_list, 0, 0);
    lv_obj_set_style_pad_all(state->app_list, 15, 0);  // 增加内边距适配高分辨率
    
    // 确保列表容器不阻挡事件传播
    lv_obj_clear_flag(state->app_list, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(state->app_list, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动避免事件冲突
    
    // 设置列表布局
    lv_obj_set_layout(state->app_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(state->app_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(state->app_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(state->app_list, 16, 0);  // 增加项目间距适配新的按钮设计
    
    // 创建音量控制区域
    lv_obj_t* volume_container = lv_obj_create(state->drawer_container);
    lv_obj_set_size(volume_container, LV_PCT(90), 80);  // 增加容器高度到60像素
    lv_obj_align_to(volume_container, state->app_list, LV_ALIGN_OUT_BOTTOM_LEFT, 10, 15);  // 增加间距到15像素
    lv_obj_set_style_bg_opa(volume_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(volume_container, 0, 0);
    lv_obj_set_style_pad_all(volume_container, 8, 0);  // 增加内边距到8像素
    
    // 确保音量容器不会阻止事件传递
    lv_obj_clear_flag(volume_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(volume_container, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    // 音量标签
    state->volume_label = lv_label_create(volume_container);
    lv_label_set_text_fmt(state->volume_label, "音量: %d%%", hal_get_speaker_volume());
    lv_obj_set_style_text_color(state->volume_label, lv_color_hex(0xFF6600), 0);  // 橙色
    lv_obj_set_style_text_font(state->volume_label, &simhei_32, 0);  // 使用中文字体
    lv_obj_align(state->volume_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 音量滑块 (调窄一些，为扬声器开关留出空间)
    state->volume_slider = lv_slider_create(volume_container);
    lv_obj_set_size(state->volume_slider, LV_PCT(50), 18);  // 调窄到50%宽度，为开关留出更多空间
    lv_obj_align_to(state->volume_slider, state->volume_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);  // 增加间距到8像素
    lv_slider_set_range(state->volume_slider, 0, 100);
    lv_slider_set_value(state->volume_slider, hal_get_speaker_volume(), LV_ANIM_OFF);
    
    // 设置音量滑块样式 (橙色主题)
    lv_obj_set_style_bg_color(state->volume_slider, lv_color_hex(0xFF9966), LV_PART_MAIN);
    lv_obj_set_style_bg_color(state->volume_slider, lv_color_hex(0xFF6600), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(state->volume_slider, lv_color_hex(0xFF4400), LV_PART_KNOB);
    
    // 添加音量滑块事件
    lv_obj_add_event_cb(state->volume_slider, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, state);
    
    // 扬声器开关 (移除标签，只保留开关)
    state->speaker_switch = lv_switch_create(volume_container);
    lv_obj_set_size(state->speaker_switch, 50, 25);  // 调整开关大小
    lv_obj_align_to(state->speaker_switch, state->volume_slider, LV_ALIGN_OUT_RIGHT_MID, 15, 0);  // 放在滑块右边，减少间距
    
    // 设置开关状态
    bool speaker_enabled = hal_get_speaker_enable();
    printf("Initial speaker state: %s\n", speaker_enabled ? "enabled" : "disabled");
    if (speaker_enabled) {
        lv_obj_add_state(state->speaker_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(state->speaker_switch, LV_STATE_CHECKED);
    }
    
    // 设置开关样式 (绿色主题)
    lv_obj_set_style_bg_color(state->speaker_switch, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_bg_color(state->speaker_switch, lv_color_hex(0x00AA00), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(state->speaker_switch, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    
    // 确保开关可以点击
    lv_obj_add_flag(state->speaker_switch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(state->speaker_switch, LV_OBJ_FLAG_SCROLLABLE);
    
    // 添加扬声器开关事件 - 监听多种事件类型
    lv_obj_add_event_cb(state->speaker_switch, speaker_switch_event_cb, LV_EVENT_VALUE_CHANGED, state);
    lv_obj_add_event_cb(state->speaker_switch, speaker_switch_event_cb, LV_EVENT_CLICKED, state);
    
    printf("Speaker switch created and configured\n");
    
    // 测试：验证开关是否可点击
    printf("Testing switch clickability...\n");
    bool is_clickable = lv_obj_has_flag(state->speaker_switch, LV_OBJ_FLAG_CLICKABLE);
    printf("Switch clickable: %s\n", is_clickable ? "true" : "false");
    
    // 调试：检查开关位置和大小
    lv_coord_t switch_x = lv_obj_get_x(state->speaker_switch);
    lv_coord_t switch_y = lv_obj_get_y(state->speaker_switch);
    lv_coord_t switch_w = lv_obj_get_width(state->speaker_switch);
    lv_coord_t switch_h = lv_obj_get_height(state->speaker_switch);
    printf("Switch position: x=%ld, y=%ld, w=%ld, h=%ld\n", switch_x, switch_y, switch_w, switch_h);
    
    // 创建亮度控制区域
    lv_obj_t* brightness_container = lv_obj_create(state->drawer_container);
    lv_obj_set_size(brightness_container, LV_PCT(90), 80);  // 增加容器高度到60像素
    lv_obj_align_to(brightness_container, volume_container, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 15);  // 增加间距到15像素
    lv_obj_set_style_bg_opa(brightness_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(brightness_container, 0, 0);
    lv_obj_set_style_pad_all(brightness_container, 4, 0);  // 增加内边距到8像素
    
    // 亮度标签
    state->brightness_label = lv_label_create(brightness_container);
    lv_label_set_text_fmt(state->brightness_label, "亮度: %d%%", hal_get_display_brightness());
    lv_obj_set_style_text_color(state->brightness_label, lv_color_hex(0x0066FF), 0);  // 蓝色
    lv_obj_set_style_text_font(state->brightness_label, &simhei_32, 0);  // 使用中文字体
    lv_obj_align(state->brightness_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 亮度滑块
    state->brightness_slider = lv_slider_create(brightness_container);
    lv_obj_set_size(state->brightness_slider, LV_PCT(100), 18);  // 调整滑块高度到22像素
    lv_obj_align_to(state->brightness_slider, state->brightness_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);  // 增加间距到8像素
    lv_slider_set_range(state->brightness_slider, 20, 100);  // 最低亮度20%
    lv_slider_set_value(state->brightness_slider, hal_get_display_brightness(), LV_ANIM_OFF);
    
    // 设置亮度滑块样式 (蓝色主题)
    lv_obj_set_style_bg_color(state->brightness_slider, lv_color_hex(0x6699FF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(state->brightness_slider, lv_color_hex(0x0066FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(state->brightness_slider, lv_color_hex(0x0044CC), LV_PART_KNOB);
    
    // 添加亮度滑块事件
    lv_obj_add_event_cb(state->brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, state);
    
    // 不在创建时刷新应用列表，延迟到第一次打开
    // refresh_app_list(state->app_list, false); // 移除这行
    
    // 保存状态到用户数据
    app->user_data = state;
}

// 销毁应用抽屉Overlay
static void drawer_overlay_destroy(app_t* app) {
    if (app && app->user_data) {
        drawer_state_t* state = (drawer_state_t*)app->user_data;
        
        // 清理应用列表中的内存
        if (state->app_list) {
            uint32_t child_count = lv_obj_get_child_count(state->app_list);
            for (uint32_t i = 0; i < child_count; i++) {
                lv_obj_t* child = lv_obj_get_child(state->app_list, i);
                cleanup_app_item(child);
            }
        }
        
        // 确保LVGL任务完成
        lv_refr_now(NULL);
        vTaskDelay(pdMS_TO_TICKS(5));
        
        free(state);
        app->user_data = NULL;
    }
}

// 打开抽屉
void app_drawer_open(void) {
    printf("Opening app drawer...\n");
    overlay_t* overlay = app_manager_get_overlay("AppDrawer");
    if (!overlay || !overlay->base.user_data) {
        printf("Error: AppDrawer overlay not found or no user data\n");
        return;
    }
    
    drawer_state_t* state = (drawer_state_t*)overlay->base.user_data;
    if (state->is_open) {
        printf("App drawer is already open\n");
        return;
    }
    
    // 记录打开时间
    state->last_open_time = esp_timer_get_time() / 1000;
    
    // 延迟加载：只在第一次打开或被清理后重新创建应用列表
    if (!state->is_initialized || state->deep_cleaned) {
        printf("Creating/recreating app list (initialized: %s, deep_cleaned: %s)\n",
               state->is_initialized ? "true" : "false",
               state->deep_cleaned ? "true" : "false");
        app_manager_log_memory_usage("Before app list creation");
        
        refresh_app_list(state->app_list, true);
        state->is_initialized = true;
        state->deep_cleaned = false;
        
        app_manager_log_memory_usage("After app list creation");
    }
    
    // 显示背景
    //lv_obj_clear_flag(state->background, LV_OBJ_FLAG_HIDDEN);
    
    // 确保抽屉容器可见
    lv_obj_clear_flag(state->drawer_container, LV_OBJ_FLAG_HIDDEN);
    
    // 强制刷新显示
    lv_obj_invalidate(state->drawer_container);
    //lv_obj_invalidate(state->background);
    
    // 创建滑动动画
    lv_anim_init(&state->slide_anim);
    lv_anim_set_var(&state->slide_anim, state->drawer_container);
    lv_anim_set_values(&state->slide_anim, -lv_obj_get_width(state->drawer_container), 0);
    lv_anim_set_time(&state->slide_anim, 300);
    lv_anim_set_exec_cb(&state->slide_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_path_cb(&state->slide_anim, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&state->slide_anim, slide_anim_ready_cb);
    lv_anim_set_user_data(&state->slide_anim, state);
    lv_anim_start(&state->slide_anim);
    
    state->is_open = true;
    
    // 临时禁用手势处理，避免事件冲突
    gesture_handler_set_enabled(false);
    
    // 调试：检查扬声器开关状态
    if (state->speaker_switch) {
        bool switch_checked = lv_obj_has_state(state->speaker_switch, LV_STATE_CHECKED);
        bool speaker_enabled = hal_get_speaker_enable();
        printf("Drawer opened - Switch checked: %s, Speaker enabled: %s\n", 
               switch_checked ? "true" : "false", 
               speaker_enabled ? "true" : "false");
    }
    
    printf("App drawer opened successfully\n");
}

// 关闭抽屉
void app_drawer_close(void) {
    overlay_t* overlay = app_manager_get_overlay("AppDrawer");
    if (!overlay || !overlay->base.user_data) {
        return;
    }
    
    drawer_state_t* state = (drawer_state_t*)overlay->base.user_data;
    if (!state->is_open) {
        return;
    }
    
    // 创建滑动动画
    lv_anim_init(&state->slide_anim);
    lv_anim_set_var(&state->slide_anim, state->drawer_container);
    lv_anim_set_values(&state->slide_anim, 0, -lv_obj_get_width(state->drawer_container));
    lv_anim_set_time(&state->slide_anim, 300);
    lv_anim_set_exec_cb(&state->slide_anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_path_cb(&state->slide_anim, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&state->slide_anim, slide_anim_ready_cb);
    lv_anim_set_user_data(&state->slide_anim, state);
    lv_anim_start(&state->slide_anim);
    
    state->is_open = false;
    
    // 重新启用手势处理
    gesture_handler_set_enabled(true);
}

// 切换抽屉状态
void app_drawer_toggle(void) {
    overlay_t* overlay = app_manager_get_overlay("AppDrawer");
    if (!overlay || !overlay->base.user_data) {
        return;
    }
    
    drawer_state_t* state = (drawer_state_t*)overlay->base.user_data;
    if (state->is_open) {
        app_drawer_close();
    } else {
        app_drawer_open();
    }
}

// 注册应用抽屉Overlay
void register_drawer_overlay(void) {
    app_manager_register_overlay("AppDrawer", LV_SYMBOL_LIST, 
                                 drawer_overlay_create, drawer_overlay_destroy,
                                 50, true);  // z_index=50, auto_start=true
} 

// 深度清理抽屉内存
static void deep_clean_drawer(drawer_state_t* state) {
    if (!state || state->is_open || state->deep_cleaned) {
        return;
    }
    
    printf("=== DEEP CLEANING DRAWER ===\n");
    app_manager_log_memory_usage("Before drawer deep clean");
    
    // 安全清理应用列表
    if (state->app_list && lv_obj_is_valid(state->app_list)) {
        // 检查对象是否仍然有效
        if (lv_obj_get_parent(state->app_list) != NULL) {
            // 先停止所有可能的动画
            lv_anim_del(state->app_list, NULL);
            
            // 安全获取子对象数量
            uint32_t child_count = lv_obj_get_child_count(state->app_list);
            
            // 从后往前删除子对象，避免索引问题
            for (int32_t i = (int32_t)child_count - 1; i >= 0; i--) {
                lv_obj_t* child = lv_obj_get_child(state->app_list, i);
                if (child && lv_obj_is_valid(child)) {
                    cleanup_app_item(child);
                    lv_obj_del(child);
                }
            }
            
            printf("Cleaned %lu app items from drawer\n", (unsigned long)child_count);
        } else {
            printf("App list parent is null, skipping cleanup\n");
        }
    } else {
        printf("App list is invalid or null, skipping cleanup\n");
    }
    
    // 重置初始化标志，下次打开时重新创建
    state->is_initialized = false;
    state->deep_cleaned = true;
    
    printf("App drawer deep cleaned\n");
    app_manager_log_memory_usage("After drawer deep clean");
}

// 检查是否需要进行空闲清理
static bool should_idle_cleanup(drawer_state_t* state) {
    if (!state || state->is_open || state->deep_cleaned) {
        return false;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    return (current_time - state->last_open_time) > state->idle_cleanup_threshold;
}

// 强制清理应用列表以释放内存
void app_drawer_cleanup_list(void) {
    overlay_t* overlay = app_manager_get_overlay("AppDrawer");
    if (!overlay || !overlay->base.user_data) {
        return;
    }
    
    drawer_state_t* state = (drawer_state_t*)overlay->base.user_data;
    if (state->is_open) {
        // 抽屉打开时不清理
        printf("Drawer is open, skipping cleanup\n");
        return;
    }
    
    // 检查是否有正在进行的动画
    if (lv_anim_count_running()) {
        printf("Animations running, skipping drawer cleanup\n");
        return;
    }
    
    // 确保抽屉已经关闭足够长时间
    uint32_t current_time = esp_timer_get_time() / 1000;
    if (state->last_open_time > 0 && (current_time - state->last_open_time) < 1000) {
        printf("Drawer recently closed, skipping cleanup\n");
        return;
    }
    
    printf("Force cleaning app drawer list to free memory\n");
    
    // 执行深度清理
    deep_clean_drawer(state);
    
    printf("App drawer list cleaned\n");
}

// 智能内存管理 - 检查是否需要清理
void app_drawer_check_cleanup(void) {
    overlay_t* overlay = app_manager_get_overlay("AppDrawer");
    if (!overlay || !overlay->base.user_data) {
        return;
    }
    
    drawer_state_t* state = (drawer_state_t*)overlay->base.user_data;
    
    // 检查是否需要空闲清理
    if (should_idle_cleanup(state)) {
        printf("Idle cleanup triggered for app drawer\n");
        deep_clean_drawer(state);
    }
} 