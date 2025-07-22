#include "hal_sdcard.h"
#include <bsp/esp-bsp.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

// SD card mount point
#define SD_MOUNT_POINT "/sdcard"
#define SD_MAX_FILES 10

// SD card state
typedef struct {
    bool is_mounted;
    SemaphoreHandle_t mutex;
    char mount_point[32];
} sdcard_state_t;

// Global SD card state
static sdcard_state_t g_sdcard_state = {
    .is_mounted = false,
    .mutex = NULL,
    .mount_point = SD_MOUNT_POINT
};

bool hal_sdcard_init(void)
{
    // Create mutex if not already created
    if (g_sdcard_state.mutex == NULL) {
        g_sdcard_state.mutex = xSemaphoreCreateMutex();
        if (g_sdcard_state.mutex == NULL) {
            printf("Failed to create SD card mutex\n");
            return false;
        }
    }

    if (xSemaphoreTake(g_sdcard_state.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Check if already mounted
        if (g_sdcard_state.is_mounted) {
            printf("SD card already mounted\n");
            xSemaphoreGive(g_sdcard_state.mutex);
            return true;
        }

        printf("Mounting SD card...\n");
        
        // Initialize SD card using BSP function
        esp_err_t ret = bsp_sdcard_init(g_sdcard_state.mount_point, SD_MAX_FILES);
        if (ret == ESP_OK) {
            g_sdcard_state.is_mounted = true;
            printf("SD card mounted successfully at %s\n", g_sdcard_state.mount_point);
            xSemaphoreGive(g_sdcard_state.mutex);
            return true;
        } else {
            printf("Failed to mount SD card: %s\n", esp_err_to_name(ret));
            g_sdcard_state.is_mounted = false;
            xSemaphoreGive(g_sdcard_state.mutex);
            return false;
        }
    } else {
        printf("Failed to acquire SD card mutex\n");
        return false;
    }
}

void hal_sdcard_deinit(void)
{
    if (g_sdcard_state.mutex == NULL) {
        return;
    }

    if (xSemaphoreTake(g_sdcard_state.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (g_sdcard_state.is_mounted) {
            printf("Unmounting SD card...\n");
            
            // Deinitialize SD card using BSP function
            esp_err_t ret = bsp_sdcard_deinit(g_sdcard_state.mount_point);
            if (ret == ESP_OK) {
                printf("SD card unmounted successfully\n");
            } else {
                printf("Failed to unmount SD card: %s\n", esp_err_to_name(ret));
            }
            
            g_sdcard_state.is_mounted = false;
        }
        
        xSemaphoreGive(g_sdcard_state.mutex);
    }
}

bool hal_sdcard_is_mounted(void)
{
    if (g_sdcard_state.mutex == NULL) {
        return false;
    }

    bool mounted = false;
    if (xSemaphoreTake(g_sdcard_state.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        mounted = g_sdcard_state.is_mounted;
        xSemaphoreGive(g_sdcard_state.mutex);
    }
    
    return mounted;
}

const char* hal_sdcard_get_mount_point(void)
{
    return g_sdcard_state.mount_point;
} 