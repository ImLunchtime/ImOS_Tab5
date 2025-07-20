#include "lv_demos.h"
#include <stdio.h>
#include "gui.h"
#include "hal.h"
#include "project_defs.h"

void app_main(void) {
    // Initialize hardware
    hal_init();

    // Initialize system
    gui_init(lvDisp);

    bsp_display_unlock();
}
