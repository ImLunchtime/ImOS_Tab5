#ifndef HAL_AUDIO_H
#define HAL_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize audio subsystem
 * 
 * This function initializes the audio codec and related hardware
 */
void hal_audio_init(void);

/**
 * @brief Set speaker volume
 * 
 * @param volume Volume level (0-100)
 */
void hal_set_speaker_volume(uint8_t volume);

/**
 * @brief Get current speaker volume
 * 
 * @return Current volume level (0-100)
 */
uint8_t hal_get_speaker_volume(void);

/**
 * @brief Simple audio playback function
 * 
 * @param data Pointer to audio data buffer (16-bit PCM)
 * @param samples Number of samples to play
 * @param sample_rate Sample rate in Hz
 * @param is_stereo true for stereo, false for mono
 * @return true if playback started successfully
 */
bool hal_audio_play_pcm(const int16_t* data, size_t samples, uint32_t sample_rate, bool is_stereo);

/**
 * @brief Check if audio is currently playing
 * 
 * @return true if audio is playing
 */
bool hal_audio_is_playing(void);

/**
 * @brief Stop current audio playback
 */
void hal_audio_stop(void);

/**
 * @brief Record audio data
 * 
 * @param buffer Buffer to store recorded data
 * @param buffer_size Size of the buffer in bytes
 * @param duration_ms Recording duration in milliseconds
 * @param gain Recording gain (0.0 - 100.0)
 * @return Number of bytes actually recorded
 */
size_t hal_audio_record(int16_t* buffer, size_t buffer_size, uint32_t duration_ms, float gain);

/**
 * @brief Play MP3 file from file system
 * 
 * @param file_path Path to MP3 file
 * @return true if playback started successfully
 */
bool hal_audio_play_mp3_file(const char* file_path);

/**
 * @brief Stop current MP3 playback
 */
void hal_audio_stop_mp3(void);

/**
 * @brief Check if MP3 is currently playing
 * 
 * @return true if MP3 is playing
 */
bool hal_audio_is_mp3_playing(void);

/**
 * @brief Get MP3 playback position in seconds
 * 
 * @return Current playback position
 */
uint32_t hal_audio_get_mp3_position(void);

/**
 * @brief Get MP3 total duration in seconds (if available)
 * 
 * @return Total duration, 0 if unknown
 */
uint32_t hal_audio_get_mp3_duration(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_AUDIO_H 