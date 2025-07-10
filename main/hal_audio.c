#include "hal_audio.h"
#include <bsp/esp-bsp.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// Audio state management
typedef struct {
    bool is_initialized;
    bool is_playing;
    uint8_t current_volume;
    SemaphoreHandle_t audio_mutex;
} audio_state_t;

// Global audio state
static audio_state_t g_audio_state = {
    .is_initialized = false,
    .is_playing = false,
    .current_volume = 50,  // Default volume 50%
    .audio_mutex = NULL
};

// Helper function to clamp values
static uint8_t clamp_uint8(uint8_t value, uint8_t min_val, uint8_t max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

void hal_audio_init(void)
{
    if (g_audio_state.is_initialized) {
        return;
    }

    // Create mutex for audio operations
    g_audio_state.audio_mutex = xSemaphoreCreateMutex();
    if (g_audio_state.audio_mutex == NULL) {
        printf("Failed to create audio mutex\n");
        return;
    }

    // Initialize codec (returns void)
    bsp_codec_init();
    printf("Codec initialized\n");

    // Set initial volume
    bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
    if (codec_handle) {
        codec_handle->set_volume(g_audio_state.current_volume);
    }

    g_audio_state.is_initialized = true;
    printf("Audio HAL initialized successfully\n");
}

void hal_set_speaker_volume(uint8_t volume)
{
    if (!g_audio_state.is_initialized) {
        printf("Audio not initialized\n");
        return;
    }

    if (xSemaphoreTake(g_audio_state.audio_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_audio_state.current_volume = clamp_uint8(volume, 0, 100);
        
        // Set codec volume
        bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
        if (codec_handle) {
            codec_handle->set_volume(g_audio_state.current_volume);
        }
        
        printf("Set speaker volume: %d%%\n", g_audio_state.current_volume);
        xSemaphoreGive(g_audio_state.audio_mutex);
    }
}

uint8_t hal_get_speaker_volume(void)
{
    return g_audio_state.current_volume;
}

bool hal_audio_play_pcm(const int16_t* data, size_t samples, uint32_t sample_rate, bool is_stereo)
{
    if (!g_audio_state.is_initialized || !data || samples == 0) {
        printf("Invalid audio play parameters\n");
        return false;
    }

    if (xSemaphoreTake(g_audio_state.audio_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_audio_state.is_playing) {
            printf("Audio already playing\n");
            xSemaphoreGive(g_audio_state.audio_mutex);
            return false;
        }

        g_audio_state.is_playing = true;
        xSemaphoreGive(g_audio_state.audio_mutex);

        // Get codec handle
        bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
        if (!codec_handle) {
            printf("Failed to get codec handle\n");
            g_audio_state.is_playing = false;
            return false;
        }

        // Configure codec for playback
        codec_handle->set_volume(g_audio_state.current_volume);
        
        // Configure I2S for the specified sample rate and channels
        esp_err_t ret = codec_handle->i2s_reconfig_clk_fn(
            sample_rate, 
            16, 
            is_stereo ? I2S_SLOT_MODE_STEREO : I2S_SLOT_MODE_MONO
        );
        
        if (ret != ESP_OK) {
            printf("Failed to configure I2S: %s\n", esp_err_to_name(ret));
            g_audio_state.is_playing = false;
            return false;
        }

        // Play audio data (remove const to match function signature)
        size_t bytes_written = 0;
        size_t total_bytes = samples * sizeof(int16_t);
        
        ret = codec_handle->i2s_write(
            (void*)data,  // Cast away const to match function signature
            total_bytes, 
            &bytes_written, 
            pdMS_TO_TICKS(5000)  // 5 second timeout
        );

        if (ret != ESP_OK) {
            printf("Failed to write audio data: %s\n", esp_err_to_name(ret));
            g_audio_state.is_playing = false;
            return false;
        }

        printf("Audio playback completed: %zu bytes written\n", bytes_written);
        g_audio_state.is_playing = false;
        return true;
    }

    printf("Failed to acquire audio mutex\n");
    return false;
}

bool hal_audio_is_playing(void)
{
    return g_audio_state.is_playing;
}

void hal_audio_stop(void)
{
    if (xSemaphoreTake(g_audio_state.audio_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_audio_state.is_playing = false;
        xSemaphoreGive(g_audio_state.audio_mutex);
        printf("Audio playback stopped\n");
    }
}

size_t hal_audio_record(int16_t* buffer, size_t buffer_size, uint32_t duration_ms, float gain)
{
    if (!g_audio_state.is_initialized || !buffer || buffer_size == 0) {
        printf("Invalid audio record parameters\n");
        return 0;
    }

    if (xSemaphoreTake(g_audio_state.audio_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Get codec handle
        bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
        if (!codec_handle) {
            printf("Failed to get codec handle\n");
            xSemaphoreGive(g_audio_state.audio_mutex);
            return 0;
        }

        // Set recording gain
        codec_handle->set_in_gain(gain);

        // Calculate expected data size
        // Assuming 48kHz, 4-channel (quad) recording
        size_t expected_samples = (48000 * 4 * duration_ms) / 1000;
        size_t expected_bytes = expected_samples * sizeof(int16_t);
        
        // Use the smaller of buffer size or expected size
        size_t bytes_to_read = (buffer_size < expected_bytes) ? buffer_size : expected_bytes;
        
        // Record audio data
        size_t bytes_read = 0;
        esp_err_t ret = codec_handle->i2s_read(
            (void*)buffer, 
            bytes_to_read, 
            &bytes_read, 
            pdMS_TO_TICKS(duration_ms + 1000)  // Add 1 second timeout buffer
        );

        xSemaphoreGive(g_audio_state.audio_mutex);

        if (ret != ESP_OK) {
            printf("Failed to read audio data: %s\n", esp_err_to_name(ret));
            return 0;
        }

        printf("Audio recording completed: %zu bytes read\n", bytes_read);
        return bytes_read;
    }

    printf("Failed to acquire audio mutex for recording\n");
    return 0;
} 