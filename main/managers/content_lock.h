#pragma once

#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// 内容锁API - 简化的接口
bool content_lock_is_unlocked(void);
esp_err_t content_lock_toggle(void);
esp_err_t content_lock_set_state(bool unlocked);

#ifdef __cplusplus
}
#endif