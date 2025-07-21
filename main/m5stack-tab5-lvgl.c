#include "lv_demos.h"
#include <stdio.h>
#include "gui.h"
#include "hal.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void app_main(void) {
    printf("Starting M5Stack Tab5 application...\n");
    
    // Initialize hardware
    printf("Initializing HAL...\n");
    hal_init();
    
    // 等待HAL完全初始化，特别是LVGL显示系统
    printf("Waiting for HAL stabilization...\n");
    vTaskDelay(pdMS_TO_TICKS(500));  // 等待500ms让LVGL完全初始化
    
    // 验证显示系统是否正确初始化
    if (!lvDisp) {
        printf("ERROR: Display not initialized properly, retrying...\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!lvDisp) {
            printf("FATAL: Display initialization failed\n");
            return;
        }
    }
    
    // // 验证LVGL是否准备就绪
    // printf("Verifying LVGL readiness...\n");
    // if (!lv_display_is_valid(lvDisp)) {
    //     printf("ERROR: LVGL display is not valid\n");
    //     return;
    // }
    
    // // 额外的稳定性检查
    // lv_coord_t width = lv_display_get_horizontal_resolution(lvDisp);
    // lv_coord_t height = lv_display_get_vertical_resolution(lvDisp);
    // if (width <= 0 || height <= 0) {
    //     printf("ERROR: Invalid display resolution: %dx%d\n", width, height);
    //     return;
    // }
    
    // printf("Display verified: %dx%d\n", width, height);
    
    // 确保LVGL任务已经启动并稳定
    // printf("Allowing LVGL task to stabilize...\n");
    // vTaskDelay(pdMS_TO_TICKS(200));
    
    // Initialize system GUI
    printf("Initializing GUI system...\n");
    gui_init(lvDisp);
    
    // 等待GUI初始化完成
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 解锁显示
    printf("Unlocking display...\n");
    bsp_display_unlock();
    
    printf("M5Stack Tab5 application started successfully\n");
}
