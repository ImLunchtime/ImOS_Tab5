#include "app_manager.h"
#include <string.h>
#include <stdlib.h>

// 全局应用管理器实例
static app_manager_t g_app_manager = {0};

// 获取应用管理器实例
app_manager_t* app_manager_get_instance(void) {
    return &g_app_manager;
}

// 初始化应用管理器
void app_manager_init(void) {
    if (g_app_manager.initialized) {
        return;
    }
    
    // 初始化应用管理器
    memset(&g_app_manager, 0, sizeof(app_manager_t));
    
    // 创建应用容器（全屏）
    g_app_manager.app_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_app_manager.app_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(g_app_manager.app_container, 0, 0);
    lv_obj_clear_flag(g_app_manager.app_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_app_manager.app_container, 0, 0);
    lv_obj_set_style_border_width(g_app_manager.app_container, 0, 0);
    
    // 创建Overlay容器（全屏，透明背景）
    g_app_manager.overlay_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_app_manager.overlay_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(g_app_manager.overlay_container, 0, 0);
    lv_obj_clear_flag(g_app_manager.overlay_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_app_manager.overlay_container, 0, 0);
    lv_obj_set_style_border_width(g_app_manager.overlay_container, 0, 0);
    lv_obj_set_style_bg_opa(g_app_manager.overlay_container, LV_OPA_TRANSP, 0);
    
    // 确保overlay容器不会拦截事件，让事件传递到底层应用
    lv_obj_add_flag(g_app_manager.overlay_container, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(g_app_manager.overlay_container, LV_OBJ_FLAG_CLICKABLE);
    
    // 确保Overlay容器在App容器之上
    lv_obj_move_foreground(g_app_manager.overlay_container);
    
    g_app_manager.initialized = true;
}

// 销毁应用管理器
void app_manager_deinit(void) {
    if (!g_app_manager.initialized) {
        return;
    }
    
    // 销毁所有应用
    app_t* app = g_app_manager.apps;
    while (app) {
        app_t* next = app->next;
        if (app->destroy_cb) {
            app->destroy_cb(app);
        }
        free(app);
        app = next;
    }
    
    // 销毁所有Overlay
    overlay_t* overlay = g_app_manager.overlays;
    while (overlay) {
        overlay_t* next = overlay->next;
        if (overlay->base.destroy_cb) {
            overlay->base.destroy_cb(&overlay->base);
        }
        free(overlay);
        overlay = next;
    }
    
    // 销毁容器
    if (g_app_manager.app_container) {
        lv_obj_del(g_app_manager.app_container);
    }
    if (g_app_manager.overlay_container) {
        lv_obj_del(g_app_manager.overlay_container);
    }
    
    memset(&g_app_manager, 0, sizeof(app_manager_t));
}

// 注册应用
app_t* app_manager_register_app(const char* name, const char* icon, 
                                app_create_cb_t create_cb, app_destroy_cb_t destroy_cb) {
    if (!name || !create_cb) {
        return NULL;
    }
    
    // 检查是否已存在同名应用
    if (app_manager_get_app(name)) {
        return NULL;
    }
    
    // 创建新应用
    app_t* app = (app_t*)malloc(sizeof(app_t));
    if (!app) {
        return NULL;
    }
    
    memset(app, 0, sizeof(app_t));
    strncpy(app->name, name, sizeof(app->name) - 1);
    if (icon) {
        strncpy(app->icon, icon, sizeof(app->icon) - 1);
    }
    app->type = APP_TYPE_NORMAL;
    app->state = APP_STATE_INACTIVE;
    app->create_cb = create_cb;
    app->destroy_cb = destroy_cb;
    
    // 添加到应用链表
    app->next = g_app_manager.apps;
    g_app_manager.apps = app;
    
    return app;
}

// 注册Overlay
overlay_t* app_manager_register_overlay(const char* name, const char* icon, 
                                        app_create_cb_t create_cb, app_destroy_cb_t destroy_cb,
                                        int z_index, bool auto_start) {
    if (!name || !create_cb) {
        return NULL;
    }
    
    // 检查是否已存在同名Overlay
    if (app_manager_get_overlay(name)) {
        return NULL;
    }
    
    // 创建新Overlay
    overlay_t* overlay = (overlay_t*)malloc(sizeof(overlay_t));
    if (!overlay) {
        return NULL;
    }
    
    memset(overlay, 0, sizeof(overlay_t));
    strncpy(overlay->base.name, name, sizeof(overlay->base.name) - 1);
    if (icon) {
        strncpy(overlay->base.icon, icon, sizeof(overlay->base.icon) - 1);
    }
    overlay->base.type = APP_TYPE_OVERLAY;
    overlay->base.state = APP_STATE_INACTIVE;
    overlay->base.create_cb = create_cb;
    overlay->base.destroy_cb = destroy_cb;
    overlay->z_index = z_index;
    overlay->auto_start = auto_start;
    
    // 按z_index排序插入到Overlay链表
    overlay_t** current = &g_app_manager.overlays;
    while (*current && (*current)->z_index < z_index) {
        current = &(*current)->next;
    }
    overlay->next = *current;
    *current = overlay;
    
    return overlay;
}

// 启动应用
bool app_manager_launch_app(const char* name) {
    if (!name) {
        return false;
    }
    
    app_t* app = app_manager_get_app(name);
    if (!app) {
        return false;
    }
    
    // 如果已经是当前应用，直接返回
    if (g_app_manager.current_app == app) {
        return true;
    }
    
    // 关闭当前应用
    if (g_app_manager.current_app) {
        app_manager_close_current_app();
    }
    
    // 创建应用容器
    app->container = lv_obj_create(g_app_manager.app_container);
    lv_obj_set_size(app->container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(app->container, 0, 0);
    lv_obj_clear_flag(app->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(app->container, 0, 0);
    lv_obj_set_style_border_width(app->container, 0, 0);
    
    // 确保App容器可以接收点击事件
    lv_obj_add_flag(app->container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(app->container, LV_OBJ_FLAG_EVENT_BUBBLE);  // 防止事件冒泡，让App自己处理
    
    // 调用应用创建回调
    if (app->create_cb) {
        app->create_cb(app);
    }
    
    // 更新状态
    app->state = APP_STATE_ACTIVE;
    g_app_manager.current_app = app;
    
    return true;
}

// 关闭当前应用
bool app_manager_close_current_app(void) {
    if (!g_app_manager.current_app) {
        return false;
    }
    
    app_t* app = g_app_manager.current_app;
    
    // 调用销毁回调
    if (app->destroy_cb) {
        app->destroy_cb(app);
    }
    
    // 销毁容器
    if (app->container) {
        lv_obj_del(app->container);
        app->container = NULL;
    }
    
    // 更新状态
    app->state = APP_STATE_INACTIVE;
    g_app_manager.current_app = NULL;
    
    return true;
}

// 显示Overlay
bool app_manager_show_overlay(const char* name) {
    if (!name) {
        return false;
    }
    
    overlay_t* overlay = app_manager_get_overlay(name);
    if (!overlay) {
        return false;
    }
    
    // 如果已经激活，直接返回
    if (overlay->base.state == APP_STATE_ACTIVE) {
        return true;
    }
    
    // 如果还没有创建容器，创建它
    if (!overlay->base.container) {
        overlay->base.container = lv_obj_create(g_app_manager.overlay_container);
        lv_obj_set_size(overlay->base.container, LV_PCT(100), LV_PCT(100));
        lv_obj_set_pos(overlay->base.container, 0, 0);
        lv_obj_clear_flag(overlay->base.container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(overlay->base.container, 0, 0);
        lv_obj_set_style_border_width(overlay->base.container, 0, 0);
        lv_obj_set_style_bg_opa(overlay->base.container, LV_OPA_TRANSP, 0);
        
        // 确保overlay的个人容器也不会拦截事件
        lv_obj_add_flag(overlay->base.container, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_clear_flag(overlay->base.container, LV_OBJ_FLAG_CLICKABLE);
        
        // 调用创建回调
        if (overlay->base.create_cb) {
            overlay->base.create_cb(&overlay->base);
        }
    }
    
    // 显示容器
    lv_obj_clear_flag(overlay->base.container, LV_OBJ_FLAG_HIDDEN);
    
    // 更新状态
    overlay->base.state = APP_STATE_ACTIVE;
    
    return true;
}

// 隐藏Overlay
bool app_manager_hide_overlay(const char* name) {
    if (!name) {
        return false;
    }
    
    overlay_t* overlay = app_manager_get_overlay(name);
    if (!overlay || !overlay->base.container) {
        return false;
    }
    
    // 隐藏容器
    lv_obj_add_flag(overlay->base.container, LV_OBJ_FLAG_HIDDEN);
    
    // 更新状态
    overlay->base.state = APP_STATE_BACKGROUND;
    
    return true;
}

// 查找应用
app_t* app_manager_get_app(const char* name) {
    if (!name) {
        return NULL;
    }
    
    app_t* app = g_app_manager.apps;
    while (app) {
        if (strcmp(app->name, name) == 0) {
            return app;
        }
        app = app->next;
    }
    
    return NULL;
}

// 查找Overlay
overlay_t* app_manager_get_overlay(const char* name) {
    if (!name) {
        return NULL;
    }
    
    overlay_t* overlay = g_app_manager.overlays;
    while (overlay) {
        if (strcmp(overlay->base.name, name) == 0) {
            return overlay;
        }
        overlay = overlay->next;
    }
    
    return NULL;
}

// 获取当前应用
app_t* app_manager_get_current_app(void) {
    return g_app_manager.current_app;
}

// 获取应用列表
app_t* app_manager_get_app_list(void) {
    return g_app_manager.apps;
}

// 获取Overlay列表
overlay_t* app_manager_get_overlay_list(void) {
    return g_app_manager.overlays;
}

// 跳转到启动器
void app_manager_go_to_launcher(void) {
    app_manager_launch_app("Launcher");
}

// 检查是否是启动器激活状态
bool app_manager_is_launcher_active(void) {
    return g_app_manager.current_app && 
           strcmp(g_app_manager.current_app->name, "Launcher") == 0;
} 