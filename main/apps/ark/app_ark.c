#include "apps/ark/app_ark.h"

#include "managers/app_manager.h"
#include "managers/content_lock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "utils/memory_utils.h"
#include "assets/picture5.c"

// 声明自定义字体
LV_FONT_DECLARE(simhei_32);
// 声明图片
LV_IMAGE_DECLARE(picture5);

// Ark应用状态结构
typedef struct {
    lv_obj_t* tabview;
    lv_obj_t* chart;
    lv_timer_t* chart_timer;
    bool is_initialized;
} ark_state_t;

// 全局状态变量
static ark_state_t* g_ark_state = NULL;

// 图表数据更新定时器回调函数
static void chart_add_data(lv_timer_t * t)
{
    // 检查状态是否有效
    if (!g_ark_state || !g_ark_state->is_initialized) {
        printf("Ark state invalid, stopping chart timer\n");
        if (t) {
            lv_timer_del(t);
        }
        return;
    }
    
    lv_obj_t * chart = lv_timer_get_user_data(t);
    
    // 验证图表对象是否仍然有效
    if (!chart || !lv_obj_is_valid(chart)) {
        printf("Chart object invalid, stopping timer\n");
        if (g_ark_state) {
            g_ark_state->chart_timer = NULL;
        }
        lv_timer_del(t);
        return;
    }
    
    lv_chart_series_t * ser = lv_chart_get_series_next(chart, NULL);
    if (!ser) {
        printf("Chart series invalid, stopping timer\n");
        if (g_ark_state) {
            g_ark_state->chart_timer = NULL;
        }
        lv_timer_del(t);
        return;
    }

    lv_chart_set_next_value(chart, ser, lv_rand(30, 60));

    uint16_t p = lv_chart_get_point_count(chart);
    uint16_t s = lv_chart_get_x_start_point(chart, ser);
    int32_t * a = lv_chart_get_y_array(chart, ser);

    if (a && p > 0) {
        a[(s + 1) % p] = LV_CHART_POINT_NONE;
        a[(s + 2) % p] = LV_CHART_POINT_NONE;
        a[(s + 3) % p] = LV_CHART_POINT_NONE;
    }

    lv_chart_refresh(chart);
}

void create_chart(lv_obj_t* parent)
{
    if (!g_ark_state) {
        printf("Error: Ark state not initialized\n");
        return;
    }
    
    /*Create a stacked_area_chart.obj*/
    lv_obj_t * chart = lv_chart_create(parent);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_CIRCULAR);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_size(chart, 700, 500);
    lv_obj_center(chart);

    lv_chart_set_point_count(chart, 100);
    lv_chart_series_t * ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    
    /*Prefill with data*/
    uint32_t i;
    for(i = 0; i < 100; i++) {
        lv_chart_set_next_value(chart, ser, lv_rand(30, 90));
    }

    // 保存图表引用并创建定时器
    g_ark_state->chart = chart;
    g_ark_state->chart_timer = lv_timer_create(chart_add_data, 100, chart);
    
    if (g_ark_state->chart_timer) {
        printf("Chart timer created successfully\n");
    } else {
        printf("Failed to create chart timer\n");
    }
}

// 修改函数签名，接受父容器参数
void create_ark_control_gui(lv_obj_t* parent)
{
    if (!parent) {
        printf("Error: No parent container provided for Ark GUI\n");
        return;
    }
    
    if (!g_ark_state) {
        printf("Error: Ark state not initialized\n");
        return;
    }
    
    /*Create a Tab view object in the provided parent container*/
    lv_obj_t * tabview;
    tabview = lv_tabview_create(parent);  // 使用传入的父容器而不是屏幕
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_LEFT);
    lv_tabview_set_tab_bar_size(tabview, 160);

    // 确保tabview填满整个父容器
    lv_obj_set_size(tabview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(tabview, 0, 0);

    // 保存tabview引用
    g_ark_state->tabview = tabview;

    lv_obj_t * tab_buttons = lv_tabview_get_tab_bar(tabview);
    lv_obj_set_style_bg_color(tab_buttons, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_text_color(tab_buttons, lv_palette_lighten(LV_PALETTE_GREY, 5), 0);
    lv_obj_set_style_border_side(tab_buttons, LV_BORDER_SIDE_RIGHT, LV_PART_ITEMS | LV_STATE_CHECKED);
    // 为标签按钮设置自定义字体
    lv_obj_set_style_text_font(tab_buttons, &lv_font_montserrat_20, 0);

    /*Add 5 tabs (the tabs are page (lv_page) and can be scrolled*/
    lv_obj_t * tab1 = lv_tabview_add_tab(tabview, "Tab 1");
    lv_obj_t * tab2 = lv_tabview_add_tab(tabview, "Tab 2");
    lv_obj_t * tab3 = lv_tabview_add_tab(tabview, "Tab 3");
    lv_obj_t * tab4 = lv_tabview_add_tab(tabview, "Tab 4");
    lv_obj_t * tab5 = lv_tabview_add_tab(tabview, "Tab 5");

    /*Add content to the tabs*/
    // -=-=-=-=-=-=-=-=-=[Tab1]=-=-=-=-=-=-=-=-=-=-
    // 创建图表容器并放置在网格的上部
    create_chart(tab1);

    // -=-=-=-=-=-=-=-=-=[Tab2]=-=-=-=-=-=-=-=-=-=-
    // 创建一个容器作为页面布局
    lv_obj_t * layout = lv_obj_create(tab2);
    lv_obj_set_size(layout, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(layout, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(layout, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_border_opa(layout, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(layout, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_pad_all(layout, 0, LV_PART_MAIN);
    lv_obj_set_style_margin_all(layout, 0, LV_PART_MAIN);
    // 创建列表
    lv_obj_t * list = lv_list_create(layout);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN);
    lv_obj_set_style_margin_all(list, 0, LV_PART_MAIN);
    
    // 为列表设置自定义字体
    lv_obj_set_style_text_font(list, &simhei_32, LV_PART_MAIN);
    
    // 添加一些示例列表项
    lv_obj_t *btn1 = lv_list_add_btn(list, LV_SYMBOL_VOLUME_MAX, "SPK1");
    lv_obj_t *btn2 = lv_list_add_btn(list, LV_SYMBOL_VOLUME_MAX, "SPK2");
    lv_obj_t *btn3 = lv_list_add_btn(list, LV_SYMBOL_VOLUME_MAX, "SPK3");
    lv_obj_t *btn4 = lv_list_add_btn(list, LV_SYMBOL_VOLUME_MAX, "SPK4");
    lv_obj_t *btn5 = lv_list_add_btn(list, LV_SYMBOL_VOLUME_MAX, "ITC1");
    lv_obj_t *btn6 = lv_list_add_btn(list, LV_SYMBOL_VOLUME_MAX, "ITC2");

    // 为每个按钮的文本标签设置自定义字体
    lv_obj_t *label;
    label = lv_obj_get_child(btn1, -1);
    lv_obj_set_style_text_font(label, &simhei_32, 0);
    label = lv_obj_get_child(btn2, -1);
    lv_obj_set_style_text_font(label, &simhei_32, 0);
    label = lv_obj_get_child(btn3, -1);
    lv_obj_set_style_text_font(label, &simhei_32, 0);
    label = lv_obj_get_child(btn4, -1);
    lv_obj_set_style_text_font(label, &simhei_32, 0);
    label = lv_obj_get_child(btn5, -1);
    lv_obj_set_style_text_font(label, &simhei_32, 0);
    label = lv_obj_get_child(btn6, -1);
    lv_obj_set_style_text_font(label, &simhei_32, 0);
    // 创建FAB按钮
    lv_obj_t * fab = lv_btn_create(tab2);
    lv_obj_set_size(fab, 144, 144);
    lv_obj_add_flag(fab, LV_OBJ_FLAG_FLOATING);
    lv_obj_align(fab, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_radius(fab, 72, LV_PART_MAIN);
    
    // 为FAB添加图标
    lv_obj_t * fab_label = lv_label_create(fab);
    lv_label_set_text(fab_label, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_font(fab_label, &lv_font_montserrat_44, 0); // 设置图标字体大小为64px
    lv_obj_center(fab_label);

    // -=-=-=-=-=-=-=-=-=[Tab3]=-=-=-=-=-=-=-=-=-=-
    lv_obj_t * bgimg = lv_image_create(tab3);
    lv_image_set_src(bgimg, &picture5);
    // -=-=-=-=-=-=-=-=-=[Tab4]=-=-=-=-=-=-=-=-=-=-
    lv_obj_t * label3 = lv_label_create(tab4);
    lv_label_set_text(label3, "Forth tab");
    // -=-=-=-=-=-=-=-=-=[Tab5]=-=-=-=-=-=-=-=-=-=-
    lv_obj_t * label4 = lv_label_create(tab5);
    lv_label_set_text(label4, "Fifth tab");

    lv_obj_remove_flag(lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE);
    
    printf("Ark tabview created successfully in app container\n");
}

// Ark应用创建函数
static void ark_app_create(app_t* app) {
    if (!app || !app->container) {
        printf("Error: Invalid app or container for Ark app\n");
        return;
    }
    
    printf("Creating Ark app in container: %p\n", (void*)app->container);
    app_manager_log_memory_usage("Before Ark app creation");
    
    // 分配状态结构
    g_ark_state = (ark_state_t*)safe_malloc(sizeof(ark_state_t));
    if (!g_ark_state) {
        printf("Failed to allocate memory for Ark state\n");
        return;
    }
    
    // 初始化状态
    memset(g_ark_state, 0, sizeof(ark_state_t));
    g_ark_state->is_initialized = true;
    
    // 保存状态到用户数据
    app->user_data = g_ark_state;
    
    // 将应用容器作为父容器传递给GUI创建函数
    create_ark_control_gui(app->container);
    
    printf("Ark app created successfully\n");
    app_manager_log_memory_usage("After Ark app creation");
}

// Ark应用销毁函数
static void ark_app_destroy(app_t* app) {
    printf("Destroying Ark app\n");
    app_manager_log_memory_usage("Before Ark app destruction");
    
    if (g_ark_state) {
        // 标记为未初始化，防止定时器回调继续执行
        g_ark_state->is_initialized = false;
        
        // 删除定时器
        if (g_ark_state->chart_timer) {
            printf("Deleting chart timer\n");
            lv_timer_del(g_ark_state->chart_timer);
            g_ark_state->chart_timer = NULL;
        }
        
        // 清空对象引用
        g_ark_state->tabview = NULL;
        g_ark_state->chart = NULL;
        
        // 等待LVGL任务完成当前的渲染周期
        lv_refr_now(NULL);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // 释放状态结构
        safe_free(g_ark_state);
        g_ark_state = NULL;
    }
    
    if (app) {
        app->user_data = NULL;
    }
    
    printf("Ark app destroyed successfully\n");
    app_manager_log_memory_usage("After Ark app destruction");
}

// 注册Ark应用
void register_ark_app(void) {
    app_manager_register_app("Ark", LV_SYMBOL_BELL, 
                             ark_app_create, ark_app_destroy);
}