#ifndef SETTINGS_DISPLAY_H
#define SETTINGS_DISPLAY_H

#include "settings_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建显示设置页面
 * 
 * @param menu 菜单对象
 * @return lv_obj_t* 创建的页面对象
 */
lv_obj_t* create_display_page(lv_obj_t* menu);

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_DISPLAY_H