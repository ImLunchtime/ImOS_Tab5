#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 安全的内存分配函数
 * 优先使用PSRAM，如果不够则使用常规内存
 * 
 * @param size 要分配的内存大小
 * @return void* 分配的内存指针，失败时返回NULL
 */
void* safe_malloc(size_t size);

/**
 * @brief 安全的内存释放函数
 * 
 * @param ptr 要释放的内存指针
 */
void safe_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_UTILS_H