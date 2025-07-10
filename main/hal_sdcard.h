#ifndef HAL_SDCARD_H
#define HAL_SDCARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and mount SD card
 * 
 * @return true if SD card mounted successfully
 */
bool hal_sdcard_init(void);

/**
 * @brief Deinitialize and unmount SD card
 */
void hal_sdcard_deinit(void);

/**
 * @brief Check if SD card is mounted
 * 
 * @return true if SD card is mounted
 */
bool hal_sdcard_is_mounted(void);

/**
 * @brief Get SD card mount point
 * 
 * @return SD card mount point path
 */
const char* hal_sdcard_get_mount_point(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_SDCARD_H 