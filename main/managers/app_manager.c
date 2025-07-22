#include "managers/app_manager.h"
#include <string.h>
#include <stdlib.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 全局应用管理器实例
static app_manager_t g_app_manager = {0};

// 内存监控结构
typedef struct {
    size_t free_heap_before;
    size_t free_heap_after;
    size_t psram_free_before;
    size_t psram_free_after;
    uint32_t gc_count;
    uint32_t last_gc_time;
} memory_monitor_t;

static memory_monitor_t g_memory_monitor = {0};

// 内存阈值配置 - 更宽松的阈值以减少频繁GC
#define MEMORY_LOW_THRESHOLD    (128 * 1024)   // 128KB低内存阈值
#define MEMORY_CRITICAL_THRESHOLD (64 * 1024)  // 64KB临界阈值
#define PSRAM_LOW_THRESHOLD     (512 * 1024)   // 512KB PSRAM低内存阈值
#define FORCED_GC_INTERVAL      (10000)        // 10秒强制GC间隔
#define MEMORY_MONITOR_INTERVAL (5000)         // 5秒内存监控间隔

// 内存监控定时器
static lv_timer_t* g_memory_monitor_timer = NULL;

// 前向声明
static void force_garbage_collection(void);
static void log_memory_usage(const char* context);
static bool wait_for_memory_stabilization(uint32_t timeout_ms);
static bool should_force_gc(void);
static void memory_monitor_timer_cb(lv_timer_t* timer);
static void start_memory_monitor(void);
static void stop_memory_monitor(void);
static void* safe_app_malloc(size_t size);
static void safe_app_free(void* ptr);
static void cleanup_app_memory(app_t* app);

// 安全的内存分配函数 - 优先使用PSRAM
static void* safe_app_malloc(size_t size) {
    void* ptr = NULL;
    
    // 首先尝试使用PSRAM（如果可用且足够）
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psram_free >= size + PSRAM_LOW_THRESHOLD) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (ptr) {
            printf("App allocated %zu bytes from PSRAM (free: %zu)\n", size, psram_free);
            return ptr;
        }
    }
    
    // 如果PSRAM不够或失败，使用常规内存
    size_t heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    if (heap_free >= size + MEMORY_LOW_THRESHOLD) {
        ptr = malloc(size);
        if (ptr) {
            printf("App allocated %zu bytes from regular heap (free: %zu)\n", size, heap_free);
            return ptr;
        }
    }
    
    printf("Failed to allocate %zu bytes - insufficient memory\n", size);
    return NULL;
}

// 安全的内存释放函数
static void safe_app_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

// 清理应用内存
static void cleanup_app_memory(app_t* app) {
    if (!app) return;
    
    printf("Cleaning up memory for app: %s\n", app->name);
    
    // 清理用户数据
    if (app->user_data) {
        printf("Cleaning user data for app: %s\n", app->name);
        // 不直接释放用户数据，让应用的destroy回调处理
        app->user_data = NULL;
    }
    
    // 确保LVGL完成所有渲染和清理
    lv_refr_now(NULL);
    vTaskDelay(pdMS_TO_TICKS(30));  // 增加等待时间确保清理完成
    
    // 强制垃圾回收
    if (should_force_gc()) {
        printf("Forcing GC after app cleanup\n");
        force_garbage_collection();
    }
}

// 获取应用管理器实例
app_manager_t* app_manager_get_instance(void) {
    return &g_app_manager;
}

// 强制垃圾回收
static void force_garbage_collection(void) {
    printf("=== FORCING GARBAGE COLLECTION ===\n");
    
    // 记录GC前的内存状态
    g_memory_monitor.free_heap_before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    g_memory_monitor.psram_free_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    // 强制LVGL内存清理
    lv_mem_monitor_t lvgl_mem_before;
    lv_mem_monitor(&lvgl_mem_before);
    
    // 清理LVGL缓存
    lv_obj_clean(lv_screen_active());
    lv_refr_now(NULL);
    
    // 让系统有时间释放内存
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 跳过抽屉清理，让空闲清理机制处理
    printf("Skipping drawer cleanup during GC to avoid conflicts\n");
    
    // 再次让系统有时间释放内存
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 记录GC后的内存状态
    g_memory_monitor.free_heap_after = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    g_memory_monitor.psram_free_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    g_memory_monitor.gc_count++;
    g_memory_monitor.last_gc_time = esp_timer_get_time() / 1000;
    
    lv_mem_monitor_t lvgl_mem_after;
    lv_mem_monitor(&lvgl_mem_after);
    
    printf("GC #%lu completed:\n", (unsigned long)g_memory_monitor.gc_count);
    printf("  Heap: %zu -> %zu (+%d bytes)\n", 
           g_memory_monitor.free_heap_before, 
           g_memory_monitor.free_heap_after,
           (int)(g_memory_monitor.free_heap_after - g_memory_monitor.free_heap_before));
    printf("  PSRAM: %zu -> %zu (+%d bytes)\n", 
           g_memory_monitor.psram_free_before, 
           g_memory_monitor.psram_free_after,
           (int)(g_memory_monitor.psram_free_after - g_memory_monitor.psram_free_before));
    printf("  LVGL: %zu -> %zu (+%d bytes)\n", 
           (size_t)lvgl_mem_before.free_size, 
           (size_t)lvgl_mem_after.free_size,
           (int)(lvgl_mem_after.free_size - lvgl_mem_before.free_size));
    printf("=== GC COMPLETE ===\n");
}

// 等待内存稳定
static bool wait_for_memory_stabilization(uint32_t timeout_ms) {
    uint32_t start_time = esp_timer_get_time() / 1000;
    size_t prev_free = 0;
    int stable_count = 0;
    
    while ((esp_timer_get_time() / 1000 - start_time) < timeout_ms) {
        size_t current_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        
        if (prev_free > 0 && abs((int)(current_free - prev_free)) < 1024) {
            stable_count++;
            if (stable_count >= 3) {
                printf("Memory stabilized after %lu ms\n", 
                       (unsigned long)(esp_timer_get_time() / 1000 - start_time));
                return true;
            }
        } else {
            stable_count = 0;
        }
        
        prev_free = current_free;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    printf("Memory stabilization timeout after %lu ms\n", (unsigned long)timeout_ms);
    return false;
}

// 记录内存使用情况
static void log_memory_usage(const char* context) {
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    
    lv_mem_monitor_t lvgl_mem;
    lv_mem_monitor(&lvgl_mem);
    
    printf("=== MEMORY USAGE [%s] ===\n", context);
    printf("  Free Heap (8bit): %zu bytes\n", free_heap);
    printf("  Free PSRAM: %zu bytes\n", free_psram);
    printf("  Free 32bit: %zu bytes\n", free_32bit);
    printf("  LVGL Free: %zu bytes (used: %u%%)\n", 
           (size_t)lvgl_mem.free_size, lvgl_mem.used_pct);
    
    // 内存警告
    if (free_heap < MEMORY_CRITICAL_THRESHOLD) {
        printf("  *** CRITICAL LOW MEMORY WARNING ***\n");
    } else if (free_heap < MEMORY_LOW_THRESHOLD) {
        printf("  *** LOW MEMORY WARNING ***\n");
    }
    
    printf("=== END MEMORY USAGE ===\n");
}

// 检查是否需要强制GC
static bool should_force_gc(void) {
    uint32_t current_time = esp_timer_get_time() / 1000;
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    // 只有内存严重不足时才强制GC，优先考虑PSRAM使用情况
    if (free_heap < MEMORY_CRITICAL_THRESHOLD && free_psram < (256 * 1024)) {
        printf("Critical memory shortage detected (Heap: %zu, PSRAM: %zu), forcing GC\n", free_heap, free_psram);
        return true;
    }
    
    return false;
}

// 内存监控定时器回调
static void memory_monitor_timer_cb(lv_timer_t* timer) {
    (void)timer; // 未使用的参数
    
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    // 检查内存使用情况，只在真正严重不足时干预
    if (free_heap < MEMORY_CRITICAL_THRESHOLD) {
        printf("*** CRITICAL MEMORY ALERT: %zu bytes free ***\n", free_heap);
        printf("Triggering emergency garbage collection\n");
        force_garbage_collection();
        
        // 记录紧急GC后的内存状态
        free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        printf("After emergency GC: %zu bytes free\n", free_heap);
        
        if (free_heap < MEMORY_CRITICAL_THRESHOLD) {
            printf("*** MEMORY STILL CRITICAL AFTER GC ***\n");
        }
    }
    // 移除低内存警告的主动清理，减少干预
    
    // 定期记录内存使用情况（每15秒）
    static uint32_t last_log_time = 0;
    uint32_t current_time = esp_timer_get_time() / 1000;
    if (current_time - last_log_time > 15000) {
        printf("Memory Monitor: Heap=%zu, PSRAM=%zu, GC_Count=%lu\n", 
               free_heap, free_psram, (unsigned long)g_memory_monitor.gc_count);
        last_log_time = current_time;
    }
}

// 启动内存监控
static void start_memory_monitor(void) {
    if (g_memory_monitor_timer != NULL) {
        printf("Memory monitor already running\n");
        return;
    }
    
    printf("Starting memory monitor (interval: %d ms)\n", MEMORY_MONITOR_INTERVAL);
    g_memory_monitor_timer = lv_timer_create(memory_monitor_timer_cb, 
                                           MEMORY_MONITOR_INTERVAL, NULL);
    if (g_memory_monitor_timer) {
        printf("Memory monitor started successfully\n");
    } else {
        printf("Failed to start memory monitor\n");
    }
}

// 停止内存监控
static void stop_memory_monitor(void) {
    if (g_memory_monitor_timer) {
        printf("Stopping memory monitor\n");
        lv_timer_del(g_memory_monitor_timer);
        g_memory_monitor_timer = NULL;
    }
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
    
    // 启动内存监控
    start_memory_monitor();
    
    g_app_manager.initialized = true;
}

// 销毁应用管理器
void app_manager_deinit(void) {
    if (!g_app_manager.initialized) {
        return;
    }
    
    // 停止内存监控
    stop_memory_monitor();
    
    // 销毁所有应用
    app_t* app = g_app_manager.apps;
    while (app) {
        app_t* next = app->next;
        if (app->destroy_cb) {
            app->destroy_cb(app);
        }
        safe_app_free(app);
        app = next;
    }
    
    // 销毁所有Overlay
    overlay_t* overlay = g_app_manager.overlays;
    while (overlay) {
        overlay_t* next = overlay->next;
        if (overlay->base.destroy_cb) {
            overlay->base.destroy_cb(&overlay->base);
        }
        safe_app_free(overlay);
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
    
    // 创建新应用 - 优先使用PSRAM
    app_t* app = (app_t*)safe_app_malloc(sizeof(app_t));
    if (!app) {
        printf("Failed to allocate memory for app: %s\n", name);
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
    
    // 创建新Overlay - 优先使用PSRAM
    overlay_t* overlay = (overlay_t*)safe_app_malloc(sizeof(overlay_t));
    if (!overlay) {
        printf("Failed to allocate memory for overlay: %s\n", name);
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
    
    printf("Launching app: %s\n", name);
    log_memory_usage("Before app launch");
    
    app_t* app = app_manager_get_app(name);
    if (!app) {
        printf("App not found: %s\n", name);
        return false;
    }
    
    // 如果已经是当前应用，直接返回
    if (g_app_manager.current_app == app) {
        printf("App %s already active\n", name);
        return true;
    }
    
    // 关闭当前应用并强制等待内存释放
    if (g_app_manager.current_app) {
        printf("Closing current app: %s\n", g_app_manager.current_app->name);
        app_manager_close_current_app();
        
        // 等待内存稳定
        if (!wait_for_memory_stabilization(2000)) {
            printf("Warning: Memory may not be fully released\n");
        }
        
        // 检查是否需要强制GC
        if (should_force_gc()) {
            force_garbage_collection();
        }
    }
    
    log_memory_usage("After previous app cleanup");
    
    // 检查内存是否足够
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    if (free_heap < MEMORY_LOW_THRESHOLD) {
        printf("Low memory detected (%zu bytes), forcing GC before launch\n", free_heap);
        force_garbage_collection();
        
        // 再次检查内存
        free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        if (free_heap < MEMORY_CRITICAL_THRESHOLD) {
            printf("Critical memory shortage (%zu bytes), cannot launch app\n", free_heap);
            return false;
        }
    }
    
    // 创建应用容器
    app->container = lv_obj_create(g_app_manager.app_container);
    if (!app->container) {
        printf("Failed to create app container for %s\n", name);
        return false;
    }
    
    lv_obj_set_size(app->container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(app->container, 0, 0);
    lv_obj_clear_flag(app->container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(app->container, 0, 0);
    lv_obj_set_style_border_width(app->container, 0, 0);
    
    // 确保App容器可以接收点击事件
    lv_obj_add_flag(app->container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(app->container, LV_OBJ_FLAG_EVENT_BUBBLE);  // 防止事件冒泡，让App自己处理
    
    // 调用应用创建回调
    printf("Creating app UI for %s\n", name);
    if (app->create_cb) {
        app->create_cb(app);
    }
    
    // 更新状态
    app->state = APP_STATE_ACTIVE;
    g_app_manager.current_app = app;
    
    log_memory_usage("After app creation");
    printf("App %s launched successfully\n", name);
    
    return true;
}

// 关闭当前应用
bool app_manager_close_current_app(void) {
    if (!g_app_manager.current_app) {
        return false;
    }
    
    app_t* app = g_app_manager.current_app;
    printf("Closing app: %s\n", app->name);
    
    // 调用销毁回调
    if (app->destroy_cb) {
        printf("Calling destroy callback for %s\n", app->name);
        app->destroy_cb(app);
    }
    
    // 销毁容器
    if (app->container) {
        printf("Destroying UI container for %s\n", app->name);
        // 先清理容器内容，再删除容器
        lv_obj_clean(app->container);
        lv_refr_now(NULL);  // 强制刷新以完成清理
        
        // 增加延时确保LVGL任务完成
        vTaskDelay(pdMS_TO_TICKS(20));
        
        lv_obj_del(app->container);
        app->container = NULL;
    }
    
    // 更新状态
    app->state = APP_STATE_INACTIVE;
    g_app_manager.current_app = NULL;
    
    // 清理应用内存
    cleanup_app_memory(app);
    
    // 等待内存稳定
    wait_for_memory_stabilization(150);
    
    printf("App %s closed\n", app->name);
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
    app_manager_launch_app("启动器");
}

// 检查是否是启动器激活状态
bool app_manager_is_launcher_active(void) {
    return g_app_manager.current_app && 
           strcmp(g_app_manager.current_app->name, "启动器") == 0;
}

// 公共API：强制垃圾回收
void app_manager_force_gc(void) {
    force_garbage_collection();
}

// 公共API：记录内存使用情况
void app_manager_log_memory_usage(const char* context) {
    log_memory_usage(context);
}

// 公共API：检查内存是否充足
bool app_manager_check_memory_sufficient(void) {
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    return free_heap >= MEMORY_LOW_THRESHOLD;
}

// 公共API：获取内存监控统计信息
void app_manager_get_memory_stats(uint32_t* gc_count, size_t* free_heap, size_t* free_psram) {
    if (gc_count) {
        *gc_count = g_memory_monitor.gc_count;
    }
    if (free_heap) {
        *free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    }
    if (free_psram) {
        *free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    }
} 