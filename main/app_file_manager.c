#include "app_file_manager.h"
#include "app_manager.h"
#include "hal_sdcard.h"
#include "menu_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>

// 声明自定义字体
LV_FONT_DECLARE(simhei_32);

// 文件项类型
typedef enum {
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_FILE,
    FILE_TYPE_PARENT
} file_type_t;

// 文件项结构
typedef struct {
    char name[256];           // 文件名（支持长文件名）
    char full_path[512];      // 完整路径
    file_type_t type;         // 文件类型
    size_t size;              // 文件大小（字节）
    uint32_t modified_time;   // 修改时间
    bool is_selected;         // 是否被选中
} file_item_t;

// 文件管理器状态
typedef struct {
    lv_obj_t* menu;              // 主菜单容器
    lv_obj_t* path_label;        // 路径显示标签
    lv_obj_t* file_list;         // 文件列表容器
    lv_obj_t* status_bar;        // 状态栏
    lv_obj_t* action_buttons;    // 操作按钮容器
    
    file_item_t* files;          // 文件列表
    uint32_t file_count;         // 文件数量
    uint32_t selected_count;     // 选中文件数量
    
    char current_path[512];      // 当前路径
    char root_path[512];         // 根路径
    
    bool is_initialized;         // 初始化标志
    bool is_scanning;            // 扫描标志
} file_manager_state_t;

// 全局状态
static file_manager_state_t* g_file_manager_state = NULL;

// 前向声明
static void file_manager_create(app_t* app);
static void file_manager_destroy(app_t* app);
static void scan_directory(const char* path);
static void create_file_list_ui(void);
static void update_path_display(void);
static void update_status_bar(void);
static void create_action_buttons(void);
static void file_item_event_cb(lv_event_t* e);
static void action_button_event_cb(lv_event_t* e);
static void* safe_malloc(size_t size);
static void safe_free(void* ptr);
static void cleanup_file_list(void);
static bool is_hidden_file(const char* name);
static char* get_file_extension(const char* filename);
static const char* get_file_icon(const char* filename, file_type_t type);
static void format_file_size(size_t size, char* buffer, size_t buffer_size);

// 安全的内存分配函数
static void* safe_malloc(size_t size) {
    void* ptr = NULL;
    
    // 首先尝试使用PSRAM
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >= size) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (ptr) {
            printf("File manager allocated %zu bytes from PSRAM\n", size);
            return ptr;
        }
    }
    
    // 如果PSRAM不够，使用常规内存
    ptr = malloc(size);
    if (ptr) {
        printf("File manager allocated %zu bytes from regular heap\n", size);
    } else {
        printf("Failed to allocate %zu bytes for file manager\n", size);
    }
    
    return ptr;
}

// 安全的内存释放函数
static void safe_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

// 清理文件列表
static void cleanup_file_list(void) {
    if (g_file_manager_state && g_file_manager_state->files) {
        safe_free(g_file_manager_state->files);
        g_file_manager_state->files = NULL;
        g_file_manager_state->file_count = 0;
        g_file_manager_state->selected_count = 0;
    }
}

// 检查是否为隐藏文件
static bool is_hidden_file(const char* name) {
    return name[0] == '.';
}

// 获取文件扩展名
static char* get_file_extension(const char* filename) {
    char* ext = strrchr(filename, '.');
    return ext ? ext + 1 : NULL;
}

// 获取文件图标
static const char* get_file_icon(const char* filename, file_type_t type) {
    if (type == FILE_TYPE_DIRECTORY) {
        return LV_SYMBOL_DIRECTORY;
    } else if (type == FILE_TYPE_PARENT) {
        return LV_SYMBOL_UP;
    }
    
    // 根据文件扩展名返回不同图标
    char* ext = get_file_extension(filename);
    if (!ext) {
        return LV_SYMBOL_FILE;
    }
    
    // 转换为小写进行比较
    char ext_lower[16];
    strncpy(ext_lower, ext, sizeof(ext_lower) - 1);
    ext_lower[sizeof(ext_lower) - 1] = '\0';
    for (int i = 0; ext_lower[i]; i++) {
        if (ext_lower[i] >= 'A' && ext_lower[i] <= 'Z') {
            ext_lower[i] = ext_lower[i] - 'A' + 'a';
        }
    }
    
    // 根据扩展名返回图标
    if (strcmp(ext_lower, "mp3") == 0 || strcmp(ext_lower, "wav") == 0) {
        return LV_SYMBOL_AUDIO;
    } else if (strcmp(ext_lower, "jpg") == 0 || strcmp(ext_lower, "png") == 0 || 
               strcmp(ext_lower, "bmp") == 0 || strcmp(ext_lower, "gif") == 0) {
        return LV_SYMBOL_IMAGE;
    } else if (strcmp(ext_lower, "txt") == 0 || strcmp(ext_lower, "log") == 0) {
        return LV_SYMBOL_EDIT;
    } else if (strcmp(ext_lower, "pdf") == 0) {
        return LV_SYMBOL_DOWNLOAD;
    } else {
        return LV_SYMBOL_FILE;
    }
}

// 格式化文件大小
static void format_file_size(size_t size, char* buffer, size_t buffer_size) {
    if (size < 1024) {
        snprintf(buffer, buffer_size, "%zu B", size);
    } else if (size < 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f KB", (float)size / 1024);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(buffer, buffer_size, "%.1f MB", (float)size / (1024 * 1024));
    } else {
        snprintf(buffer, buffer_size, "%.1f GB", (float)size / (1024 * 1024 * 1024));
    }
}

// 扫描目录
static void scan_directory(const char* path) {
    if (!g_file_manager_state || !path) {
        return;
    }
    
    printf("Scanning directory: %s\n", path);
    app_manager_log_memory_usage("Before directory scan");
    
    // 清理之前的文件列表
    cleanup_file_list();
    
    // 检查SD卡是否挂载
    if (!hal_sdcard_is_mounted()) {
        printf("SD card not mounted\n");
        return;
    }
    
    g_file_manager_state->is_scanning = true;
    
    // 打开目录
    DIR* dir = opendir(path);
    if (!dir) {
        printf("Failed to open directory: %s\n", path);
        g_file_manager_state->is_scanning = false;
        return;
    }
    
    // 第一次扫描：计算文件数量
    struct dirent* entry;
    uint32_t file_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过隐藏文件
        if (is_hidden_file(entry->d_name)) {
            continue;
        }
        file_count++;
    }
    
    // 分配内存
    g_file_manager_state->files = (file_item_t*)safe_malloc(file_count * sizeof(file_item_t));
    if (!g_file_manager_state->files) {
        printf("Failed to allocate memory for file list\n");
        closedir(dir);
        g_file_manager_state->is_scanning = false;
        return;
    }
    
    // 重新扫描目录，填充文件信息
    rewinddir(dir);
    g_file_manager_state->file_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // 跳过隐藏文件
        if (is_hidden_file(entry->d_name)) {
            continue;
        }
        
        file_item_t* file = &g_file_manager_state->files[g_file_manager_state->file_count];
        
        // 设置文件名
        strncpy(file->name, entry->d_name, sizeof(file->name) - 1);
        file->name[sizeof(file->name) - 1] = '\0';
        
        // 设置完整路径
        snprintf(file->full_path, sizeof(file->full_path), "%s/%s", path, entry->d_name);
        
        // 设置文件类型
        if (entry->d_type == DT_DIR) {
            file->type = FILE_TYPE_DIRECTORY;
        } else {
            file->type = FILE_TYPE_FILE;
        }
        
        // 获取文件信息
        struct stat st;
        if (stat(file->full_path, &st) == 0) {
            file->size = st.st_size;
            file->modified_time = st.st_mtime;
        } else {
            file->size = 0;
            file->modified_time = 0;
        }
        
        file->is_selected = false;
        g_file_manager_state->file_count++;
    }
    
    closedir(dir);
    g_file_manager_state->is_scanning = false;
    
    printf("Scanned %lu files in directory\n", (unsigned long)g_file_manager_state->file_count);
    app_manager_log_memory_usage("After directory scan");
}

// 创建文件列表UI
static void create_file_list_ui(void) {
    if (!g_file_manager_state || !g_file_manager_state->file_list) {
        return;
    }
    
    // 清空现有列表
    lv_obj_clean(g_file_manager_state->file_list);
    
    // 设置列表布局
    lv_obj_set_layout(g_file_manager_state->file_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_file_manager_state->file_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_file_manager_state->file_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(g_file_manager_state->file_list, 8, 0);
    
    // 创建文件项
    for (uint32_t i = 0; i < g_file_manager_state->file_count; i++) {
        file_item_t* file = &g_file_manager_state->files[i];
        
        // 创建文件项容器
        lv_obj_t* item_container = lv_obj_create(g_file_manager_state->file_list);
        lv_obj_set_size(item_container, LV_PCT(100), 60);
        
        // 设置样式
        lv_obj_set_style_bg_opa(item_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(item_container, 0, 0);
        lv_obj_set_style_pad_all(item_container, 8, 0);
        
        // 选中效果
        lv_obj_set_style_bg_color(item_container, lv_color_hex(0xE3F2FD), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(item_container, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_set_style_radius(item_container, 8, LV_STATE_PRESSED);
        
        // 让容器可以接收点击事件
        lv_obj_add_flag(item_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(item_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(item_container, LV_OBJ_FLAG_EVENT_BUBBLE);
        
        // 创建图标
        lv_obj_t* icon = lv_label_create(item_container);
        lv_label_set_text(icon, get_file_icon(file->name, file->type));
        lv_obj_set_style_text_color(icon, lv_color_hex(0x2196F3), 0);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
        
        // 创建文件名标签
        lv_obj_t* name_label = lv_label_create(item_container);
        lv_label_set_text(name_label, file->name);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0x333333), 0);
        lv_obj_set_style_text_font(name_label, &simhei_32, 0);
        lv_obj_align_to(name_label, icon, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
        
        // 创建文件信息标签
        lv_obj_t* info_label = lv_label_create(item_container);
        char info_text[64];
        if (file->type == FILE_TYPE_DIRECTORY) {
            snprintf(info_text, sizeof(info_text), "Directory");
        } else {
            format_file_size(file->size, info_text, sizeof(info_text));
        }
        lv_label_set_text(info_label, info_text);
        lv_obj_set_style_text_color(info_label, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
        lv_obj_align(info_label, LV_ALIGN_RIGHT_MID, -8, 0);
        
        // 添加点击事件
        lv_obj_add_event_cb(item_container, file_item_event_cb, LV_EVENT_CLICKED, file);
    }
}

// 更新路径显示
static void update_path_display(void) {
    if (!g_file_manager_state || !g_file_manager_state->path_label) {
        return;
    }
    
    lv_label_set_text(g_file_manager_state->path_label, g_file_manager_state->current_path);
}

// 更新状态栏
static void update_status_bar(void) {
    if (!g_file_manager_state || !g_file_manager_state->status_bar) {
        return;
    }
    
    char status_text[128];
    snprintf(status_text, sizeof(status_text), 
             "Files: %lu | Selected: %lu", 
             (unsigned long)g_file_manager_state->file_count,
             (unsigned long)g_file_manager_state->selected_count);
    
    lv_label_set_text(g_file_manager_state->status_bar, status_text);
}

// 创建操作按钮
static void create_action_buttons(void) {
    if (!g_file_manager_state || !g_file_manager_state->action_buttons) {
        return;
    }
    
    // 清空现有按钮
    lv_obj_clean(g_file_manager_state->action_buttons);
    
    // 设置按钮容器布局
    lv_obj_set_layout(g_file_manager_state->action_buttons, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_file_manager_state->action_buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_file_manager_state->action_buttons, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 创建按钮
    const char* button_texts[] = {"Copy", "Delete", "Rename", "New Folder"};
    const char* button_icons[] = {LV_SYMBOL_COPY, LV_SYMBOL_TRASH, LV_SYMBOL_EDIT, LV_SYMBOL_DIRECTORY};
    
    for (int i = 0; i < 4; i++) {
        lv_obj_t* button = lv_btn_create(g_file_manager_state->action_buttons);
        lv_obj_set_size(button, 80, 40);
        
        // 设置按钮样式
        lv_obj_set_style_bg_color(button, lv_color_hex(0x2196F3), 0);
        lv_obj_set_style_text_color(button, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_radius(button, 8, 0);
        
        // 创建按钮内容
        lv_obj_t* label = lv_label_create(button);
        lv_label_set_text_fmt(label, "%s\n%s", button_icons[i], button_texts[i]);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        
        // 添加点击事件
        lv_obj_add_event_cb(button, action_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

// 文件项点击事件
static void file_item_event_cb(lv_event_t* e) {
    file_item_t* file = (file_item_t*)lv_event_get_user_data(e);
    if (!file) {
        return;
    }
    
    printf("File clicked: %s\n", file->name);
    
    if (file->type == FILE_TYPE_DIRECTORY) {
        // 进入目录
        strncpy(g_file_manager_state->current_path, file->full_path, sizeof(g_file_manager_state->current_path) - 1);
        g_file_manager_state->current_path[sizeof(g_file_manager_state->current_path) - 1] = '\0';
        
        // 重新扫描目录
        scan_directory(g_file_manager_state->current_path);
        create_file_list_ui();
        update_path_display();
        update_status_bar();
    } else {
        // 文件操作（这里可以添加文件预览等功能）
        printf("File selected: %s (size: %zu bytes)\n", file->name, file->size);
    }
}

// 操作按钮点击事件
static void action_button_event_cb(lv_event_t* e) {
    int button_id = (int)(intptr_t)lv_event_get_user_data(e);
    
    printf("Action button clicked: %d\n", button_id);
    
    // 这里可以添加具体的文件操作实现
    switch (button_id) {
        case 0: // 复制
            printf("Copy action\n");
            break;
        case 1: // 删除
            printf("Delete action\n");
            break;
        case 2: // 重命名
            printf("Rename action\n");
            break;
        case 3: // 新建文件夹
            printf("New folder action\n");
            break;
    }
}

// 文件管理器App创建
static void file_manager_create(app_t* app) {
    if (!app || !app->container) {
        return;
    }
    
    printf("Creating file manager app\n");
    app_manager_log_memory_usage("Before file manager creation");
    
    // 分配状态结构
    g_file_manager_state = (file_manager_state_t*)safe_malloc(sizeof(file_manager_state_t));
    if (!g_file_manager_state) {
        printf("Failed to allocate file manager state\n");
        return;
    }
    
    memset(g_file_manager_state, 0, sizeof(file_manager_state_t));
    
    // 设置初始路径
    const char* mount_point = hal_sdcard_get_mount_point();
    if (mount_point) {
        strncpy(g_file_manager_state->current_path, mount_point, sizeof(g_file_manager_state->current_path) - 1);
        strncpy(g_file_manager_state->root_path, mount_point, sizeof(g_file_manager_state->root_path) - 1);
    } else {
        strcpy(g_file_manager_state->current_path, "/sdcard");
        strcpy(g_file_manager_state->root_path, "/sdcard");
    }
    
    // 创建主容器
    g_file_manager_state->menu = lv_obj_create(app->container);
    lv_obj_set_size(g_file_manager_state->menu, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(g_file_manager_state->menu, 0, 0);
    lv_obj_set_style_pad_all(g_file_manager_state->menu, 16, 0);
    
    // 创建路径显示
    g_file_manager_state->path_label = lv_label_create(g_file_manager_state->menu);
    lv_label_set_text(g_file_manager_state->path_label, g_file_manager_state->current_path);
    lv_obj_set_style_text_color(g_file_manager_state->path_label, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_text_font(g_file_manager_state->path_label, &simhei_32, 0);
    lv_obj_align(g_file_manager_state->path_label, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 创建状态栏
    g_file_manager_state->status_bar = lv_label_create(g_file_manager_state->menu);
    lv_label_set_text(g_file_manager_state->status_bar, "文件: 0 | 选中: 0");
    lv_obj_set_style_text_color(g_file_manager_state->status_bar, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(g_file_manager_state->status_bar, &lv_font_montserrat_14, 0);
    lv_obj_align_to(g_file_manager_state->status_bar, g_file_manager_state->path_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
    
    // 创建文件列表容器
    g_file_manager_state->file_list = lv_obj_create(g_file_manager_state->menu);
    lv_obj_set_size(g_file_manager_state->file_list, LV_PCT(100), LV_PCT(70));
    lv_obj_align_to(g_file_manager_state->file_list, g_file_manager_state->status_bar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
    lv_obj_set_style_bg_opa(g_file_manager_state->file_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_file_manager_state->file_list, 0, 0);
    lv_obj_set_style_pad_all(g_file_manager_state->file_list, 8, 0);
    
    // 创建操作按钮容器
    g_file_manager_state->action_buttons = lv_obj_create(g_file_manager_state->menu);
    lv_obj_set_size(g_file_manager_state->action_buttons, LV_PCT(100), 60);
    lv_obj_align_to(g_file_manager_state->action_buttons, g_file_manager_state->file_list, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
    lv_obj_set_style_bg_opa(g_file_manager_state->action_buttons, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_file_manager_state->action_buttons, 0, 0);
    lv_obj_set_style_pad_all(g_file_manager_state->action_buttons, 8, 0);
    
    // 扫描目录并创建UI
    scan_directory(g_file_manager_state->current_path);
    create_file_list_ui();
    update_path_display();
    update_status_bar();
    create_action_buttons();
    
    g_file_manager_state->is_initialized = true;
    
    app->user_data = g_file_manager_state;
    
    printf("File manager app created successfully\n");
    app_manager_log_memory_usage("After file manager creation");
}

// 文件管理器App销毁
static void file_manager_destroy(app_t* app) {
    if (!app) {
        return;
    }
    
    printf("Destroying file manager app\n");
    app_manager_log_memory_usage("Before file manager destruction");
    
    if (g_file_manager_state) {
        // 清理文件列表
        cleanup_file_list();
        
        // 重置状态
        g_file_manager_state->is_initialized = false;
        g_file_manager_state->is_scanning = false;
        
        // 等待LVGL任务完成
        lv_refr_now(NULL);
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // 释放状态结构
        safe_free(g_file_manager_state);
        g_file_manager_state = NULL;
    }
    
    if (app) {
        app->user_data = NULL;
    }
    
    printf("File manager app destroyed\n");
    app_manager_log_memory_usage("After file manager destruction");
}

// 注册文件管理器App
void register_file_manager_app(void) {
    app_manager_register_app("File Manager", LV_SYMBOL_DIRECTORY, 
                             file_manager_create, file_manager_destroy);
} 