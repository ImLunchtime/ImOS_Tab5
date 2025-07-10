#include "gesture_handler.h"
#include "app_manager.h"
#include "overlay_drawer.h"
#include <string.h>
#include <stdio.h>

// 手势状态
typedef struct {
    bool is_enabled;
    lv_obj_t* gesture_area;
    lv_point_t start_point;
    bool is_dragging;
    lv_coord_t edge_threshold;
    lv_coord_t drag_threshold;
} gesture_state_t;

static gesture_state_t g_gesture_state = {0};

// 手势事件处理
static void gesture_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (!g_gesture_state.is_enabled) {
        return;
    }
    
    // 获取触摸点
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) {
        return;
    }
    
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    
    // 现在手势区域只在左边缘，所以所有事件都是有效的
    
    switch (code) {
        case LV_EVENT_PRESSED: {
            printf("Gesture PRESSED at (%ld, %ld)\n", 
                   (long)point.x, (long)point.y);
            
            g_gesture_state.start_point = point;
            g_gesture_state.is_dragging = true;
            printf("*** GESTURE STARTED at left edge ***\n");
            break;
        }
        
        case LV_EVENT_PRESSING: {
            if (g_gesture_state.is_dragging) {
                // 计算拖拽距离
                lv_coord_t drag_distance = point.x - g_gesture_state.start_point.x;
                
                printf("Gesture PRESSING: current=(%ld, %ld), start=(%ld, %ld), distance=%ld\n", 
                       (long)point.x, (long)point.y, (long)g_gesture_state.start_point.x, (long)g_gesture_state.start_point.y, (long)drag_distance);
                
                // 如果向右拖拽距离超过阈值，打开抽屉
                if (drag_distance > g_gesture_state.drag_threshold) {
                    printf("*** GESTURE THRESHOLD REACHED - OPENING DRAWER ***\n");
                    app_drawer_open();
                    g_gesture_state.is_dragging = false;
                }
            }
            break;
        }
        
        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST: {
            g_gesture_state.is_dragging = false;
            break;
        }
        
        default:
            break;
    }
}

// 初始化手势处理
void gesture_handler_init(void) {
    if (g_gesture_state.is_enabled) {
        return;
    }
    
    // 获取屏幕尺寸
    lv_coord_t screen_height = lv_display_get_vertical_resolution(NULL);
    
    // 设置手势参数
    g_gesture_state.edge_threshold = 20;  // 左侧边缘20像素范围
    g_gesture_state.drag_threshold = 50;  // 拖拽50像素触发
    
    // 创建手势检测区域（只覆盖左边缘区域）
    g_gesture_state.gesture_area = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_gesture_state.gesture_area, g_gesture_state.edge_threshold, screen_height);  // 只设置左边缘宽度
    lv_obj_set_pos(g_gesture_state.gesture_area, 0, 0);  // 位置在屏幕最左侧
    lv_obj_set_style_bg_opa(g_gesture_state.gesture_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_gesture_state.gesture_area, 0, 0);
    lv_obj_clear_flag(g_gesture_state.gesture_area, LV_OBJ_FLAG_SCROLLABLE);
    
    // 确保手势区域在最顶层，但现在它只覆盖左边缘
    lv_obj_move_foreground(g_gesture_state.gesture_area);
    
    // 只监听必要的事件，减少事件拦截
    lv_obj_add_event_cb(g_gesture_state.gesture_area, gesture_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_gesture_state.gesture_area, gesture_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(g_gesture_state.gesture_area, gesture_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_gesture_state.gesture_area, gesture_event_cb, LV_EVENT_PRESS_LOST, NULL);
    
    printf("Gesture handler initialized with edge area width=%ld, drag_threshold=%ld\n", 
           (long)g_gesture_state.edge_threshold, (long)g_gesture_state.drag_threshold);
    
    g_gesture_state.is_enabled = true;
}

// 销毁手势处理
void gesture_handler_deinit(void) {
    if (!g_gesture_state.is_enabled) {
        return;
    }
    
    if (g_gesture_state.gesture_area) {
        lv_obj_del(g_gesture_state.gesture_area);
        g_gesture_state.gesture_area = NULL;
    }
    
    memset(&g_gesture_state, 0, sizeof(gesture_state_t));
}

// 启用/禁用手势处理
void gesture_handler_set_enabled(bool enabled) {
    g_gesture_state.is_enabled = enabled;
    
    if (g_gesture_state.gesture_area) {
        if (enabled) {
            lv_obj_clear_flag(g_gesture_state.gesture_area, LV_OBJ_FLAG_HIDDEN);
            // 确保手势区域在最顶层
            lv_obj_move_foreground(g_gesture_state.gesture_area);
            printf("Gesture handler enabled and moved to foreground\n");
        } else {
            lv_obj_add_flag(g_gesture_state.gesture_area, LV_OBJ_FLAG_HIDDEN);
            printf("Gesture handler disabled\n");
        }
    }
}

// 获取手势处理状态
bool gesture_handler_is_enabled(void) {
    return g_gesture_state.is_enabled;
} 