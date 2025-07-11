#pragma once

#include "lvgl.h"
#include <stdbool.h>

// 前向声明
typedef struct app_manager_t app_manager_t;
typedef struct app_t app_t;
typedef struct overlay_t overlay_t;

// 应用类型
typedef enum {
    APP_TYPE_NORMAL,    // 普通应用：全屏显示，离开即销毁
    APP_TYPE_OVERLAY    // 覆盖层：可后台驻留，显示在App之上
} app_type_t;

// 应用状态
typedef enum {
    APP_STATE_INACTIVE,  // 未激活
    APP_STATE_ACTIVE,    // 激活中
    APP_STATE_BACKGROUND // 后台运行（仅Overlay支持）
} app_state_t;

// 应用回调函数类型
typedef void (*app_create_cb_t)(app_t* app);
typedef void (*app_destroy_cb_t)(app_t* app);
typedef void (*app_resume_cb_t)(app_t* app);
typedef void (*app_pause_cb_t)(app_t* app);

// 应用结构体
struct app_t {
    char name[32];              // 应用名称
    char icon[8];               // 应用图标（LVGL符号）
    app_type_t type;            // 应用类型
    app_state_t state;          // 应用状态
    lv_obj_t* container;        // 应用容器
    
    // 回调函数
    app_create_cb_t create_cb;
    app_destroy_cb_t destroy_cb;
    app_resume_cb_t resume_cb;
    app_pause_cb_t pause_cb;
    
    // 用户数据
    void* user_data;
    
    // 链表指针
    app_t* next;
};

// Overlay特定结构体
struct overlay_t {
    app_t base;                 // 继承自app_t
    int z_index;                // 显示层级
    bool auto_start;            // 系统启动时自动启动
    overlay_t* next;            // 链表指针
};

// 应用管理器结构体
struct app_manager_t {
    app_t* apps;                // 应用链表
    overlay_t* overlays;        // Overlay链表
    app_t* current_app;         // 当前激活的应用
    lv_obj_t* app_container;    // 应用容器
    lv_obj_t* overlay_container; // Overlay容器
    bool initialized;           // 初始化标志
};

// 应用管理器API
app_manager_t* app_manager_get_instance(void);
void app_manager_init(void);
void app_manager_deinit(void);

// 应用注册和管理
app_t* app_manager_register_app(const char* name, const char* icon, 
                                app_create_cb_t create_cb, app_destroy_cb_t destroy_cb);
overlay_t* app_manager_register_overlay(const char* name, const char* icon, 
                                        app_create_cb_t create_cb, app_destroy_cb_t destroy_cb,
                                        int z_index, bool auto_start);

// 应用控制
bool app_manager_launch_app(const char* name);
bool app_manager_close_current_app(void);
bool app_manager_show_overlay(const char* name);
bool app_manager_hide_overlay(const char* name);

// 应用查询
app_t* app_manager_get_app(const char* name);
overlay_t* app_manager_get_overlay(const char* name);
app_t* app_manager_get_current_app(void);
app_t* app_manager_get_app_list(void);
overlay_t* app_manager_get_overlay_list(void);

// 启动器相关
void app_manager_go_to_launcher(void);
bool app_manager_is_launcher_active(void);

// 内存管理相关
void app_manager_force_gc(void);
void app_manager_log_memory_usage(const char* context);
bool app_manager_check_memory_sufficient(void);
void app_manager_get_memory_stats(uint32_t* gc_count, size_t* free_heap, size_t* free_psram); 