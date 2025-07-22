#ifndef SETTINGS_SOUND_H
#define SETTINGS_SOUND_H

#include "settings_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建声音设置页面
 * 
 * @param menu 菜单对象
 * @return lv_obj_t* 创建的页面对象
 */
lv_obj_t* create_sound_page(lv_obj_t* menu);

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_SOUND_H