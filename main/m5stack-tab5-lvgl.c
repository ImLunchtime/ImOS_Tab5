#include "lv_demos.h"
#include <stdio.h>
#include "gui.h"
#include "hal.h"

void app_main(void) {
    // Initialize hardware
    hal_init();
    
    // Initialize touchpad
    // hal_touchpad_init();

    // Initialize our GUI
    gui_init(lvDisp);

    bsp_display_unlock();
}
