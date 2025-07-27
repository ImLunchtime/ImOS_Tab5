#include "apps/photo/app_photo.h"
#include "managers/app_manager.h"
#include "utils/menu_utils.h"
#include "managers/nvs_manager.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include "utils/memory_utils.h"

// 声明自定义字体
LV_FONT_DECLARE(simhei_32);

// 声明图片资源
extern const lv_img_dsc_t image1;
extern const lv_img_dsc_t scenery1;

// 照片项结构
typedef struct {
    const char* name;           // 照片名称
    const lv_img_dsc_t* img;    // 照片图像描述符
    bool is_selected;           // 是否被选中
    bool is_hidden;             // 是否隐藏
} photo_item_t;

// 照片应用状态
typedef struct {
    lv_obj_t* main_container;    // 主容器
    lv_obj_t* title_label;       // 标题标签
    lv_obj_t* split_container;   // 分割容器
    lv_obj_t* photo_list;        // 照片列表容器
    lv_obj_t* preview_container; // 预览容器
    lv_obj_t* preview_img;       // 预览图像
    lv_obj_t* info_label;        // 信息标签
    
    photo_item_t* photos;        // 照片列表
    uint32_t photo_count;        // 照片数量
    int32_t selected_index;      // 当前选中的照片索引
    
    bool is_initialized;         // 初始化标志
} photo_app_state_t;

// 全局状态
static photo_app_state_t* g_photo_state = NULL;

// 照片数据 - 添加/删除图片修改此处
static photo_item_t photo_items[] = {
    {"照片1", &image1, false, false},  // image1可以被隐藏
    {"照片2", &scenery1, false, false},
};

// 前向声明
static void photo_app_create(app_t* app);
static void photo_app_destroy(app_t* app);
static void create_photo_list_ui(void);
static void update_preview(int32_t index);
static void photo_item_event_cb(lv_event_t* e);

// 获取可见照片数量
static uint32_t get_visible_photo_count(void) {
    uint32_t count = 0;
    bool unlocked = nvs_manager_get_unlocked();
    
    for (uint32_t i = 0; i < sizeof(photo_items) / sizeof(photo_items[0]); i++) {
        // image1只有在解锁状态下才可见
        if (photo_items[i].img == &image1 && !unlocked) {
            continue;
        }
        count++;
    }
    return count;
}

// 获取第n个可见照片的索引
static int32_t get_visible_photo_index(uint32_t visible_index) {
    uint32_t count = 0;
    bool unlocked = nvs_manager_get_unlocked();
    
    for (uint32_t i = 0; i < sizeof(photo_items) / sizeof(photo_items[0]); i++) {
        // image1只有在解锁状态下才可见
        if (photo_items[i].img == &image1 && !unlocked) {
            continue;
        }
        if (count == visible_index) {
            return i;
        }
        count++;
    }
    return -1;
}

// 创建照片列表UI
static void create_photo_list_ui(void) {
    if (!g_photo_state || !g_photo_state->photo_list) {
        printf("Invalid state for create_photo_list_ui\n");
        return;
    }
    
    // 清空现有列表
    lv_obj_clean(g_photo_state->photo_list);
    
    // 设置列表布局
    lv_obj_set_layout(g_photo_state->photo_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_photo_state->photo_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_photo_state->photo_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(g_photo_state->photo_list, 8, 0);
    
    bool unlocked = nvs_manager_get_unlocked();
    uint32_t visible_index = 0;
    
    // 创建照片项
    for (uint32_t i = 0; i < g_photo_state->photo_count; i++) {
        photo_item_t* photo = &g_photo_state->photos[i];
        
        // 检查是否应该隐藏image1
        if (photo->img == &image1 && !unlocked) {
            continue;
        }
        
        // 创建照片项容器
        lv_obj_t* item_container = lv_obj_create(g_photo_state->photo_list);
        lv_obj_set_size(item_container, LV_PCT(100), 60);
        
        // 设置样式
        lv_obj_set_style_bg_opa(item_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(item_container, 0, 0);
        lv_obj_set_style_pad_all(item_container, 8, 0);
        
        // 选中效果
        lv_obj_set_style_bg_color(item_container, lv_color_hex(0xE3F2FD), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(item_container, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_radius(item_container, 8, LV_STATE_PRESSED);
        
        // 当前选中项的高亮效果
        if (i == g_photo_state->selected_index) {
            lv_obj_set_style_bg_color(item_container, lv_color_hex(0xBBDEFB), 0);
            lv_obj_set_style_bg_opa(item_container, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(item_container, 8, 0);
        }
        
        // 让容器可以接收点击事件
        lv_obj_add_flag(item_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(item_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(item_container, LV_OBJ_FLAG_EVENT_BUBBLE);
        
        // 创建图标
        lv_obj_t* icon = lv_label_create(item_container);
        lv_label_set_text(icon, LV_SYMBOL_IMAGE);
        lv_obj_set_style_text_color(icon, lv_color_hex(0x2196F3), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
        
        // 创建照片名称标签
        lv_obj_t* name_label = lv_label_create(item_container);
        lv_label_set_text(name_label, photo->name);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0x333333), 0);
        lv_obj_set_style_text_font(name_label, &simhei_32, 0);
        lv_obj_align_to(name_label, icon, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
        
        // 添加点击事件，使用原始索引
        lv_obj_add_event_cb(item_container, photo_item_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        visible_index++;
    }
}

// 更新预览
static void update_preview(int32_t index) {
    if (!g_photo_state || index < 0 || index >= g_photo_state->photo_count) {
        return;
    }
    
    // 检查选中的照片是否可见
    bool unlocked = nvs_manager_get_unlocked();
    if (g_photo_state->photos[index].img == &image1 && !unlocked) {
        // 如果选中的是隐藏的image1，选择第一个可见的照片
        int32_t visible_index = get_visible_photo_index(0);
        if (visible_index >= 0) {
            index = visible_index;
        } else {
            return; // 没有可见的照片
        }
    }
    
    // 更新选中状态
    g_photo_state->selected_index = index;
    
    // 更新预览图像
    if (g_photo_state->preview_img) {
        lv_img_set_src(g_photo_state->preview_img, g_photo_state->photos[index].img);
        
        // 居中显示图像
        lv_obj_center(g_photo_state->preview_img);
    }
    
    // 更新信息标签
    if (g_photo_state->info_label) {
        lv_label_set_text(g_photo_state->info_label, g_photo_state->photos[index].name);
    }
    
    // 更新列表UI以反映选中状态
    create_photo_list_ui();
}

// 照片项点击事件
static void photo_item_event_cb(lv_event_t* e) {
    int32_t index = (int32_t)(intptr_t)lv_event_get_user_data(e);
    update_preview(index);
}

// 照片应用创建
static void photo_app_create(app_t* app) {
    if (!app || !app->container) {
        return;
    }
    
    printf("Creating photo app\n");
    app_manager_log_memory_usage("Before photo app creation");
    
    // 分配状态结构
    g_photo_state = (photo_app_state_t*)safe_malloc(sizeof(photo_app_state_t));
    if (!g_photo_state) {
        printf("Failed to allocate photo app state\n");
        return;
    }
    
    memset(g_photo_state, 0, sizeof(photo_app_state_t));
    
    // 设置照片数据
    g_photo_state->photos = photo_items;
    g_photo_state->photo_count = sizeof(photo_items) / sizeof(photo_items[0]);
    g_photo_state->selected_index = -1; // 初始无选中项
    
    // 创建主容器
    g_photo_state->main_container = lv_obj_create(app->container);
    lv_obj_set_size(g_photo_state->main_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(g_photo_state->main_container, 0, 0);
    lv_obj_set_style_pad_all(g_photo_state->main_container, 16, 0);
    lv_obj_clear_flag(g_photo_state->main_container, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动
    
    // 创建标题
    g_photo_state->title_label = lv_label_create(g_photo_state->main_container);
    lv_label_set_text(g_photo_state->title_label, "照片");
    lv_obj_set_style_text_color(g_photo_state->title_label, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_text_font(g_photo_state->title_label, &simhei_32, 0);
    lv_obj_align(g_photo_state->title_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 创建分割容器
    g_photo_state->split_container = lv_obj_create(g_photo_state->main_container);
    lv_obj_set_size(g_photo_state->split_container, LV_PCT(100), LV_PCT(85));
    lv_obj_align_to(g_photo_state->split_container, g_photo_state->title_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 16);
    lv_obj_set_style_bg_opa(g_photo_state->split_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_photo_state->split_container, 0, 0);
    lv_obj_set_style_pad_all(g_photo_state->split_container, 0, 0);
    lv_obj_set_flex_flow(g_photo_state->split_container, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(g_photo_state->split_container, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动
    
    // 创建照片列表容器 (左侧 30%)
    g_photo_state->photo_list = lv_obj_create(g_photo_state->split_container);
    lv_obj_set_size(g_photo_state->photo_list, LV_PCT(30), LV_PCT(100));
    lv_obj_set_style_bg_opa(g_photo_state->photo_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_photo_state->photo_list, 1, 0);
    lv_obj_set_style_border_color(g_photo_state->photo_list, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_pad_all(g_photo_state->photo_list, 8, 0);
    
    // 创建预览容器 (右侧 70%)
    g_photo_state->preview_container = lv_obj_create(g_photo_state->split_container);
    lv_obj_set_size(g_photo_state->preview_container, LV_PCT(70), LV_PCT(100));
    lv_obj_set_style_bg_opa(g_photo_state->preview_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_photo_state->preview_container, 1, 0);
    lv_obj_set_style_border_color(g_photo_state->preview_container, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_pad_all(g_photo_state->preview_container, 8, 0);
    lv_obj_clear_flag(g_photo_state->preview_container, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动
    
    // 创建预览图像
    g_photo_state->preview_img = lv_img_create(g_photo_state->preview_container);
    lv_obj_center(g_photo_state->preview_img);
    
    // 创建信息标签
    g_photo_state->info_label = lv_label_create(g_photo_state->preview_container);
    lv_obj_set_style_text_color(g_photo_state->info_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(g_photo_state->info_label, &simhei_32, 0);
    lv_obj_align(g_photo_state->info_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    // 创建照片列表UI
    create_photo_list_ui();
    
    // 默认选中第一张可见照片
    uint32_t visible_count = get_visible_photo_count();
    if (visible_count > 0) {
        int32_t first_visible = get_visible_photo_index(0);
        if (first_visible >= 0) {
            update_preview(first_visible);
        }
    }
    
    g_photo_state->is_initialized = true;
    app->user_data = g_photo_state;
    
    printf("Photo app created successfully\n");
    app_manager_log_memory_usage("After photo app creation");
}

// 照片应用销毁
static void photo_app_destroy(app_t* app) {
    if (!app) {
        return;
    }
    
    printf("Destroying photo app\n");
    app_manager_log_memory_usage("Before photo app destruction");
    
    if (g_photo_state) {
        // 重置状态
        g_photo_state->is_initialized = false;
        
        // 等待LVGL任务完成
        lv_refr_now(NULL);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // 释放状态结构
        safe_free(g_photo_state);
        g_photo_state = NULL;
    }
    
    if (app) {
        app->user_data = NULL;
    }
    
    printf("Photo app destroyed\n");
    app_manager_log_memory_usage("After photo app destruction");
}

// 注册照片应用
void register_photo_app(void) {
    app_manager_register_app("照片", LV_SYMBOL_IMAGE, 
                             photo_app_create, photo_app_destroy);
}