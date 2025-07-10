#include "app_music_player.h"
#include "app_manager.h"
#include "hal_sdcard.h"
#include "hal_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <math.h>

// 高DPI屏幕尺寸 (1280x720)
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// UI 元素尺寸 (为高DPI屏幕优化)
#define TITLE_HEIGHT 80
#define BUTTON_HEIGHT 60
#define BUTTON_WIDTH 200
#define LIST_ITEM_HEIGHT 80
#define LARGE_FONT_SIZE 24
#define MEDIUM_FONT_SIZE 18
#define SMALL_FONT_SIZE 14

// 全局音乐播放器数据
static music_player_data_t g_music_data = {
    .files = NULL,
    .file_count = 0,
    .current_index = 0,
    .is_scanning = false,
    .sd_card_mounted = false,
    .play_state = PLAY_STATE_STOPPED,
    .play_position = 0,
    .play_duration = 0,
    .repeat_mode = false,
    .shuffle_mode = false
};

// UI 元素指针
static lv_obj_t* g_play_pause_btn = NULL;
static lv_obj_t* g_prev_btn = NULL;
static lv_obj_t* g_next_btn = NULL;
static lv_obj_t* g_current_song_label = NULL;
static lv_obj_t* g_progress_bar = NULL;
static lv_obj_t* g_time_label = NULL;

// 文件列表点击事件处理
static void file_list_event_cb(lv_event_t* e);

// 播放控制按钮事件处理
static void play_pause_button_event_cb(lv_event_t* e);
static void prev_button_event_cb(lv_event_t* e);
static void next_button_event_cb(lv_event_t* e);

// 刷新文件列表显示
static void refresh_file_list(lv_obj_t* list);

// 检查SD卡是否挂载
static bool is_sd_card_mounted(void);

// UI更新定时器回调
static void ui_update_timer_cb(lv_timer_t* timer);

bool is_mp3_file(const char* filename) {
    if (!filename) return false;
    
    size_t len = strlen(filename);
    if (len < 4) return false;
    
    // 检查文件扩展名
    const char* ext = filename + len - 4;
    return (strcasecmp(ext, ".mp3") == 0);
}

void extract_title_from_filename(const char* filename, char* title, size_t title_size) {
    if (!filename || !title || title_size == 0) return;
    
    // 找到最后一个路径分隔符
    const char* basename = strrchr(filename, '/');
    if (basename) {
        basename++; // 跳过路径分隔符
    } else {
        basename = filename;
    }
    
    // 复制文件名（不包括扩展名）
    strncpy(title, basename, title_size - 1);
    title[title_size - 1] = '\0';
    
    // 移除.mp3扩展名
    char* dot = strrchr(title, '.');
    if (dot && strcasecmp(dot, ".mp3") == 0) {
        *dot = '\0';
    }
    
    // 将下划线替换为空格，提升可读性
    for (char* p = title; *p; p++) {
        if (*p == '_') {
            *p = ' ';
        }
    }
}

static bool is_sd_card_mounted(void) {
    return hal_sdcard_is_mounted();
}

uint32_t scan_mp3_files(music_player_data_t* data) {
    if (!data) return 0;
    
    // 释放之前的文件列表
    free_mp3_files(data);
    
    // 检查SD卡是否挂载
    data->sd_card_mounted = is_sd_card_mounted();
    if (!data->sd_card_mounted) {
        printf("SD card not mounted\n");
        return 0;
    }
    
    data->is_scanning = true;
    
    const char* mount_point = hal_sdcard_get_mount_point();
    DIR* dir = opendir(mount_point);
    if (!dir) {
        printf("Failed to open SD card directory: %s\n", mount_point);
        data->is_scanning = false;
        return 0;
    }
    
    // 第一次扫描：计算MP3文件数量
    struct dirent* entry;
    uint32_t mp3_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && is_mp3_file(entry->d_name)) {
            mp3_count++;
        }
    }
    
    if (mp3_count == 0) {
        printf("No MP3 files found\n");
        closedir(dir);
        data->is_scanning = false;
        return 0;
    }
    
    // 分配内存
    data->files = (mp3_file_info_t*)malloc(mp3_count * sizeof(mp3_file_info_t));
    if (!data->files) {
        printf("Failed to allocate memory for MP3 files\n");
        closedir(dir);
        data->is_scanning = false;
        return 0;
    }
    
    // 重新扫描目录
    rewinddir(dir);
    data->file_count = 0;
    
    while ((entry = readdir(dir)) != NULL && data->file_count < mp3_count) {
        if (entry->d_type == DT_REG && is_mp3_file(entry->d_name)) {
            mp3_file_info_t* file_info = &data->files[data->file_count];
            
            // 构建完整文件路径
            snprintf(file_info->filename, sizeof(file_info->filename), 
                     "%s/%s", mount_point, entry->d_name);
            
            // 提取标题
            extract_title_from_filename(entry->d_name, file_info->title, sizeof(file_info->title));
            
            // 获取文件大小
            struct stat file_stat;
            if (stat(file_info->filename, &file_stat) == 0) {
                file_info->file_size = file_stat.st_size;
            } else {
                file_info->file_size = 0;
            }
            
            // 暂时设置为空或默认值
            strcpy(file_info->artist, "Unknown Artist");
            strcpy(file_info->album, "Unknown Album");
            file_info->duration = 0;
            
            data->file_count++;
        }
    }
    
    closedir(dir);
    data->is_scanning = false;
    data->current_index = 0;
    
    printf("Found %lu MP3 files\n", (unsigned long)data->file_count);
    return data->file_count;
}

void free_mp3_files(music_player_data_t* data) {
    if (!data) return;
    
    if (data->files) {
        free(data->files);
        data->files = NULL;
    }
    
    data->file_count = 0;
    data->current_index = 0;
}



static void file_list_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
        
        if (index < g_music_data.file_count) {
            g_music_data.current_index = index;
            
            // 显示选中的文件信息
            mp3_file_info_t* file = &g_music_data.files[index];
            printf("Selected: %s\n", file->title);
            
            // 这里可以添加播放逻辑（暂时不实现）
            
            // 创建反馈提示
            lv_obj_t* feedback = lv_label_create(lv_screen_active());
            lv_label_set_text_fmt(feedback, "Selected: %s", file->title);
            lv_obj_set_style_text_color(feedback, lv_color_hex(0x00AA00), 0);
            lv_obj_set_style_text_font(feedback, &lv_font_montserrat_18, 0);
            lv_obj_align(feedback, LV_ALIGN_TOP_MID, 0, 20);
            
            // 2秒后自动删除提示
            lv_obj_delete_delayed(feedback, 2000);
        }
    }
}

// 播放控制按钮事件处理器
static void play_pause_button_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        switch (g_music_data.play_state) {
            case PLAY_STATE_STOPPED:
                if (g_music_data.files && g_music_data.file_count > 0) {
                    play_current_music(&g_music_data);
                }
                break;
            case PLAY_STATE_PLAYING:
                pause_music(&g_music_data);
                break;
            case PLAY_STATE_PAUSED:
                resume_music(&g_music_data);
                break;
            case PLAY_STATE_LOADING:
                // 加载中，忽略点击
                break;
        }
    }
}

static void prev_button_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        play_previous_music(&g_music_data);
    }
}

static void next_button_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        play_next_music(&g_music_data);
    }
}

static void refresh_file_list(lv_obj_t* list) {
    if (!list) return;
    
    // 清空现有列表
    lv_obj_clean(list);
    
    if (!g_music_data.sd_card_mounted) {
        // SD卡未挂载
        lv_obj_t* item = lv_list_add_text(list, "SD Card not mounted");
        lv_obj_set_style_text_color(item, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_text_font(item, &lv_font_montserrat_18, 0);
        return;
    }
    
    if (g_music_data.file_count == 0) {
        // 没有找到MP3文件
        lv_obj_t* item = lv_list_add_text(list, "No MP3 files found");
        lv_obj_set_style_text_color(item, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(item, &lv_font_montserrat_18, 0);
        return;
    }
    
    // 添加MP3文件到列表
    for (uint32_t i = 0; i < g_music_data.file_count; i++) {
        mp3_file_info_t* file = &g_music_data.files[i];
        
        // 创建列表项
        lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_AUDIO, file->title);
        lv_obj_set_height(btn, LIST_ITEM_HEIGHT);
        
        // 设置字体和颜色
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x333333), 0);
        
        // 创建文件大小标签
        if (file->file_size > 0) {
            lv_obj_t* size_label = lv_label_create(btn);
            lv_label_set_text_fmt(size_label, "%.1f MB", file->file_size / 1024.0 / 1024.0);
            lv_obj_set_style_text_font(size_label, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(size_label, lv_color_hex(0x666666), 0);
            lv_obj_align(size_label, LV_ALIGN_RIGHT_MID, -20, 0);
        }
        
        // 将索引直接转换为指针传递（避免内存分配）
        lv_obj_add_event_cb(btn, file_list_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }
}

// 音乐播放器应用创建
static void music_player_app_create(app_t* app) {
    if (!app || !app->container) {
        return;
    }
    
    // 设置应用背景
    lv_obj_set_style_bg_color(app->container, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(app->container, LV_OPA_COVER, 0);
    
    // 创建标题
    lv_obj_t* title = lv_label_create(app->container);
    lv_label_set_text(title, "Music Player");
    lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
    
    // 创建当前歌曲信息区域
    lv_obj_t* song_info_panel = lv_obj_create(app->container);
    lv_obj_set_size(song_info_panel, SCREEN_WIDTH - 40, 120);
    lv_obj_align(song_info_panel, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_color(song_info_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(song_info_panel, 2, 0);
    lv_obj_set_style_border_color(song_info_panel, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_radius(song_info_panel, 10, 0);
    lv_obj_set_style_pad_all(song_info_panel, 20, 0);
    
    // 当前歌曲标签
    g_current_song_label = lv_label_create(song_info_panel);
    lv_label_set_text(g_current_song_label, "No song selected");
    lv_obj_set_style_text_color(g_current_song_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(g_current_song_label, &lv_font_montserrat_20, 0);
    lv_obj_align(g_current_song_label, LV_ALIGN_TOP_MID, 0, 0);
    
    // 进度条
    g_progress_bar = lv_bar_create(song_info_panel);
    lv_obj_set_size(g_progress_bar, SCREEN_WIDTH - 80, 8);
    lv_obj_align(g_progress_bar, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_progress_bar, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_color(g_progress_bar, lv_color_hex(0x2196F3), LV_PART_INDICATOR);
    lv_bar_set_value(g_progress_bar, 0, LV_ANIM_OFF);
    
    // 时间标签
    g_time_label = lv_label_create(song_info_panel);
    lv_label_set_text(g_time_label, "00:00 / 00:00");
    lv_obj_set_style_text_color(g_time_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_16, 0);
    lv_obj_align(g_time_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // 创建播放控制按钮区域
    lv_obj_t* control_panel = lv_obj_create(app->container);
    lv_obj_set_size(control_panel, SCREEN_WIDTH - 40, 100);
    lv_obj_align(control_panel, LV_ALIGN_TOP_MID, 0, 210);
    lv_obj_set_style_bg_color(control_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(control_panel, 2, 0);
    lv_obj_set_style_border_color(control_panel, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_radius(control_panel, 10, 0);
    lv_obj_set_style_pad_all(control_panel, 20, 0);
    
    // 上一曲按钮
    g_prev_btn = lv_btn_create(control_panel);
    lv_obj_set_size(g_prev_btn, 80, 60);
    lv_obj_align(g_prev_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(g_prev_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_radius(g_prev_btn, 15, 0);
    lv_obj_add_event_cb(g_prev_btn, prev_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* prev_label = lv_label_create(g_prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_24, 0);
    lv_obj_center(prev_label);
    
    // 播放/暂停按钮
    g_play_pause_btn = lv_btn_create(control_panel);
    lv_obj_set_size(g_play_pause_btn, 80, 60);
    lv_obj_align(g_play_pause_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_play_pause_btn, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_radius(g_play_pause_btn, 15, 0);
    lv_obj_add_event_cb(g_play_pause_btn, play_pause_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* play_label = lv_label_create(g_play_pause_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(play_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(play_label, &lv_font_montserrat_24, 0);
    lv_obj_center(play_label);
    
    // 下一曲按钮
    g_next_btn = lv_btn_create(control_panel);
    lv_obj_set_size(g_next_btn, 80, 60);
    lv_obj_align(g_next_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(g_next_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_radius(g_next_btn, 15, 0);
    lv_obj_add_event_cb(g_next_btn, next_button_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* next_label = lv_label_create(g_next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(next_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_24, 0);
    lv_obj_center(next_label);
    
    // 创建文件列表
    lv_obj_t* list = lv_list_create(app->container);
    lv_obj_set_size(list, SCREEN_WIDTH - 40, SCREEN_HEIGHT - 360);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 330);
    lv_obj_set_style_bg_color(list, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(list, 2, 0);
    lv_obj_set_style_border_color(list, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_radius(list, 10, 0);
    
    // 初始化音乐数据
    g_music_data.play_state = PLAY_STATE_STOPPED;
    g_music_data.play_position = 0;
    g_music_data.play_duration = 0;
    g_music_data.repeat_mode = false;
    g_music_data.shuffle_mode = false;
    
    // 保存UI元素到用户数据
    app->user_data = list;
    
    // 自动扫描一次
    scan_mp3_files(&g_music_data);
    refresh_file_list(list);
    
    // 初始化UI状态
    update_playback_ui(app->container, &g_music_data);
    
    // 创建定时器定期更新播放进度
    lv_timer_create(ui_update_timer_cb, 1000, NULL);  // 每秒更新一次
}

// 音乐播放器应用销毁
static void music_player_app_destroy(app_t* app) {
    // 停止播放
    stop_music(&g_music_data);
    
    // 释放MP3文件列表
    free_mp3_files(&g_music_data);
    
    // 清空全局UI指针
    g_play_pause_btn = NULL;
    g_prev_btn = NULL;
    g_next_btn = NULL;
    g_current_song_label = NULL;
    g_progress_bar = NULL;
    g_time_label = NULL;
    
    // 清空用户数据
    if (app) {
        app->user_data = NULL;
    }
    
    // 注意：动态分配的索引内存会在列表销毁时自动清理
    // 因为LVGL会在对象销毁时清理关联的事件回调
}

// 播放控制函数实现
bool play_current_music(music_player_data_t* data) {
    if (!data || !data->files || data->current_index >= data->file_count) {
        return false;
    }
    
    mp3_file_info_t* current_file = &data->files[data->current_index];
    printf("Playing MP3: %s\n", current_file->title);
    
    // 设置加载状态
    data->play_state = PLAY_STATE_LOADING;
    update_playback_ui(NULL, data);
    
    // 播放真实的MP3文件
    bool success = hal_audio_play_mp3_file(current_file->filename);
    
    if (success) {
        data->play_state = PLAY_STATE_PLAYING;
        data->play_position = 0;
        // 尝试获取时长，如果不可用则设为未知
        data->play_duration = hal_audio_get_mp3_duration();
        if (data->play_duration == 0) {
            // 如果无法获取确切时长，设置一个估计值（基于文件大小）
            // 平均比特率 128kbps，计算大概时长
            data->play_duration = (current_file->file_size * 8) / (128 * 1000);
        }
        printf("MP3 playback started: %s (estimated duration: %lu sec)\n", 
               current_file->title, (unsigned long)data->play_duration);
    } else {
        data->play_state = PLAY_STATE_STOPPED;
        printf("Failed to start MP3 playback: %s\n", current_file->title);
    }
    
    update_playback_ui(NULL, data);
    return success;
}

void pause_music(music_player_data_t* data) {
    if (!data) return;
    
    if (data->play_state == PLAY_STATE_PLAYING) {
        hal_audio_stop_mp3();
        data->play_state = PLAY_STATE_PAUSED;
        update_playback_ui(NULL, data);
        printf("MP3 music paused\n");
    }
}

void resume_music(music_player_data_t* data) {
    if (!data) return;
    
    if (data->play_state == PLAY_STATE_PAUSED) {
        // 重新开始播放当前音乐
        play_current_music(data);
        printf("Music resumed\n");
    }
}

void stop_music(music_player_data_t* data) {
    if (!data) return;
    
    hal_audio_stop_mp3();
    data->play_state = PLAY_STATE_STOPPED;
    data->play_position = 0;
    update_playback_ui(NULL, data);
    printf("MP3 music stopped\n");
}

void play_next_music(music_player_data_t* data) {
    if (!data || !data->files || data->file_count == 0) return;
    
    // 停止当前播放
    stop_music(data);
    
    // 切换到下一首
    if (data->shuffle_mode) {
        // 随机播放
        data->current_index = rand() % data->file_count;
    } else {
        // 顺序播放
        data->current_index = (data->current_index + 1) % data->file_count;
    }
    
    // 播放新的音乐
    play_current_music(data);
    printf("Playing next: %s\n", data->files[data->current_index].title);
}

void play_previous_music(music_player_data_t* data) {
    if (!data || !data->files || data->file_count == 0) return;
    
    // 停止当前播放
    stop_music(data);
    
    // 切换到上一首
    if (data->shuffle_mode) {
        // 随机播放
        data->current_index = rand() % data->file_count;
    } else {
        // 顺序播放
        data->current_index = (data->current_index + data->file_count - 1) % data->file_count;
    }
    
    // 播放新的音乐
    play_current_music(data);
    printf("Playing previous: %s\n", data->files[data->current_index].title);
}

void update_playback_ui(lv_obj_t* container, music_player_data_t* data) {
    if (!data) return;
    
    // 更新播放位置（如果正在播放MP3）
    if (data->play_state == PLAY_STATE_PLAYING) {
        data->play_position = hal_audio_get_mp3_position();
        
        // 检查播放是否自然结束
        if (!hal_audio_is_mp3_playing() && data->play_state == PLAY_STATE_PLAYING) {
            data->play_state = PLAY_STATE_STOPPED;
            data->play_position = 0;
            printf("MP3 playback finished naturally\n");
        }
    }
    
    // 更新播放/暂停按钮
    if (g_play_pause_btn) {
        lv_obj_t* btn_label = lv_obj_get_child(g_play_pause_btn, 0);
        if (btn_label) {
            switch (data->play_state) {
                case PLAY_STATE_PLAYING:
                    lv_label_set_text(btn_label, LV_SYMBOL_PAUSE);
                    break;
                case PLAY_STATE_PAUSED:
                case PLAY_STATE_STOPPED:
                    lv_label_set_text(btn_label, LV_SYMBOL_PLAY);
                    break;
                case PLAY_STATE_LOADING:
                    lv_label_set_text(btn_label, LV_SYMBOL_REFRESH);
                    break;
            }
        }
    }
    
    // 更新当前歌曲信息
    if (g_current_song_label && data->files && data->current_index < data->file_count) {
        lv_label_set_text(g_current_song_label, data->files[data->current_index].title);
    }
    
    // 更新进度条
    if (g_progress_bar && data->play_duration > 0) {
        int32_t progress = (int32_t)((data->play_position * 100) / data->play_duration);
        lv_bar_set_value(g_progress_bar, progress, LV_ANIM_ON);
    }
    
    // 更新时间标签
    if (g_time_label) {
        lv_label_set_text_fmt(g_time_label, "%02lu:%02lu / %02lu:%02lu",
                              (unsigned long)(data->play_position / 60),
                              (unsigned long)(data->play_position % 60),
                              (unsigned long)(data->play_duration / 60),
                              (unsigned long)(data->play_duration % 60));
    }
}

// UI更新定时器回调
static void ui_update_timer_cb(lv_timer_t* timer) {
    (void)timer; // 避免未使用参数警告
    update_playback_ui(NULL, &g_music_data);
}

// 注册音乐播放器应用
void register_music_player_app(void) {
    app_manager_register_app("Music Player", LV_SYMBOL_AUDIO, 
                             music_player_app_create, music_player_app_destroy);
} 