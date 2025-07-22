#ifndef HAL_DISPLAY_H
#define HAL_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Display and input device handles
extern lv_disp_t *lvDisp;
extern lv_indev_t *lvTouchpad;

/**
 * @brief Initialize display subsystem
 * 
 * This function initializes the display, backlight, and touchpad
 */
void hal_display_init(void);

/**
 * @brief Initialize touchpad input device
 * 
 * This function creates and configures the LVGL touchpad input device
 */
void hal_touchpad_init(void);

/**
 * @brief Set display brightness
 * 
 * @param brightness Brightness level (0-100)
 */
void hal_set_display_brightness(uint8_t brightness);

/**
 * @brief Get current display brightness
 * 
 * @return Current brightness level (0-100)
 */
uint8_t hal_get_display_brightness(void);

/**
 * @brief Turn display backlight on
 */
void hal_display_backlight_on(void);

/**
 * @brief Turn display backlight off
 */
void hal_display_backlight_off(void);

/**
 * @brief Check if display is on
 * 
 * @return true if display is on
 */
bool hal_display_is_on(void);

/**
 * @brief Set display rotation
 * 
 * @param rotation Display rotation (0, 90, 180, 270 degrees)
 */
void hal_display_set_rotation(lv_disp_rotation_t rotation);

/**
 * @brief Get display resolution
 * 
 * @param width Pointer to store width
 * @param height Pointer to store height
 */
void hal_display_get_resolution(uint32_t *width, uint32_t *height);

#ifdef __cplusplus
}
#endif

#endif // HAL_DISPLAY_H 