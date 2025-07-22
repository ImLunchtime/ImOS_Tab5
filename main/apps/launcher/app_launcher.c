#include "apps/launcher/app_launcher.h"
#include "managers/app_manager.h"
#include "overlay_drawer.h"
#include <stdio.h>
//#include "assets/bg.c"

// 声明自定义字体
LV_FONT_DECLARE(simhei_32);

// 外部声明背景图片 - 来自assets/bg.c
//extern const lv_image_dsc_t bg;
//LV_FONT_DECLARE(simhei_32);

// 启动器应用创建
static void launcher_app_create(app_t* app) {
    if (!app || !app->container) {
        return;
    }
    
    // // 设置透明背景，让背景图片显示出来
    // lv_obj_set_style_bg_opa(app->container, LV_OPA_TRANSP, 0);
    
    // // 创建背景图片对象
    // lv_obj_t* bg_img = lv_image_create(app->container);
    // lv_image_set_src(bg_img, &bg);
    
    // // 将图片放大到两倍大小并居中 - 使用正确的缩放API
    // lv_image_set_scale(bg_img, 512);  // 512 = 2.0x scale (256 is 1.0x)
    // lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);
    
    // // 确保图片不会拦截事件，让手势处理器能正常工作
    // lv_obj_add_flag(bg_img, LV_OBJ_FLAG_EVENT_BUBBLE);
    // lv_obj_clear_flag(bg_img, LV_OBJ_FLAG_CLICKABLE);

    // 创建欢迎文本标签
    lv_obj_t* welcome_label = lv_label_create(app->container);
    lv_label_set_text(welcome_label, "欢迎使用");
    
    // 设置自定义字体
    lv_obj_set_style_text_font(welcome_label, &simhei_32, 0);
    
    // 设置文本颜色为白色
    lv_obj_set_style_text_color(welcome_label, lv_color_black(), 0);
    
    // 居中显示
    lv_obj_align(welcome_label, LV_ALIGN_CENTER, 0, 0);
    
    // 确保标签不会拦截事件
    lv_obj_add_flag(welcome_label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(welcome_label, LV_OBJ_FLAG_CLICKABLE);

    app_drawer_open();
}

// 启动器应用销毁
static void launcher_app_destroy(app_t* app) {
    // 关闭应用抽屉
    app_drawer_close();
}

// 注册启动器应用
void register_launcher_app(void) {
    app_manager_register_app("启动器", LV_SYMBOL_HOME, 
                             launcher_app_create, launcher_app_destroy);
} 