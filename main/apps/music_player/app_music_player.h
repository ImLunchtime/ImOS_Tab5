#pragma once

#include "managers/app_manager.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// MP3文件信息结构体
typedef struct {
    char filename[256];     // 文件名
    char title[128];        // 音乐标题（从文件名提取）
    char artist[128];       // 艺术家（暂时为空）
    char album[128];        // 专辑（暂时为空）
    uint32_t duration;      // 时长（秒，暂时为0）
    uint32_t file_size;     // 文件大小（字节）
} mp3_file_info_t;

// 播放状态枚举
typedef enum {
    PLAY_STATE_STOPPED,     // 停止
    PLAY_STATE_PLAYING,     // 播放中
    PLAY_STATE_PAUSED,      // 暂停
    PLAY_STATE_LOADING      // 加载中
} play_state_t;

// 音乐播放器数据结构
typedef struct {
    mp3_file_info_t* files;     // MP3文件列表
    uint32_t file_count;        // 文件数量
    uint32_t current_index;     // 当前选中的文件索引
    bool is_scanning;           // 是否正在扫描
    bool sd_card_mounted;       // SD卡是否挂载
    play_state_t play_state;    // 播放状态
    uint32_t play_position;     // 播放位置（秒）
    uint32_t play_duration;     // 播放时长（秒）
    bool repeat_mode;           // 重复播放模式
    bool shuffle_mode;          // 随机播放模式
} music_player_data_t;

/**
 * @brief 注册音乐播放器应用
 */
void register_music_player_app(void);

/**
 * @brief 扫描SD卡中的MP3文件
 * 
 * @param data 音乐播放器数据指针
 * @return 扫描到的MP3文件数量
 */
uint32_t scan_mp3_files(music_player_data_t* data);

/**
 * @brief 释放MP3文件列表内存
 * 
 * @param data 音乐播放器数据指针
 */
void free_mp3_files(music_player_data_t* data);

/**
 * @brief 检查文件是否为MP3格式
 * 
 * @param filename 文件名
 * @return true if MP3 file, false otherwise
 */
bool is_mp3_file(const char* filename);

/**
 * @brief 从文件名提取音乐标题
 * 
 * @param filename 文件名
 * @param title 输出标题缓冲区
 * @param title_size 缓冲区大小
 */
void extract_title_from_filename(const char* filename, char* title, size_t title_size);

/**
 * @brief 播放当前选中的音乐文件
 * 
 * @param data 音乐播放器数据指针
 * @return true if playback started successfully
 */
bool play_current_music(music_player_data_t* data);

/**
 * @brief 暂停播放
 * 
 * @param data 音乐播放器数据指针
 */
void pause_music(music_player_data_t* data);

/**
 * @brief 恢复播放
 * 
 * @param data 音乐播放器数据指针
 */
void resume_music(music_player_data_t* data);

/**
 * @brief 停止播放
 * 
 * @param data 音乐播放器数据指针
 */
void stop_music(music_player_data_t* data);

/**
 * @brief 播放下一首
 * 
 * @param data 音乐播放器数据指针
 */
void play_next_music(music_player_data_t* data);

/**
 * @brief 播放上一首
 * 
 * @param data 音乐播放器数据指针
 */
void play_previous_music(music_player_data_t* data);

/**
 * @brief 更新播放界面
 * 
 * @param container 应用容器
 * @param data 音乐播放器数据指针
 */
void update_playback_ui(lv_obj_t* container, music_player_data_t* data);

#ifdef __cplusplus
}
#endif 