#include "hal.h"
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

// HAL initialization state
static bool g_hal_initialized = false;

void hal_init(void)
{
    if (g_hal_initialized) {
        printf("HAL already initialized\n");
        return;
    }

    printf("Initializing HAL...\n");

    // Initialize I2C bus
    esp_err_t ret = bsp_i2c_init();
    if (ret != ESP_OK) {
        printf("Failed to initialize I2C bus: %s\n", esp_err_to_name(ret));
        return;
    }
    
    // Wait for I2C to stabilize
    vTaskDelay(pdMS_TO_TICKS(200));

    // Get I2C bus handle and initialize IO expander
    i2c_master_bus_handle_t i2c_bus_handle = bsp_i2c_get_handle();
    if (!i2c_bus_handle) {
        printf("Failed to get I2C bus handle\n");
        return;
    }

    // Initialize IO expander (returns void)
    bsp_io_expander_pi4ioe_init(i2c_bus_handle);
    printf("IO expander initialized\n");

    // Initialize audio subsystem
    hal_audio_init();

    // Initialize display subsystem
    hal_display_init();

    // Initialize touchpad
    hal_touchpad_init();

    // Initialize SD card
    hal_sdcard_init();

    g_hal_initialized = true;
    printf("HAL initialized successfully\n");
    
    // Run system tests
    // run_system_tests(); removed for stabillity and performance reasons
}

void hal_deinit(void)
{
    if (!g_hal_initialized) {
        printf("HAL not initialized\n");
        return;
    }

    printf("Deinitializing HAL...\n");

    // Turn off display backlight
    hal_display_backlight_off();

    // Deinitialize SD card
    hal_sdcard_deinit();

    // Deinitialize I2C (if needed)
    // bsp_i2c_deinit(); // Usually not needed as it's used by other components

    g_hal_initialized = false;
    printf("HAL deinitialized\n");
}

uint32_t hal_get_uptime_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void hal_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

bool hal_is_initialized(void)
{
    return g_hal_initialized;
} 