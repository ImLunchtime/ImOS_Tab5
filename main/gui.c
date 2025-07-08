#include "gui.h"

static void btn_event_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Toggle button state
        bool current_state = lv_obj_has_state(btn, LV_STATE_CHECKED);
        if(current_state) {
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
        } else {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        }
    }
}

void gui_init(lv_disp_t *disp) 
{
    // Get the screen size
    lv_coord_t screen_width = lv_display_get_horizontal_resolution(disp);
    lv_coord_t screen_height = lv_display_get_vertical_resolution(disp);

    // Create a button in the center
    lv_obj_t *btn = lv_btn_create(lv_scr_act());
    
    // Set button size (100x50 pixels)
    lv_obj_set_size(btn, 100, 50);
    
    // Center the button
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    
    // Add a label to the button
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Press Me!");
    lv_obj_center(label);

    // Make the button toggleable
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    
    // Add click event handler
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
} 