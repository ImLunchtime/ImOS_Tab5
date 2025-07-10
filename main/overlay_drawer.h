#pragma once

#include "app_manager.h"

// 抽屉控制函数
void app_drawer_open(void);
void app_drawer_close(void);
void app_drawer_toggle(void);

// 内存管理函数
void app_drawer_cleanup_list(void);

// 注册应用抽屉Overlay
void register_drawer_overlay(void); 