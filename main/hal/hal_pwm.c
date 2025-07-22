#include "hal_pwm.h"
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <driver/ledc.h>
#include <esp_err.h>

// PWM状态管理
typedef struct {
    bool is_initialized;
    bool channel_configured[PWM_CHANNEL_MAX];
    uint8_t gpio_pins[PWM_CHANNEL_MAX];
    uint16_t current_duty[PWM_CHANNEL_MAX];
    SemaphoreHandle_t pwm_mutex;
} pwm_state_t;

// 全局PWM状态
static pwm_state_t g_pwm_state = {
    .is_initialized = false,
    .channel_configured = {false, false},
    .gpio_pins = {0xFF, 0xFF},  // 0xFF表示未配置
    .current_duty = {0, 0},
    .pwm_mutex = NULL
};

// 计算PWM周期的计数值
#define PWM_PERIOD_COUNT   ((1 << PWM_RESOLUTION) - 1)  // 4095 for 12-bit

// 工具函数：限制值在范围内
static uint16_t clamp_uint16(uint16_t value, uint16_t min_val, uint16_t max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

static uint8_t clamp_uint8(uint8_t value, uint8_t min_val, uint8_t max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

esp_err_t hal_pwm_init(void)
{
    if (g_pwm_state.is_initialized) {
        return ESP_OK;
    }

    // 创建互斥锁
    g_pwm_state.pwm_mutex = xSemaphoreCreateMutex();
    if (g_pwm_state.pwm_mutex == NULL) {
        printf("Failed to create PWM mutex\n");
        return ESP_FAIL;
    }

    // 配置LEDC定时器
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_1,  // 使用定时器1，避免与显示冲突
        .freq_hz = PWM_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };
    
    esp_err_t ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        printf("Failed to configure LEDC timer: %s\n", esp_err_to_name(ret));
        vSemaphoreDelete(g_pwm_state.pwm_mutex);
        g_pwm_state.pwm_mutex = NULL;
        return ret;
    }

    // 初始化状态
    memset(g_pwm_state.channel_configured, false, sizeof(g_pwm_state.channel_configured));
    memset(g_pwm_state.gpio_pins, 0xFF, sizeof(g_pwm_state.gpio_pins));
    memset(g_pwm_state.current_duty, 0, sizeof(g_pwm_state.current_duty));

    g_pwm_state.is_initialized = true;
    printf("PWM HAL initialized successfully\n");
    return ESP_OK;
}

esp_err_t hal_pwm_deinit(void)
{
    if (!g_pwm_state.is_initialized) {
        return ESP_OK;
    }

    if (xSemaphoreTake(g_pwm_state.pwm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 停止所有PWM通道
        for (int i = 0; i < PWM_CHANNEL_MAX; i++) {
            if (g_pwm_state.channel_configured[i]) {
                hal_pwm_stop((pwm_channel_t)i);
            }
        }

        g_pwm_state.is_initialized = false;
        xSemaphoreGive(g_pwm_state.pwm_mutex);
    }

    // 删除互斥锁
    if (g_pwm_state.pwm_mutex) {
        vSemaphoreDelete(g_pwm_state.pwm_mutex);
        g_pwm_state.pwm_mutex = NULL;
    }

    printf("PWM HAL deinitialized\n");
    return ESP_OK;
}

esp_err_t hal_pwm_config_channel(pwm_channel_t channel, uint8_t gpio_pin)
{
    if (!g_pwm_state.is_initialized) {
        printf("PWM HAL not initialized\n");
        return ESP_ERR_INVALID_STATE;
    }

    if (channel >= PWM_CHANNEL_MAX) {
        printf("Invalid PWM channel: %d\n", channel);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_pwm_state.pwm_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 配置LEDC通道
        ledc_channel_config_t channel_config = {
            .gpio_num = gpio_pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_1,
            .duty = 0,
            .hpoint = 0
        };

        esp_err_t ret = ledc_channel_config(&channel_config);
        if (ret != ESP_OK) {
            printf("Failed to configure LEDC channel %d: %s\n", channel, esp_err_to_name(ret));
            xSemaphoreGive(g_pwm_state.pwm_mutex);
            return ret;
        }

        // 更新状态
        g_pwm_state.channel_configured[channel] = true;
        g_pwm_state.gpio_pins[channel] = gpio_pin;
        g_pwm_state.current_duty[channel] = 0;

        printf("PWM channel %d configured on GPIO %d\n", channel, gpio_pin);
        xSemaphoreGive(g_pwm_state.pwm_mutex);
        return ESP_OK;
    }

    printf("Failed to acquire PWM mutex\n");
    return ESP_ERR_TIMEOUT;
}

esp_err_t hal_pwm_set_duty(pwm_channel_t channel, uint16_t duty_cycle)
{
    if (!g_pwm_state.is_initialized) {
        printf("PWM HAL not initialized\n");
        return ESP_ERR_INVALID_STATE;
    }

    if (channel >= PWM_CHANNEL_MAX || !g_pwm_state.channel_configured[channel]) {
        printf("PWM channel %d not configured\n", channel);
        return ESP_ERR_INVALID_ARG;
    }

    // 限制占空比范围
    duty_cycle = clamp_uint16(duty_cycle, 0, PWM_PERIOD_COUNT);

    if (xSemaphoreTake(g_pwm_state.pwm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty_cycle);
        if (ret == ESP_OK) {
            ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
        }

        if (ret == ESP_OK) {
            g_pwm_state.current_duty[channel] = duty_cycle;
            printf("Set PWM channel %d duty to %d\n", channel, duty_cycle);
        } else {
            printf("Failed to set PWM duty: %s\n", esp_err_to_name(ret));
        }

        xSemaphoreGive(g_pwm_state.pwm_mutex);
        return ret;
    }

    printf("Failed to acquire PWM mutex\n");
    return ESP_ERR_TIMEOUT;
}

esp_err_t hal_pwm_set_pulse_width(pwm_channel_t channel, uint16_t pulse_width_us)
{
    // 限制脉宽范围
    pulse_width_us = clamp_uint16(pulse_width_us, PWM_SERVO_MIN_US, PWM_SERVO_MAX_US);
    
    // 转换脉宽为占空比
    uint16_t duty_cycle = hal_pwm_pulse_width_to_duty(pulse_width_us);
    
    return hal_pwm_set_duty(channel, duty_cycle);
}

esp_err_t hal_pwm_set_servo_angle(pwm_channel_t channel, uint8_t angle)
{
    // 限制角度范围
    angle = clamp_uint8(angle, SERVO_ANGLE_MIN, SERVO_ANGLE_MAX);
    
    // 转换角度为脉宽
    uint16_t pulse_width_us = hal_pwm_angle_to_pulse_width(angle);
    
    return hal_pwm_set_pulse_width(channel, pulse_width_us);
}

esp_err_t hal_pwm_stop(pwm_channel_t channel)
{
    if (!g_pwm_state.is_initialized) {
        printf("PWM HAL not initialized\n");
        return ESP_ERR_INVALID_STATE;
    }

    if (channel >= PWM_CHANNEL_MAX || !g_pwm_state.channel_configured[channel]) {
        printf("PWM channel %d not configured\n", channel);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_pwm_state.pwm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        esp_err_t ret = ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
        
        if (ret == ESP_OK) {
            printf("PWM channel %d stopped\n", channel);
        } else {
            printf("Failed to stop PWM channel %d: %s\n", channel, esp_err_to_name(ret));
        }

        xSemaphoreGive(g_pwm_state.pwm_mutex);
        return ret;
    }

    printf("Failed to acquire PWM mutex\n");
    return ESP_ERR_TIMEOUT;
}

esp_err_t hal_pwm_start(pwm_channel_t channel)
{
    if (!g_pwm_state.is_initialized) {
        printf("PWM HAL not initialized\n");
        return ESP_ERR_INVALID_STATE;
    }

    if (channel >= PWM_CHANNEL_MAX || !g_pwm_state.channel_configured[channel]) {
        printf("PWM channel %d not configured\n", channel);
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_pwm_state.pwm_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // 恢复之前的占空比
        esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 
                                      g_pwm_state.current_duty[channel]);
        if (ret == ESP_OK) {
            ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
        }

        if (ret == ESP_OK) {
            printf("PWM channel %d started with duty %d\n", channel, g_pwm_state.current_duty[channel]);
        } else {
            printf("Failed to start PWM channel %d: %s\n", channel, esp_err_to_name(ret));
        }

        xSemaphoreGive(g_pwm_state.pwm_mutex);
        return ret;
    }

    printf("Failed to acquire PWM mutex\n");
    return ESP_ERR_TIMEOUT;
}

uint8_t hal_pwm_get_gpio_pin(pwm_channel_t channel)
{
    if (channel >= PWM_CHANNEL_MAX || !g_pwm_state.channel_configured[channel]) {
        return 0xFF;  // 无效值
    }
    
    return g_pwm_state.gpio_pins[channel];
}

bool hal_pwm_is_initialized(void)
{
    return g_pwm_state.is_initialized;
}

uint16_t hal_pwm_angle_to_pulse_width(uint8_t angle)
{
    // 限制角度范围
    angle = clamp_uint8(angle, SERVO_ANGLE_MIN, SERVO_ANGLE_MAX);
    
    // 线性映射: 0-180度 -> 500-2500微秒
    uint16_t pulse_width = PWM_SERVO_MIN_US + 
                          ((uint32_t)(PWM_SERVO_MAX_US - PWM_SERVO_MIN_US) * angle) / SERVO_ANGLE_MAX;
    
    return pulse_width;
}

uint16_t hal_pwm_pulse_width_to_duty(uint16_t pulse_width_us)
{
    // 限制脉宽范围
    pulse_width_us = clamp_uint16(pulse_width_us, PWM_SERVO_MIN_US, PWM_SERVO_MAX_US);
    
    // 计算占空比
    // PWM周期 = 1/50Hz = 20ms = 20000微秒
    // 占空比 = (脉宽 / 周期) * 分辨率
    uint32_t period_us = 1000000 / PWM_FREQUENCY;  // 20000微秒
    uint32_t duty_cycle = ((uint32_t)pulse_width_us * PWM_PERIOD_COUNT) / period_us;
    
    return (uint16_t)clamp_uint16(duty_cycle, 0, PWM_PERIOD_COUNT);
} 