# 音频回环App说明

## 概述

音频回环App是一个用于测试M5Stack Tab5音频系统的应用程序。它将ES7210音频输入（IN1）的音频信号直接映射到ES8388音频输出，实现实时音频回环功能。

## 功能特性

### 🔄 音频回环功能
- **实时回环**：将ES7210的IN1输入直接映射到ES8388输出
- **双声道输出**：支持立体声输出
- **按住控制**：按住按钮开始回环，松开停止
- **状态监控**：实时显示回环状态和统计信息

### 🛡️ 安全保护
- **扬声器检测**：启动时自动检测扬声器状态
- **安全限制**：扬声器启用时拒绝工作，防止音频反馈
- **状态提示**：清晰的状态显示和警告信息

### 📊 统计信息
- **回环次数**：记录总回环次数
- **回环时间**：累计回环时间统计
- **实时状态**：当前工作状态显示

## 使用方法

### 1. 启动App
1. 在应用启动器中找到"音频回环"应用
2. 点击启动应用

### 2. 安全检查
- App启动时会自动检查扬声器状态
- 如果扬声器已启用，会显示警告信息
- 只有在扬声器禁用时才能使用回环功能

### 3. 开始回环
1. 确保扬声器已禁用
2. 按住"按住开始回环"按钮
3. 按钮变为红色，显示"松开停止回环"
4. 开始音频回环

### 4. 停止回环
- 松开按钮即可停止回环
- 按钮恢复绿色，显示"按住开始回环"

## 技术实现

### 音频流程
```
ES7210 IN1 → 音频录制 → 音频缓冲区 → ES8388输出
```

### 关键组件
- **ES7210**：音频输入编解码器
  - IN1：内置左麦克风
  - IN2：内置右麦克风
  - IN3：扬声器输出回环
  - IN4：耳机口麦克风输入

- **ES8388**：音频输出编解码器
  - 支持立体声输出
  - 可控制音量

- **NS4150**：扬声器功放
  - 通过PI4IOE5V控制使能

### 安全机制
```c
// 扬声器状态检查
bool check_speaker_status(audio_loopback_data_t* data) {
    data->speaker_enabled = hal_get_speaker_enable();
    
    if (data->speaker_enabled) {
        // 扬声器启用时拒绝工作
        data->state = AUDIO_LOOPBACK_STATE_ERROR;
        return false;
    }
    
    // 扬声器禁用时允许工作
    data->state = AUDIO_LOOPBACK_STATE_IDLE;
    return true;
}
```

### 音频回环任务
```c
static void audio_loopback_task(void* pvParameters) {
    while (data->state == AUDIO_LOOPBACK_STATE_ACTIVE) {
        // 1. 录制音频数据
        size_t bytes_read = hal_audio_record(buffer, size, 100ms, gain);
        
        // 2. 播放录制的数据
        bool success = hal_audio_play_pcm(buffer, samples, 48kHz, stereo);
        
        // 3. 检查按钮状态
        if (!data->button_pressed) break;
        
        // 4. 短暂延时
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## 配置参数

### 音频参数
```c
#define AUDIO_LOOPBACK_BUFFER_SIZE (1024 * 4)  // 4KB缓冲区
#define AUDIO_LOOPBACK_SAMPLE_RATE 48000        // 48kHz采样率
#define AUDIO_LOOPBACK_CHANNELS 2               // 立体声
```

### UI参数
```c
#define BUTTON_HEIGHT 80
#define BUTTON_WIDTH 300
#define TITLE_HEIGHT 80
```

## 状态管理

### 状态枚举
```c
typedef enum {
    AUDIO_LOOPBACK_STATE_IDLE,      // 空闲状态
    AUDIO_LOOPBACK_STATE_ACTIVE,    // 激活状态（正在回环）
    AUDIO_LOOPBACK_STATE_ERROR      // 错误状态
} audio_loopback_state_t;
```

### 数据结构
```c
typedef struct {
    audio_loopback_state_t state;   // 当前状态
    bool speaker_enabled;           // 扬声器是否启用
    bool button_pressed;            // 按钮是否按下
    uint32_t loopback_start_time;   // 回环开始时间
    uint32_t total_loopback_time;   // 总回环时间
    uint32_t loopback_count;        // 回环次数
} audio_loopback_data_t;
```

## 测试功能

### 自动测试
系统启动时会自动运行测试：
```c
test_audio_loopback_app_basic();  // 基本功能测试
```

### 手动测试
```c
// 基本功能测试
test_audio_loopback_basic();

// 安全检查测试
test_audio_loopback_safety();
```

## 注意事项

### ⚠️ 重要提醒
1. **扬声器安全**：使用前必须禁用扬声器，防止音频反馈
2. **音量控制**：建议在安静环境中使用，避免过大的音频输入
3. **耳机使用**：建议使用耳机进行测试，避免影响他人

### 🔧 故障排除
1. **App无法启动**：检查扬声器是否已禁用
2. **回环无声音**：检查音频输入设备是否正常工作
3. **音频延迟**：这是正常现象，由于音频处理需要时间

### 📋 使用建议
1. 在安静环境中测试
2. 使用耳机监听输出
3. 避免过大的音频输入
4. 定期检查扬声器状态

## 开发信息

### 文件结构
```
main/
├── app_audio_loopback.h          # 头文件
├── app_audio_loopback.c          # 实现文件
├── audio_loopback_test.c         # 测试文件
└── gui.c                         # 注册App
```

### 注册方式
```c
// 在gui.c中注册App
register_audio_loopback_app();
```

### 编译配置
```cmake
# 在CMakeLists.txt中添加源文件
"app_audio_loopback.c"
"audio_loopback_test.c"
```

## 版本历史

- **v1.0.0**：初始版本
  - 基本音频回环功能
  - 扬声器安全检查
  - 状态监控和统计
  - 用户友好的UI界面 