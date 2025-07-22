#ifndef APP_PWM_SERVO_H
#define APP_PWM_SERVO_H

#include "managers/app_manager.h"
#include "hal/hal_pwm.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// PWM舵机应用数据结构
typedef struct {
    bool is_initialized;
    pwm_channel_t current_channel;        // 当前选择的PWM通道
    uint8_t current_angle;                // 当前角度 (0-180)
    uint16_t current_pulse_width;         // 当前脉宽 (微秒)
    uint16_t current_duty_cycle;          // 当前占空比 (0-4095)
    bool pwm_enabled;                     // PWM输出是否启用
    
    // UI组件指针
    lv_obj_t* main_container;
    lv_obj_t* title_label;
    lv_obj_t* pin_dropdown;
    lv_obj_t* angle_arc;
    lv_obj_t* angle_label;
    lv_obj_t* pulse_width_label;
    lv_obj_t* duty_cycle_label;
    lv_obj_t* enable_switch;
    lv_obj_t* reset_button;
    lv_obj_t* test_button;
    
    // 状态信息面板
    lv_obj_t* info_panel;
    lv_obj_t* info_channel_label;
    lv_obj_t* info_gpio_label;
    lv_obj_t* info_frequency_label;
    lv_obj_t* info_status_label;
} pwm_servo_data_t;

// 引脚选项数据
typedef struct {
    const char* name;        // 显示名称 (如 "G0")
    uint8_t gpio_pin;        // GPIO引脚号
    pwm_channel_t channel;   // PWM通道
} pin_option_t;

// 预定义的引脚选项
extern const pin_option_t g_pin_options[];
extern const size_t g_pin_options_count;

/**
 * @brief 注册PWM舵机测试应用
 */
void register_pwm_servo_app(void);

/**
 * @brief PWM舵机应用创建回调
 * 
 * @param app 应用指针
 */
void pwm_servo_app_create(app_t* app);

/**
 * @brief PWM舵机应用销毁回调
 * 
 * @param app 应用指针
 */
void pwm_servo_app_destroy(app_t* app);

/**
 * @brief 更新PWM通道配置
 * 
 * @param data 应用数据指针
 * @param channel 新的PWM通道
 * @return true 配置成功，false 配置失败
 */
bool update_pwm_channel(pwm_servo_data_t* data, pwm_channel_t channel);

/**
 * @brief 更新舵机角度
 * 
 * @param data 应用数据指针
 * @param angle 新的角度 (0-180)
 * @return true 设置成功，false 设置失败
 */
bool update_servo_angle(pwm_servo_data_t* data, uint8_t angle);

/**
 * @brief 启用或禁用PWM输出
 * 
 * @param data 应用数据指针
 * @param enable true启用，false禁用
 * @return true 操作成功，false 操作失败
 */
bool set_pwm_enable(pwm_servo_data_t* data, bool enable);

/**
 * @brief 重置舵机到中位
 * 
 * @param data 应用数据指针
 * @return true 重置成功，false 重置失败
 */
bool reset_servo_to_center(pwm_servo_data_t* data);

/**
 * @brief 执行舵机测试序列
 * 
 * @param data 应用数据指针
 */
void run_servo_test_sequence(pwm_servo_data_t* data);

/**
 * @brief 更新信息显示
 * 
 * @param data 应用数据指针
 */
void update_info_display(pwm_servo_data_t* data);

/**
 * @brief 根据通道获取引脚选项
 * 
 * @param channel PWM通道
 * @return const pin_option_t* 引脚选项指针，NULL表示未找到
 */
const pin_option_t* get_pin_option_by_channel(pwm_channel_t channel);

/**
 * @brief 根据GPIO引脚号获取引脚选项
 * 
 * @param gpio_pin GPIO引脚号
 * @return const pin_option_t* 引脚选项指针，NULL表示未找到
 */
const pin_option_t* get_pin_option_by_gpio(uint8_t gpio_pin);

#ifdef __cplusplus
}
#endif

#endif // APP_PWM_SERVO_H 