#include "app_test.h"
#include <stdio.h>
#include <time.h>
#include <string.h>  // 添加string.h头文件以使用memset

// 用于存储Arc值的全局变量
static int g_arc_value = 0;

// 更新时间标签的回调函数
static void update_time_label(lv_timer_t* timer) {
    lv_obj_t* time_label = (lv_obj_t*)lv_timer_get_user_data(timer); // 修复：使用lv_timer_get_user_data函数
    time_t now;
    struct tm timeinfo;
    char time_str[10];
    
    time(&now);
    localtime_r(&now, &timeinfo);
    snprintf(time_str, sizeof(time_str), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    
    lv_label_set_text(time_label, time_str);
}

// Arc值变化事件回调
static void arc_value_changed_event(lv_event_t* e) {
    lv_obj_t* arc = lv_event_get_target(e);
    lv_obj_t* value_label = lv_event_get_user_data(e);
    
    g_arc_value = lv_arc_get_value(arc);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", g_arc_value);
    lv_label_set_text(value_label, buf);
}

// 创建应用界面的回调函数
// 在app_test.h中添加定义
typedef struct {
    lv_timer_t* time_update_timer;
} test_app_data_t;

// 在test_app_create函数中
static void test_app_create(app_t* app) {
    if (!app || !app->container) {
        return;
    }
    
    printf("Creating test app\n");
    
    // 设置深色背景
    lv_obj_set_style_bg_color(app->container, lv_color_hex(0x303030), 0);
    lv_obj_set_style_bg_opa(app->container, LV_OPA_COVER, 0);
    
    // 创建顶栏
    lv_obj_t* top_bar = lv_obj_create(app->container);
    lv_obj_set_size(top_bar, 1280, 60);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
    // 移除顶栏边框和圆角
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    
    // 在顶栏左侧添加时间显示
    lv_obj_t* time_label = lv_label_create(top_bar);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_24, 0);
    lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 20, 0);
    lv_label_set_text(time_label, "00:00");
    
    // 删除这行，避免创建未被管理的定时器
    // lv_timer_t* timer = lv_timer_create(update_time_label, 1000, time_label);
    
    // 在顶栏右侧添加更多按钮
    lv_obj_t* more_btn = lv_btn_create(top_bar);
    lv_obj_set_size(more_btn, 50, 40);
    lv_obj_align(more_btn, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_color(more_btn, lv_color_hex(0x303030), 0);
    
    lv_obj_t* more_label = lv_label_create(more_btn);
    lv_label_set_text(more_label, LV_SYMBOL_LIST);
    lv_obj_center(more_label);
    
    // 创建灰色圆形背景
    lv_obj_t* circle_bg = lv_obj_create(app->container);
    lv_obj_set_size(circle_bg, 400, 400);
    lv_obj_set_style_radius(circle_bg, 200, 0);
    lv_obj_set_style_bg_color(circle_bg, lv_color_hex(0x505050), 0);
    lv_obj_set_style_bg_opa(circle_bg, LV_OPA_COVER, 0);
    // 移除灰色圆的边框
    lv_obj_set_style_border_width(circle_bg, 0, 0);
    // 居中显示
    lv_obj_align(circle_bg, LV_ALIGN_CENTER, 0, 0);
    
    // 创建蓝色旋钮
    lv_obj_t* arc = lv_arc_create(app->container);
    lv_obj_set_size(arc, 360, 360);
    lv_arc_set_rotation(arc, 0);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_value(arc, 0);
    lv_arc_set_range(arc, 0, 99);
    
    // 设置背景弧形样式
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 20, LV_PART_MAIN);
    
    // 设置前景弧形样式，使用渐变效果
    lv_obj_set_style_arc_color(arc, lv_color_hex(0x2196F3), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 20, LV_PART_INDICATOR);
    // // 使用背景渐变来模拟3D效果
    // lv_obj_set_style_bg_grad_color(arc, lv_color_hex(0x1976D2), LV_PART_INDICATOR);
    // lv_obj_set_style_bg_grad_dir(arc, LV_GRAD_DIR_VER, LV_PART_INDICATOR);
    
    // 居中显示
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, 0);
    
    // 添加新的灰色圆，位于黑色圆外面，Arc控件里面
    lv_obj_t* middle_circle = lv_obj_create(app->container);
    // 设置大小为260x260，刚好能触碰到Arc控件的内边（Arc宽度为360，弧宽为20，所以内径为320）
    lv_obj_set_size(middle_circle, 310, 310);
    lv_obj_set_style_radius(middle_circle, 150, 0); // 半径为宽度的一半
    
    // 设置渐变背景色，上亮下暗
    lv_obj_set_style_bg_color(middle_circle, lv_color_hex(0x707070), 0); // 亮灰色
    lv_obj_set_style_bg_grad_color(middle_circle, lv_color_hex(0x404040), 0); // 暗灰色
    lv_obj_set_style_bg_grad_dir(middle_circle, LV_GRAD_DIR_VER, 0); // 垂直渐变
    lv_obj_set_style_bg_opa(middle_circle, LV_OPA_COVER, 0);
    
    // 添加灰色边框
    lv_obj_set_style_border_color(middle_circle, lv_color_hex(0x606060), 0);
    lv_obj_set_style_border_width(middle_circle, 2, 0);
    
    // 添加模糊的黑色阴影
    lv_obj_set_style_shadow_color(middle_circle, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_width(middle_circle, 30, 0); // 阴影宽度
    lv_obj_set_style_shadow_opa(middle_circle, LV_OPA_60, 0); // 阴影透明度60%，不要太深
    lv_obj_set_style_shadow_spread(middle_circle, 0, 0); // 阴影扩散
    lv_obj_set_style_shadow_ofs_x(middle_circle, 0, 0); // 阴影X偏移
    lv_obj_set_style_shadow_ofs_y(middle_circle, 0, 0); // 阴影Y偏移
    
    // 居中显示
    lv_obj_align(middle_circle, LV_ALIGN_CENTER, 0, 0);
    
    // 在旋钮中间添加黑色圆，带有深灰色边框
    lv_obj_t* center_circle = lv_obj_create(app->container);
    lv_obj_set_size(center_circle, 180, 180);
    lv_obj_set_style_radius(center_circle, 90, 0);
    lv_obj_set_style_bg_color(center_circle, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(center_circle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(center_circle, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(center_circle, 3, 0);
    lv_obj_align(center_circle, LV_ALIGN_CENTER, 0, 0);
    
    // 在中间圆内添加白色数字标签
    lv_obj_t* value_label = lv_label_create(center_circle);
    lv_obj_set_style_text_color(value_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_44, 0); // 修复：使用lv_font_montserrat_44代替不存在的lv_font_montserrat_48
    lv_label_set_text(value_label, "0");
    lv_obj_center(value_label);
    
    // 添加事件处理，当旋钮值变化时更新数字
    lv_obj_add_event_cb(arc, arc_value_changed_event, LV_EVENT_VALUE_CHANGED, value_label);
    
    // 分配应用数据
    test_app_data_t* app_data = malloc(sizeof(test_app_data_t));
    if (!app_data) {
        return;
    }
    memset(app_data, 0, sizeof(test_app_data_t));
    app->user_data = app_data;
    
    // 创建定时器更新时间并保存引用
    app_data->time_update_timer = lv_timer_create(update_time_label, 1000, time_label);
    
    // 删除从这里开始的所有重复代码
}

// 销毁应用的回调函数
static void test_app_destroy(app_t* app) {
    if (!app || !app->user_data) {
        return;
    }
    printf("Destroying test app\n");
    
    test_app_data_t* app_data = (test_app_data_t*)app->user_data;
    
    // 删除定时器
    if (app_data->time_update_timer) {
        lv_timer_del(app_data->time_update_timer);
        app_data->time_update_timer = NULL;
    }
    
    // 释放应用数据
    free(app_data);
    app->user_data = NULL;
}

// 注册应用
void register_test_app(void) {
    app_manager_register_app("测试", LV_SYMBOL_SETTINGS, 
                           test_app_create, test_app_destroy);
}