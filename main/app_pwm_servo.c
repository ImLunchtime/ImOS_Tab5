#include "app_pwm_servo.h"
#include "menu_utils.h"
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// 外部字体声明
LV_FONT_DECLARE(simhei_32);

// 引脚选项定义
const pin_option_t g_pin_options[] = {
    {"G0", PWM_PIN_G0, PWM_CHANNEL_G0},
    {"G1", PWM_PIN_G1, PWM_CHANNEL_G1}
};

const size_t g_pin_options_count = sizeof(g_pin_options) / sizeof(g_pin_options[0]);

// 全局应用数据
static pwm_servo_data_t* g_pwm_servo_data = NULL;

// 前向声明
static void create_main_ui(pwm_servo_data_t* data, lv_obj_t* parent);
static void create_control_panel(pwm_servo_data_t* data, lv_obj_t* parent);
static void create_info_panel(pwm_servo_data_t* data, lv_obj_t* parent);
static void pin_dropdown_event_cb(lv_event_t* e);
static void angle_arc_event_cb(lv_event_t* e);
static void enable_switch_event_cb(lv_event_t* e);
static void reset_button_event_cb(lv_event_t* e);
static void test_button_event_cb(lv_event_t* e);

// 内存管理函数（与其他应用保持一致）
static void* safe_pwm_malloc(size_t size) {
    // 优先使用PSRAM
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ptr) {
        // 回退到普通RAM
        ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    }
    return ptr;
}

static void safe_pwm_free(void* ptr) {
    if (ptr) {
        heap_caps_free(ptr);
    }
}

// 注册PWM舵机应用
void register_pwm_servo_app(void) {
    app_manager_register_app("PWM舵机", LV_SYMBOL_SETTINGS, 
                             pwm_servo_app_create, pwm_servo_app_destroy);
}

// 应用创建函数
void pwm_servo_app_create(app_t* app) {
    if (!app) {
        return;
    }

    printf("Creating PWM servo app\n");

    // 分配应用数据
    g_pwm_servo_data = (pwm_servo_data_t*)safe_pwm_malloc(sizeof(pwm_servo_data_t));
    if (!g_pwm_servo_data) {
        printf("Failed to allocate PWM servo data\n");
        return;
    }

    memset(g_pwm_servo_data, 0, sizeof(pwm_servo_data_t));
    app->user_data = g_pwm_servo_data;

    // 初始化PWM HAL
    esp_err_t ret = hal_pwm_init();
    if (ret != ESP_OK) {
        printf("Failed to initialize PWM HAL: %s\n", esp_err_to_name(ret));
        safe_pwm_free(g_pwm_servo_data);
        g_pwm_servo_data = NULL;
        app->user_data = NULL;
        return;
    }

    // 初始化应用数据
    g_pwm_servo_data->is_initialized = true;
    g_pwm_servo_data->current_channel = PWM_CHANNEL_G0;  // 默认选择G0
    g_pwm_servo_data->current_angle = SERVO_ANGLE_MID;   // 默认中位角度
    g_pwm_servo_data->current_pulse_width = PWM_SERVO_MID_US;
    g_pwm_servo_data->current_duty_cycle = hal_pwm_pulse_width_to_duty(PWM_SERVO_MID_US);
    g_pwm_servo_data->pwm_enabled = false;

    // 配置默认PWM通道
    update_pwm_channel(g_pwm_servo_data, PWM_CHANNEL_G0);

    // 创建用户界面
    create_main_ui(g_pwm_servo_data, app->container);

    // 更新显示
    update_info_display(g_pwm_servo_data);

    printf("PWM servo app created successfully\n");
}

// 应用销毁函数
void pwm_servo_app_destroy(app_t* app) {
    if (!app || !g_pwm_servo_data) {
        return;
    }

    printf("Destroying PWM servo app\n");

    // 停止PWM输出
    if (g_pwm_servo_data->pwm_enabled) {
        set_pwm_enable(g_pwm_servo_data, false);
    }

    // 反初始化PWM HAL
    hal_pwm_deinit();

    // 清理应用数据
    g_pwm_servo_data->is_initialized = false;
    safe_pwm_free(g_pwm_servo_data);
    g_pwm_servo_data = NULL;
    app->user_data = NULL;

    printf("PWM servo app destroyed\n");
}

// 创建主界面
static void create_main_ui(pwm_servo_data_t* data, lv_obj_t* parent) {
    // 主容器
    data->main_container = lv_obj_create(parent);
    lv_obj_set_size(data->main_container, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(data->main_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(data->main_container, 10, 0);

    // 标题
    data->title_label = lv_label_create(data->main_container);
    lv_label_set_text(data->title_label, "PWM舵机测试");
    lv_obj_set_style_text_font(data->title_label, &simhei_32, 0);
    lv_obj_align(data->title_label, LV_ALIGN_TOP_MID, 0, 0);

    // 创建控制面板
    create_control_panel(data, data->main_container);

    // 创建信息面板
    create_info_panel(data, data->main_container);
}

// 创建控制面板
static void create_control_panel(pwm_servo_data_t* data, lv_obj_t* parent) {
    // 控制面板容器
    lv_obj_t* control_panel = lv_obj_create(parent);
    lv_obj_set_size(control_panel, LV_PCT(100), 350);  // 增加高度以容纳更大的Arc
    lv_obj_align(control_panel, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_clear_flag(control_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(control_panel, 15, 0);

    // 引脚选择标签
    lv_obj_t* pin_label = lv_label_create(control_panel);
    lv_label_set_text(pin_label, "数据引脚:");
    lv_obj_set_style_text_font(pin_label, &simhei_32, 0);
    lv_obj_align(pin_label, LV_ALIGN_TOP_LEFT, 0, 0);

    // 引脚选择下拉框
    data->pin_dropdown = lv_dropdown_create(control_panel);
    lv_dropdown_set_options(data->pin_dropdown, "G0\nG1");
    lv_dropdown_set_selected(data->pin_dropdown, 0);  // 默认选择G0
    lv_obj_set_size(data->pin_dropdown, 100, 35);
    lv_obj_align(data->pin_dropdown, LV_ALIGN_TOP_LEFT, 120, -5);  // 调整位置以适应中文字体
    lv_obj_add_event_cb(data->pin_dropdown, pin_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, data);

    // 角度控制标签
    lv_obj_t* angle_control_label = lv_label_create(control_panel);
    lv_label_set_text(angle_control_label, "角度控制:");
    lv_obj_set_style_text_font(angle_control_label, &simhei_32, 0);
    lv_obj_align(angle_control_label, LV_ALIGN_TOP_LEFT, 0, 50);

    // 角度弧形控件
    data->angle_arc = lv_arc_create(control_panel);
    lv_obj_set_size(data->angle_arc, 200, 200);  // 增大Arc尺寸
    lv_obj_align(data->angle_arc, LV_ALIGN_TOP_MID, 0, 80);
    lv_arc_set_range(data->angle_arc, SERVO_ANGLE_MIN, SERVO_ANGLE_MAX);
    lv_arc_set_value(data->angle_arc, SERVO_ANGLE_MID);
    lv_obj_add_event_cb(data->angle_arc, angle_arc_event_cb, LV_EVENT_VALUE_CHANGED, data);

    // 角度显示标签
    data->angle_label = lv_label_create(control_panel);
    lv_label_set_text_fmt(data->angle_label, "%d°", SERVO_ANGLE_MID);
    lv_obj_set_style_text_font(data->angle_label, &simhei_32, 0);
    lv_obj_align(data->angle_label, LV_ALIGN_TOP_MID, 0, 185);  // 调整位置以适应更大的Arc

    // 脉宽显示标签
    data->pulse_width_label = lv_label_create(control_panel);
    lv_label_set_text_fmt(data->pulse_width_label, "脉宽: %dus", PWM_SERVO_MID_US);
    lv_obj_set_style_text_font(data->pulse_width_label, &simhei_32, 0);
    lv_obj_align(data->pulse_width_label, LV_ALIGN_TOP_LEFT, 240, 90);  // 调整位置以适应更大的Arc

    // 占空比显示标签
    data->duty_cycle_label = lv_label_create(control_panel);
    lv_label_set_text_fmt(data->duty_cycle_label, "占空比: %d", 
                          hal_pwm_pulse_width_to_duty(PWM_SERVO_MID_US));
    lv_obj_set_style_text_font(data->duty_cycle_label, &simhei_32, 0);
    lv_obj_align(data->duty_cycle_label, LV_ALIGN_TOP_LEFT, 240, 130);  // 调整位置

    // PWM使能开关
    lv_obj_t* enable_label = lv_label_create(control_panel);
    lv_label_set_text(enable_label, "PWM输出:");
    lv_obj_set_style_text_font(enable_label, &simhei_32, 0);
    lv_obj_align(enable_label, LV_ALIGN_TOP_LEFT, 240, 170);  // 调整位置

    data->enable_switch = lv_switch_create(control_panel);
    lv_obj_align(data->enable_switch, LV_ALIGN_TOP_LEFT, 360, 165);  // 调整位置
    lv_obj_add_event_cb(data->enable_switch, enable_switch_event_cb, LV_EVENT_VALUE_CHANGED, data);

    // 重置按钮
    data->reset_button = lv_btn_create(control_panel);
    lv_obj_set_size(data->reset_button, 90, 40);  // 稍微增大按钮以适应中文字体
    lv_obj_align(data->reset_button, LV_ALIGN_TOP_LEFT, 240, 210);  // 调整位置
    lv_obj_add_event_cb(data->reset_button, reset_button_event_cb, LV_EVENT_CLICKED, data);

    lv_obj_t* reset_label = lv_label_create(data->reset_button);
    lv_label_set_text(reset_label, "重置");
    lv_obj_set_style_text_font(reset_label, &simhei_32, 0);
    lv_obj_center(reset_label);

    // 测试按钮
    data->test_button = lv_btn_create(control_panel);
    lv_obj_set_size(data->test_button, 90, 40);  // 稍微增大按钮以适应中文字体
    lv_obj_align(data->test_button, LV_ALIGN_TOP_LEFT, 340, 210);  // 调整位置
    lv_obj_add_event_cb(data->test_button, test_button_event_cb, LV_EVENT_CLICKED, data);

    lv_obj_t* test_label = lv_label_create(data->test_button);
    lv_label_set_text(test_label, "测试");
    lv_obj_set_style_text_font(test_label, &simhei_32, 0);
    lv_obj_center(test_label);
}

// 创建信息面板
static void create_info_panel(pwm_servo_data_t* data, lv_obj_t* parent) {
    data->info_panel = lv_obj_create(parent);
    lv_obj_set_size(data->info_panel, LV_PCT(100), 180);  // 增加高度以适应中文字体
    lv_obj_align(data->info_panel, LV_ALIGN_BOTTOM_MID, 0, -5);  // 稍微向上调整
    lv_obj_clear_flag(data->info_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(data->info_panel, 15, 0);

    // 信息面板标题
    lv_obj_t* info_title = lv_label_create(data->info_panel);
    lv_label_set_text(info_title, "状态信息");
    lv_obj_set_style_text_font(info_title, &simhei_32, 0);
    lv_obj_align(info_title, LV_ALIGN_TOP_LEFT, 0, 0);

    // 通道信息
    data->info_channel_label = lv_label_create(data->info_panel);
    lv_label_set_text(data->info_channel_label, "通道: G0 (PWM_CHANNEL_0)");
    lv_obj_set_style_text_font(data->info_channel_label, &simhei_32, 0);
    lv_obj_align(data->info_channel_label, LV_ALIGN_TOP_LEFT, 0, 35);  // 调整位置

    // GPIO引脚信息
    data->info_gpio_label = lv_label_create(data->info_panel);
    lv_label_set_text(data->info_gpio_label, "GPIO: 0");
    lv_obj_set_style_text_font(data->info_gpio_label, &simhei_32, 0);
    lv_obj_align(data->info_gpio_label, LV_ALIGN_TOP_LEFT, 0, 65);  // 调整位置

    // 频率信息
    data->info_frequency_label = lv_label_create(data->info_panel);
    lv_label_set_text_fmt(data->info_frequency_label, "频率: %dHz", PWM_FREQUENCY);
    lv_obj_set_style_text_font(data->info_frequency_label, &simhei_32, 0);
    lv_obj_align(data->info_frequency_label, LV_ALIGN_TOP_LEFT, 0, 95);  // 调整位置

    // 状态信息
    data->info_status_label = lv_label_create(data->info_panel);
    lv_label_set_text(data->info_status_label, "状态: 已禁用");
    lv_obj_set_style_text_font(data->info_status_label, &simhei_32, 0);
    lv_obj_align(data->info_status_label, LV_ALIGN_TOP_LEFT, 0, 125);  // 调整位置
}

// 引脚下拉框事件回调
static void pin_dropdown_event_cb(lv_event_t* e) {
    pwm_servo_data_t* data = (pwm_servo_data_t*)lv_event_get_user_data(e);
    if (!data || !data->is_initialized) return;

    uint16_t selected = lv_dropdown_get_selected(data->pin_dropdown);
    if (selected < g_pin_options_count) {
        pwm_channel_t new_channel = g_pin_options[selected].channel;
        if (new_channel != data->current_channel) {
            // 禁用当前PWM输出
            if (data->pwm_enabled) {
                set_pwm_enable(data, false);
                lv_obj_clear_state(data->enable_switch, LV_STATE_CHECKED);
            }
            
            // 更新PWM通道
            update_pwm_channel(data, new_channel);
            update_info_display(data);
        }
    }
}

// 角度弧形控件事件回调
static void angle_arc_event_cb(lv_event_t* e) {
    pwm_servo_data_t* data = (pwm_servo_data_t*)lv_event_get_user_data(e);
    if (!data || !data->is_initialized) return;

    int32_t angle = lv_arc_get_value(data->angle_arc);
    update_servo_angle(data, (uint8_t)angle);
}

// PWM使能开关事件回调
static void enable_switch_event_cb(lv_event_t* e) {
    pwm_servo_data_t* data = (pwm_servo_data_t*)lv_event_get_user_data(e);
    if (!data || !data->is_initialized) return;

    bool enable = lv_obj_has_state(data->enable_switch, LV_STATE_CHECKED);
    set_pwm_enable(data, enable);
}

// 重置按钮事件回调
static void reset_button_event_cb(lv_event_t* e) {
    pwm_servo_data_t* data = (pwm_servo_data_t*)lv_event_get_user_data(e);
    if (!data || !data->is_initialized) return;

    reset_servo_to_center(data);
}

// 测试按钮事件回调
static void test_button_event_cb(lv_event_t* e) {
    pwm_servo_data_t* data = (pwm_servo_data_t*)lv_event_get_user_data(e);
    if (!data || !data->is_initialized) return;

    run_servo_test_sequence(data);
}

// 更新PWM通道配置
bool update_pwm_channel(pwm_servo_data_t* data, pwm_channel_t channel) {
    if (!data || !data->is_initialized || channel >= PWM_CHANNEL_MAX) {
        return false;
    }

    const pin_option_t* pin_option = get_pin_option_by_channel(channel);
    if (!pin_option) {
        printf("Invalid PWM channel: %d\n", channel);
        return false;
    }

    esp_err_t ret = hal_pwm_config_channel(channel, pin_option->gpio_pin);
    if (ret != ESP_OK) {
        printf("Failed to configure PWM channel %d: %s\n", channel, esp_err_to_name(ret));
        return false;
    }

    data->current_channel = channel;
    printf("PWM channel updated to %s (GPIO %d)\n", pin_option->name, pin_option->gpio_pin);
    return true;
}

// 更新舵机角度
bool update_servo_angle(pwm_servo_data_t* data, uint8_t angle) {
    if (!data || !data->is_initialized) {
        return false;
    }

    // 限制角度范围
    if (angle > SERVO_ANGLE_MAX) {
        angle = SERVO_ANGLE_MAX;
    }

    // 更新内部状态
    data->current_angle = angle;
    data->current_pulse_width = hal_pwm_angle_to_pulse_width(angle);
    data->current_duty_cycle = hal_pwm_pulse_width_to_duty(data->current_pulse_width);

    // 更新显示
    lv_label_set_text_fmt(data->angle_label, "%d°", angle);
    lv_label_set_text_fmt(data->pulse_width_label, "脉宽: %dus", data->current_pulse_width);
    lv_label_set_text_fmt(data->duty_cycle_label, "占空比: %d", data->current_duty_cycle);
    lv_arc_set_value(data->angle_arc, angle);

    // 如果PWM已启用，更新输出
    if (data->pwm_enabled) {
        esp_err_t ret = hal_pwm_set_servo_angle(data->current_channel, angle);
        if (ret != ESP_OK) {
            printf("Failed to set servo angle: %s\n", esp_err_to_name(ret));
            return false;
        }
    }

    printf("Servo angle updated to %d degrees\n", angle);
    return true;
}

// 启用或禁用PWM输出
bool set_pwm_enable(pwm_servo_data_t* data, bool enable) {
    if (!data || !data->is_initialized) {
        return false;
    }

    esp_err_t ret;
    if (enable) {
        // 启用PWM输出
        ret = hal_pwm_set_servo_angle(data->current_channel, data->current_angle);
        if (ret == ESP_OK) {
            ret = hal_pwm_start(data->current_channel);
        }
    } else {
        // 禁用PWM输出
        ret = hal_pwm_stop(data->current_channel);
    }

    if (ret != ESP_OK) {
        printf("Failed to %s PWM: %s\n", enable ? "enable" : "disable", esp_err_to_name(ret));
        return false;
    }

    data->pwm_enabled = enable;
    update_info_display(data);
    
    printf("PWM output %s\n", enable ? "enabled" : "disabled");
    return true;
}

// 重置舵机到中位
bool reset_servo_to_center(pwm_servo_data_t* data) {
    if (!data || !data->is_initialized) {
        return false;
    }

    return update_servo_angle(data, SERVO_ANGLE_MID);
}

// 执行舵机测试序列
void run_servo_test_sequence(pwm_servo_data_t* data) {
    if (!data || !data->is_initialized) {
        return;
    }

    printf("Running servo test sequence\n");

    // 确保PWM已启用
    bool was_enabled = data->pwm_enabled;
    if (!was_enabled) {
        set_pwm_enable(data, true);
        lv_obj_add_state(data->enable_switch, LV_STATE_CHECKED);
    }

    // 测试序列：0° -> 90° -> 180° -> 90°
    uint8_t test_angles[] = {0, 90, 180, 90};
    size_t test_count = sizeof(test_angles) / sizeof(test_angles[0]);

    for (size_t i = 0; i < test_count; i++) {
        update_servo_angle(data, test_angles[i]);
        vTaskDelay(pdMS_TO_TICKS(1000));  // 等待1秒
        
        // 刷新LVGL显示
        lv_timer_handler();
    }

    // 恢复原始状态
    if (!was_enabled) {
        set_pwm_enable(data, false);
        lv_obj_clear_state(data->enable_switch, LV_STATE_CHECKED);
    }

    printf("Servo test sequence completed\n");
}

// 更新信息显示
void update_info_display(pwm_servo_data_t* data) {
    if (!data || !data->is_initialized) {
        return;
    }

    const pin_option_t* pin_option = get_pin_option_by_channel(data->current_channel);
    if (pin_option) {
        lv_label_set_text_fmt(data->info_channel_label, "通道: %s (PWM_CHANNEL_%d)", 
                              pin_option->name, data->current_channel);
        lv_label_set_text_fmt(data->info_gpio_label, "GPIO: %d", pin_option->gpio_pin);
    }

    lv_label_set_text_fmt(data->info_status_label, "状态: %s", 
                          data->pwm_enabled ? "已启用" : "已禁用");
}

// 根据通道获取引脚选项
const pin_option_t* get_pin_option_by_channel(pwm_channel_t channel) {
    for (size_t i = 0; i < g_pin_options_count; i++) {
        if (g_pin_options[i].channel == channel) {
            return &g_pin_options[i];
        }
    }
    return NULL;
}

// 根据GPIO引脚号获取引脚选项
const pin_option_t* get_pin_option_by_gpio(uint8_t gpio_pin) {
    for (size_t i = 0; i < g_pin_options_count; i++) {
        if (g_pin_options[i].gpio_pin == gpio_pin) {
            return &g_pin_options[i];
        }
    }
    return NULL;
} 