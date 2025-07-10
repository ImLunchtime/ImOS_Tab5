#pragma once

#include "lvgl.h"
#include <stdbool.h>

// 手势处理初始化和销毁
void gesture_handler_init(void);
void gesture_handler_deinit(void);

// 启用/禁用手势处理
void gesture_handler_set_enabled(bool enabled);
bool gesture_handler_is_enabled(void); 