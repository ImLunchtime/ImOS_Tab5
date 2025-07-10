#ifndef HAL_H
#define HAL_H

#include <bsp/esp-bsp.h>
#include <stdint.h>

// Include sub-modules
#include "hal_audio.h"
#include "hal_display.h"
#include "hal_sdcard.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all HAL subsystems
 * 
 * This function initializes all hardware abstraction layers including:
 * - I2C bus
 * - IO expander
 * - Audio codec
 * - Display and touchpad
 */
void hal_init(void);

/**
 * @brief Deinitialize all HAL subsystems
 */
void hal_deinit(void);

/**
 * @brief Get system uptime in milliseconds
 * 
 * @return System uptime in milliseconds
 */
uint32_t hal_get_uptime_ms(void);

/**
 * @brief Delay for specified milliseconds
 * 
 * @param ms Delay time in milliseconds
 */
void hal_delay_ms(uint32_t ms);

/**
 * @brief Check if HAL is initialized
 * 
 * @return true if HAL is initialized
 */
bool hal_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_H 