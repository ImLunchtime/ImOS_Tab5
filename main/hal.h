#ifndef HAL_H
#define HAL_H

#include <bsp/esp-bsp.h>
#include "lvgl.h"

// Display and input device handles
extern lv_disp_t *lvDisp;
extern lv_indev_t *lvTouchpad;

// HAL initialization functions
void hal_init(void);
void hal_touchpad_init(void);

#endif // HAL_H 