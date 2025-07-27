#pragma once

#include "overlay_drawer_types.h"

// 内存管理函数
void drawer_memory_deep_clean(drawer_state_t* state);
bool drawer_memory_should_idle_cleanup(drawer_state_t* state);
void drawer_memory_cleanup_list(void);
void drawer_memory_check_cleanup(void);