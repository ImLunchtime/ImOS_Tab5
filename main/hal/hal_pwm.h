#ifndef HAL_PWM_H
#define HAL_PWM_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// PWM引脚定义 (对应G0和G1扩展接口)
#define PWM_PIN_G0    0   // GPIO 0
#define PWM_PIN_G1    1   // GPIO 1

// PWM配置参数
#define PWM_FREQUENCY      50      // 50Hz，舵机标准频率
#define PWM_RESOLUTION     12      // 12位分辨率 (0-4095)
#define PWM_SERVO_MIN_US   500     // 舵机最小脉宽 (0.5ms)
#define PWM_SERVO_MAX_US   2500    // 舵机最大脉宽 (2.5ms)
#define PWM_SERVO_MID_US   1500    // 舵机中位脉宽 (1.5ms)

// PWM通道分配
typedef enum {
    PWM_CHANNEL_G0 = 0,  // 对应G0引脚
    PWM_CHANNEL_G1 = 1,  // 对应G1引脚
    PWM_CHANNEL_MAX
} pwm_channel_t;

// 舵机角度范围
#define SERVO_ANGLE_MIN    0       // 最小角度
#define SERVO_ANGLE_MAX    180     // 最大角度
#define SERVO_ANGLE_MID    90      // 中位角度

/**
 * @brief 初始化PWM子系统
 * 
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t hal_pwm_init(void);

/**
 * @brief 反初始化PWM子系统
 * 
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t hal_pwm_deinit(void);

/**
 * @brief 配置PWM通道
 * 
 * @param channel PWM通道
 * @param gpio_pin GPIO引脚号
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t hal_pwm_config_channel(pwm_channel_t channel, uint8_t gpio_pin);

/**
 * @brief 设置PWM占空比 (原始值)
 * 
 * @param channel PWM通道
 * @param duty_cycle 占空比 (0-4095)
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t hal_pwm_set_duty(pwm_channel_t channel, uint16_t duty_cycle);

/**
 * @brief 设置PWM脉宽 (微秒)
 * 
 * @param channel PWM通道
 * @param pulse_width_us 脉宽 (微秒)
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t hal_pwm_set_pulse_width(pwm_channel_t channel, uint16_t pulse_width_us);

/**
 * @brief 设置舵机角度
 * 
 * @param channel PWM通道
 * @param angle 角度 (0-180度)
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t hal_pwm_set_servo_angle(pwm_channel_t channel, uint8_t angle);

/**
 * @brief 停止PWM输出
 * 
 * @param channel PWM通道
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t hal_pwm_stop(pwm_channel_t channel);

/**
 * @brief 启动PWM输出
 * 
 * @param channel PWM通道
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t hal_pwm_start(pwm_channel_t channel);

/**
 * @brief 获取PWM通道对应的GPIO引脚
 * 
 * @param channel PWM通道
 * @return uint8_t GPIO引脚号，0xFF表示无效
 */
uint8_t hal_pwm_get_gpio_pin(pwm_channel_t channel);

/**
 * @brief 检查PWM子系统是否已初始化
 * 
 * @return bool true表示已初始化
 */
bool hal_pwm_is_initialized(void);

/**
 * @brief 将角度转换为脉宽
 * 
 * @param angle 角度 (0-180)
 * @return uint16_t 脉宽 (微秒)
 */
uint16_t hal_pwm_angle_to_pulse_width(uint8_t angle);

/**
 * @brief 将脉宽转换为占空比
 * 
 * @param pulse_width_us 脉宽 (微秒)
 * @return uint16_t 占空比 (0-4095)
 */
uint16_t hal_pwm_pulse_width_to_duty(uint16_t pulse_width_us);

#ifdef __cplusplus
}
#endif

#endif // HAL_PWM_H 