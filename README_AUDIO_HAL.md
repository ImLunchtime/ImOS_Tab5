# 音频HAL修改说明

## 概述

本次修改主要针对M5Stack Tab5设备的音频HAL层，修复了扬声器控制功能。设备使用PI4IOE5V芯片来控制扬声器功放，通过I2C接口进行通信。

## 硬件配置

- **音频输入**: ES7210 (IN1,IN2为内置左右麦克风，IN3为扬声器输出回环，IN4为耳机口麦克风输入)
- **音频输出**: ES8388
- **扬声器功放**: NS4150
- **控制芯片**: PI4IOE5V (I2C地址0x43和0x44)

## 主要修改

### 1. PI4IOE5V寄存器定义

更新了寄存器定义，与参考代码保持一致：

```c
#define PI4IO_REG_CHIP_RESET 0x01
#define PI4IO_REG_IO_DIR     0x03
#define PI4IO_REG_OUT_SET    0x05
#define PI4IO_REG_OUT_H_IM   0x07
#define PI4IO_REG_IN_DEF_STA 0x09
#define PI4IO_REG_PULL_EN    0x0B
#define PI4IO_REG_PULL_SEL   0x0D
#define PI4IO_REG_IN_STA     0x0F
#define PI4IO_REG_INT_MASK   0x11
#define PI4IO_REG_IRQ_STA    0x13
```

### 2. 设备初始化

修改了`init_pi4ioe5v()`函数，正确初始化两个PI4IOE5V设备：

- **PI4IOE1** (地址0x43): 控制扬声器使能、外部5V、LCD复位、触摸屏复位、摄像头复位
- **PI4IOE2** (地址0x44): 控制WiFi电源、USB 5V、充电使能等

### 3. 扬声器控制

扬声器使能通过PI4IOE1的P1引脚控制：

```c
#define SPEAKER_ENABLE_PIN   1  // PI4IOE1 P1引脚
```

### 4. 位操作宏

添加了位操作宏定义：

```c
#define setbit(x, y) x |= (0x01 << y)
#define clrbit(x, y) x &= ~(0x01 << y)
```

## 主要函数

### `hal_set_speaker_enable(bool enable)`
控制扬声器使能状态：
- `enable = true`: 启用扬声器
- `enable = false`: 禁用扬声器

### `hal_get_speaker_enable()`
获取当前扬声器使能状态，会从硬件读取实际状态。

### `hal_set_speaker_volume(uint8_t volume)`
设置扬声器音量 (0-100)。

### `hal_get_speaker_volume()`
获取当前扬声器音量。

## 测试

添加了音频HAL测试函数`test_audio_hal_basic()`，包括：

1. 扬声器使能/禁用测试
2. 音量控制测试
3. 边界值测试

运行测试：
```c
run_system_tests();  // 包含音频HAL测试
```

## 参考代码

修改基于`M5Tab5-UserDemo-main/platforms/tab5/components/m5stack_tab5/m5stack_tab5.c`中的PI4IOE5V配置代码。

## 注意事项

1. 确保I2C总线已正确初始化
2. 扬声器控制需要先初始化PI4IOE5V设备
3. 音量控制通过ES8388编解码器实现
4. 音频播放功能支持PCM和MP3格式 