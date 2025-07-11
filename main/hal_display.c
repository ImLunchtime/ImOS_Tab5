#include "hal_display.h"
#include <bsp/esp-bsp.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 需要访问BSP的显示配置常量
#define BSP_LCD_DRAW_BUFF_SIZE   (BSP_LCD_H_RES * 50)
#define BSP_LCD_DRAW_BUFF_DOUBLE (0)

// Display global variables
lv_disp_t *lvDisp = NULL;
lv_indev_t *lvTouchpad = NULL;

// Display state
static struct {
    bool is_initialized;
    bool is_backlight_on;
    uint8_t current_brightness;
} g_display_state = {
    .is_initialized = false,
    .is_backlight_on = false,
    .current_brightness = 100  // Default brightness 100%
};

// External LCD touch handle
extern esp_lcd_touch_handle_t _lcd_touch_handle;

// Helper function to clamp values
static uint8_t clamp_uint8(uint8_t value, uint8_t min_val, uint8_t max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

// LVGL touchpad read callback
static void lvgl_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (_lcd_touch_handle == NULL) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    
    uint16_t touch_x[1];
    uint16_t touch_y[1];
    uint16_t touch_strength[1];
    uint8_t touch_cnt = 0;

    esp_lcd_touch_read_data(_lcd_touch_handle);
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(
        _lcd_touch_handle, 
        touch_x, 
        touch_y, 
        touch_strength, 
        &touch_cnt, 
        1
    );

    if (!touchpad_pressed) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touch_x[0];
        data->point.y = touch_y[0];
    }
}

void hal_display_init(void)
{
    if (g_display_state.is_initialized) {
        printf("Display already initialized\n");
        return;
    }

    // Reset touchpad
    bsp_reset_tp();

    // 使用自定义配置启动显示
    bsp_display_cfg_t display_cfg = {
        .lvgl_port_cfg = {
            .task_priority = 4,
            .task_stack = 16384,  // 增加LVGL任务栈大小
            .task_affinity = -1,
            .task_max_sleep_ms = 500,
            .timer_period_ms = 5,
        },
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,  // 使用PSRAM缓冲区
            .sw_rotate = true,
        }
    };

    // Initialize display with custom config
    lvDisp = bsp_display_start_with_config(&display_cfg);
    if (!lvDisp) {
        printf("Failed to start display\n");
        return;
    }

    // Set display rotation to landscape
    lv_display_set_rotation(lvDisp, LV_DISPLAY_ROTATION_90);

    // Turn on backlight
    bsp_display_backlight_on();
    g_display_state.is_backlight_on = true;

    // Set initial brightness
    hal_set_display_brightness(g_display_state.current_brightness);

    g_display_state.is_initialized = true;
    printf("Display HAL initialized successfully\n");
}

void hal_touchpad_init(void)
{
    if (!g_display_state.is_initialized) {
        printf("Display not initialized, cannot initialize touchpad\n");
        return;
    }

    if (lvTouchpad) {
        printf("Touchpad already initialized\n");
        return;
    }

    // Create and configure touchpad input device
    lvTouchpad = lv_indev_create();
    if (!lvTouchpad) {
        printf("Failed to create touchpad input device\n");
        return;
    }

    lv_indev_set_type(lvTouchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lvTouchpad, lvgl_read_cb);
    lv_indev_set_display(lvTouchpad, lvDisp);

    printf("Touchpad initialized successfully\n");
}

void hal_set_display_brightness(uint8_t brightness)
{
    g_display_state.current_brightness = clamp_uint8(brightness, 0, 100);
    
    // Set hardware brightness
    bsp_display_brightness_set(g_display_state.current_brightness);
    
    printf("Set display brightness: %d%%\n", g_display_state.current_brightness);
}

uint8_t hal_get_display_brightness(void)
{
    return g_display_state.current_brightness;
}

void hal_display_backlight_on(void)
{
    if (!g_display_state.is_initialized) {
        printf("Display not initialized\n");
        return;
    }

    bsp_display_backlight_on();
    g_display_state.is_backlight_on = true;
    printf("Display backlight turned on\n");
}

void hal_display_backlight_off(void)
{
    if (!g_display_state.is_initialized) {
        printf("Display not initialized\n");
        return;
    }

    bsp_display_backlight_off();
    g_display_state.is_backlight_on = false;
    printf("Display backlight turned off\n");
}

bool hal_display_is_on(void)
{
    return g_display_state.is_backlight_on;
}

void hal_display_set_rotation(lv_disp_rotation_t rotation)
{
    if (!g_display_state.is_initialized || !lvDisp) {
        printf("Display not initialized\n");
        return;
    }

    lv_display_set_rotation(lvDisp, rotation);
    printf("Display rotation set to: %d\n", (int)rotation);
}

void hal_display_get_resolution(uint32_t *width, uint32_t *height)
{
    if (!width || !height) {
        printf("Invalid parameters for get resolution\n");
        return;
    }

    if (!g_display_state.is_initialized || !lvDisp) {
        printf("Display not initialized\n");
        *width = 0;
        *height = 0;
        return;
    }

    *width = lv_display_get_horizontal_resolution(lvDisp);
    *height = lv_display_get_vertical_resolution(lvDisp);
    
    printf("Display resolution: %dx%d\n", (int)*width, (int)*height);
} 