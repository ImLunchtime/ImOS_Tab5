#include "memory_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"

// 安全的内存分配函数
void* safe_malloc(size_t size) {
    void* ptr = NULL;
    
    // 首先尝试使用PSRAM
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >= size) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        if (ptr) {
            printf("Allocated %zu bytes from PSRAM\n", size);
            return ptr;
        }
    }
    
    // 如果PSRAM不够，使用常规内存
    ptr = malloc(size);
    if (ptr) {
        printf("Allocated %zu bytes from regular heap\n", size);
    } else {
        printf("Failed to allocate %zu bytes\n", size);
    }
    
    return ptr;
}

// 安全的内存释放函数
void safe_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}