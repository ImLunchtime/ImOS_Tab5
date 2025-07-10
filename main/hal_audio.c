#include "hal_audio.h"
#include <bsp/esp-bsp.h>
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <audio_player.h>
#include <esp_err.h>

// Audio state management
typedef struct {
    bool is_initialized;
    bool is_playing;
    uint8_t current_volume;
    SemaphoreHandle_t audio_mutex;
} audio_state_t;

// MP3 playback state
typedef struct {
    bool is_playing;
    bool is_initialized;
    uint32_t start_time;
    uint32_t duration;
    char current_file[256];
    SemaphoreHandle_t mp3_mutex;
} mp3_state_t;

// Global audio state
static audio_state_t g_audio_state = {
    .is_initialized = false,
    .is_playing = false,
    .current_volume = 50,  // Default volume 50%
    .audio_mutex = NULL
};

// Global MP3 state
static mp3_state_t g_mp3_state = {
    .is_playing = false,
    .is_initialized = false,
    .start_time = 0,
    .duration = 0,
    .current_file = {0},
    .mp3_mutex = NULL
};

// Global variables for MP3 playback monitoring
static uint32_t g_expected_sample_rate = 44100;
static bool g_override_audio_player_config = false;

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
        // Set a reasonable default I2S configuration
        // This will be overridden by audio_player when playing MP3 files
        codec_handle->i2s_reconfig_clk_fn(44100, 16, I2S_SLOT_MODE_STEREO);
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

/* -------------------------------------------------------------------------- */
/*                               MP3 Playback                                */
/* -------------------------------------------------------------------------- */

// Forward declaration for audio reconfiguration
static esp_err_t hal_audio_force_reconfig(uint32_t sample_rate, uint32_t bits_per_sample, i2s_slot_mode_t slot_mode);

// Audio player callback for MP3 playback
static void mp3_audio_player_callback(audio_player_cb_ctx_t* ctx)
{
    printf("MP3 audio event: %d\n", (int)ctx->audio_event);
    
    audio_player_state_t state = audio_player_get_state();
    printf("MP3 audio state: %d\n", (int)state);
    
    if (state == AUDIO_PLAYER_STATE_IDLE) {
        if (xSemaphoreTake(g_mp3_state.mp3_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_mp3_state.is_playing = false;
            // Reset override flag when playback finishes
            g_override_audio_player_config = false;
            printf("MP3 playback finished\n");
            xSemaphoreGive(g_mp3_state.mp3_mutex);
        }
    }
}

// Audio mute function for MP3 playback
static esp_err_t mp3_audio_mute_function(AUDIO_PLAYER_MUTE_SETTING setting)
{
    bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
    if (codec_handle) {
        codec_handle->set_mute(setting == AUDIO_PLAYER_MUTE ? true : false);
    }
    return ESP_OK;
}

// Force audio system reconfiguration for specific sample rate
static esp_err_t hal_audio_force_reconfig(uint32_t sample_rate, uint32_t bits_per_sample, i2s_slot_mode_t slot_mode)
{
    printf("Force reconfiguring audio system: %luHz, %lubit, %s\n", 
           (unsigned long)sample_rate, (unsigned long)bits_per_sample, 
           slot_mode == I2S_SLOT_MODE_STEREO ? "stereo" : "mono");
    
    bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
    if (!codec_handle) {
        printf("Failed to get codec handle for reconfiguration\n");
        return ESP_FAIL;
    }
    
    // Force reconfigure the codec with new sample rate
    esp_err_t ret = codec_handle->i2s_reconfig_clk_fn(sample_rate, bits_per_sample, slot_mode);
    if (ret != ESP_OK) {
        printf("Failed to reconfigure I2S clock: %s\n", esp_err_to_name(ret));
        return ret;
    }
    
    // Small delay to let the configuration stabilize
    vTaskDelay(pdMS_TO_TICKS(50));
    
    printf("Audio system reconfigured successfully\n");
    return ESP_OK;
}

// Wrapper for the original I2S clock configuration function
static esp_err_t mp3_clk_set_wrapper(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    printf("audio_player calling clk_set_fn: %luHz, %lubit, %s\n", 
           (unsigned long)rate, (unsigned long)bits_cfg, 
           ch == I2S_SLOT_MODE_STEREO ? "stereo" : "mono");
    
    bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
    if (!codec_handle) {
        printf("Failed to get codec handle in wrapper\n");
        return ESP_FAIL;
    }
    
    // If we're overriding, use our expected sample rate instead
    if (g_override_audio_player_config) {
        printf("OVERRIDING audio_player config: using %luHz instead of %luHz\n", 
               (unsigned long)g_expected_sample_rate, (unsigned long)rate);
        rate = g_expected_sample_rate;
    }
    
    // Call the original function
    esp_err_t ret = codec_handle->i2s_reconfig_clk_fn(rate, bits_cfg, ch);
    
    printf("clk_set_fn result: %s\n", esp_err_to_name(ret));
    return ret;
}

// Simple MP3 header parser to detect sample rate
static uint32_t hal_audio_detect_mp3_sample_rate(const char* file_path)
{
    // MP3 sample rate table for MPEG-1 Layer III
    static const uint32_t mp3_sample_rates[] = {44100, 48000, 32000, 0};
    // MP3 sample rate table for MPEG-2 Layer III  
    static const uint32_t mp3_sample_rates_v2[] = {22050, 24000, 16000, 0};
    
    FILE* fp = fopen(file_path, "rb");
    if (!fp) {
        printf("Failed to open MP3 file for analysis: %s\n", file_path);
        return 44100; // Default fallback
    }
    
    uint8_t header[4];
    uint32_t sample_rate = 44100; // Default fallback
    
    // Read file in chunks to find MP3 frame header
    for (int i = 0; i < 1024; i++) {
        if (fread(header, 1, 4, fp) != 4) {
            break;
        }
        
        // Check for MP3 frame sync (first 11 bits all set)
        if ((header[0] == 0xFF) && ((header[1] & 0xE0) == 0xE0)) {
            // Extract version and sample rate index
            uint8_t version = (header[1] >> 3) & 0x03;
            uint8_t sample_rate_index = (header[2] >> 2) & 0x03;
            
            if (version == 0x03) { // MPEG-1
                if (sample_rate_index < 3) {
                    sample_rate = mp3_sample_rates[sample_rate_index];
                    printf("Detected MP3 sample rate: %lu Hz (MPEG-1)\n", (unsigned long)sample_rate);
                    break;
                }
            } else if (version == 0x02) { // MPEG-2
                if (sample_rate_index < 3) {
                    sample_rate = mp3_sample_rates_v2[sample_rate_index];
                    printf("Detected MP3 sample rate: %lu Hz (MPEG-2)\n", (unsigned long)sample_rate);
                    break;
                }
            }
        }
        
        // Move back 3 bytes to slide the window
        fseek(fp, -3, SEEK_CUR);
    }
    
    fclose(fp);
    
    if (sample_rate == 44100) {
        printf("Using default sample rate: %lu Hz\n", (unsigned long)sample_rate);
    }
    
    return sample_rate;
}

bool hal_audio_play_mp3_file(const char* file_path)
{
    if (!file_path) {
        printf("Invalid MP3 file path\n");
        return false;
    }
    
    // Create MP3 mutex if not already created
    if (g_mp3_state.mp3_mutex == NULL) {
        g_mp3_state.mp3_mutex = xSemaphoreCreateMutex();
        if (g_mp3_state.mp3_mutex == NULL) {
            printf("Failed to create MP3 mutex\n");
            return false;
        }
    }
    
    if (xSemaphoreTake(g_mp3_state.mp3_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // Stop any current playback
        if (g_mp3_state.is_playing) {
            hal_audio_stop_mp3();
        }
        
        // Configure codec
        bsp_codec_config_t* codec_handle = bsp_get_codec_handle();
        if (!codec_handle) {
            printf("Failed to get codec handle for MP3\n");
            xSemaphoreGive(g_mp3_state.mp3_mutex);
            return false;
        }
        
        // Detect MP3 sample rate from file header
        uint32_t detected_sample_rate = hal_audio_detect_mp3_sample_rate(file_path);
        printf("MP3 file analysis complete, detected sample rate: %lu Hz\n", (unsigned long)detected_sample_rate);
        
        // Set global variables for the wrapper function
        g_expected_sample_rate = detected_sample_rate;
        g_override_audio_player_config = true;
        
        // Force reconfigure audio system with detected sample rate
        esp_err_t reconfig_ret = hal_audio_force_reconfig(detected_sample_rate, 16, I2S_SLOT_MODE_STEREO);
        if (reconfig_ret != ESP_OK) {
            printf("Failed to reconfigure audio system for MP3 playback\n");
            xSemaphoreGive(g_mp3_state.mp3_mutex);
            return false;
        }
        
        codec_handle->set_volume(g_audio_state.current_volume);
        // Remove hardcoded sample rate - let audio_player detect and set correct rate
        // codec_handle->i2s_reconfig_clk_fn(48000, 16, I2S_SLOT_MODE_STEREO);
        
        // Configure audio player with our wrapper function
        audio_player_config_t config = {
            .mute_fn = mp3_audio_mute_function,
            .clk_set_fn = mp3_clk_set_wrapper,  // Use our wrapper function
            .write_fn = codec_handle->i2s_write,
            .priority = 8,
            .coreID = 1,
        };
        
        esp_err_t ret = audio_player_new(config);
        if (ret != ESP_OK) {
            printf("Failed to create audio player: %s\n", esp_err_to_name(ret));
            g_override_audio_player_config = false;  // Reset override flag
            xSemaphoreGive(g_mp3_state.mp3_mutex);
            return false;
        }
        
        // Register callback
        audio_player_callback_register(mp3_audio_player_callback, NULL);
        
        // Open and play MP3 file
        FILE* fp = fopen(file_path, "rb");
        if (!fp) {
            printf("Failed to open MP3 file: %s\n", file_path);
            audio_player_delete();
            g_override_audio_player_config = false;  // Reset override flag
            xSemaphoreGive(g_mp3_state.mp3_mutex);
            return false;
        }
        
        ret = audio_player_play(fp);
        if (ret != ESP_OK) {
            printf("Failed to start MP3 playback: %s\n", esp_err_to_name(ret));
            fclose(fp);
            audio_player_delete();
            g_override_audio_player_config = false;  // Reset override flag
            xSemaphoreGive(g_mp3_state.mp3_mutex);
            return false;
        }
        
        // Give audio_player a moment to initialize and call clk_set_fn
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Force reconfigure again after audio_player has started
        // This is to ensure our sample rate is used even if audio_player overrides it
        printf("Force reconfiguring after audio_player start...\n");
        reconfig_ret = hal_audio_force_reconfig(detected_sample_rate, 16, I2S_SLOT_MODE_STEREO);
        if (reconfig_ret != ESP_OK) {
            printf("Warning: Failed to force reconfigure after start: %s\n", esp_err_to_name(reconfig_ret));
        }
        
        // Update state
        g_mp3_state.is_playing = true;
        g_mp3_state.is_initialized = true;
        g_mp3_state.start_time = (uint32_t)(esp_timer_get_time() / 1000000);
        g_mp3_state.duration = 0; // Duration not available from audio_player
        strncpy(g_mp3_state.current_file, file_path, sizeof(g_mp3_state.current_file) - 1);
        g_mp3_state.current_file[sizeof(g_mp3_state.current_file) - 1] = '\0';
        
        printf("Started MP3 playback: %s at %lu Hz (override active: %s)\n", 
               file_path, (unsigned long)detected_sample_rate, 
               g_override_audio_player_config ? "yes" : "no");
        xSemaphoreGive(g_mp3_state.mp3_mutex);
        return true;
    }
    
    printf("Failed to acquire MP3 mutex\n");
    return false;
}

void hal_audio_stop_mp3(void)
{
    if (g_mp3_state.mp3_mutex == NULL) {
        return;
    }
    
    if (xSemaphoreTake(g_mp3_state.mp3_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (g_mp3_state.is_playing) {
            printf("Stopping MP3 playback\n");
            
            // Reset override flag
            g_override_audio_player_config = false;
            
            esp_err_t ret = audio_player_delete();
            if (ret != ESP_OK) {
                printf("Failed to delete audio player: %s\n", esp_err_to_name(ret));
            }
            
            g_mp3_state.is_playing = false;
            g_mp3_state.is_initialized = false;
            g_mp3_state.start_time = 0;
            g_mp3_state.duration = 0;
            g_mp3_state.current_file[0] = '\0';
            
            printf("MP3 playback stopped\n");
        }
        xSemaphoreGive(g_mp3_state.mp3_mutex);
    }
}

bool hal_audio_is_mp3_playing(void)
{
    if (g_mp3_state.mp3_mutex == NULL) {
        return false;
    }
    
    bool playing = false;
    if (xSemaphoreTake(g_mp3_state.mp3_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        playing = g_mp3_state.is_playing;
        xSemaphoreGive(g_mp3_state.mp3_mutex);
    }
    
    return playing;
}

uint32_t hal_audio_get_mp3_position(void)
{
    if (g_mp3_state.mp3_mutex == NULL || !g_mp3_state.is_playing) {
        return 0;
    }
    
    uint32_t position = 0;
    if (xSemaphoreTake(g_mp3_state.mp3_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_mp3_state.is_playing) {
            uint32_t current_time = (uint32_t)(esp_timer_get_time() / 1000000);
            position = current_time - g_mp3_state.start_time;
        }
        xSemaphoreGive(g_mp3_state.mp3_mutex);
    }
    
    return position;
}

uint32_t hal_audio_get_mp3_duration(void)
{
    if (g_mp3_state.mp3_mutex == NULL) {
        return 0;
    }
    
    uint32_t duration = 0;
    if (xSemaphoreTake(g_mp3_state.mp3_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        duration = g_mp3_state.duration;
        xSemaphoreGive(g_mp3_state.mp3_mutex);
    }
    
    return duration;
} 