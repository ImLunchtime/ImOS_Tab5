# M5Tab5 Application Manager System

This is a complete application manager system designed for the M5Tab5 development board, providing modern multi-application management and user interface.

## 🎯 System Features

### Application Types
- **App**: Full-screen applications, destroyed when exited, only one can run at a time
- **Overlay**: Overlay applications, can run in background, displayed above Apps

### Core Features
- ✅ Application registration and lifecycle management
- ✅ Layered display system (App layer + Overlay layer)
- ✅ Gesture interaction (left edge swipe)
- ✅ Modern UI design
- ✅ Animation effects support

## 📱 User Interface

### 1. Navigation Overlay
- **Position**: Bottom center of screen
- **Style**: Floating capsule container
- **Functions**: 
  - Back button (←) - Reserved functionality
  - Home button (●) - Return to launcher

### 2. App Drawer Overlay
- **Position**: Slides out from left side
- **Size**: 1/4 of screen width
- **Function**: Display list of all registered applications
- **Interactions**: 
  - Tap app item to launch application
  - Tap background to close drawer
  - Left edge swipe gesture to open

### 3. Launcher App
- **Function**: System default home screen
- **Features**: Automatically opens app drawer on startup
- **Style**: Clean solid color background

### 4. Settings App
- **Function**: System settings interface
- **Content**: 
  - Mechanical settings (velocity, acceleration, weight limit)
  - Sound settings
  - Display settings (brightness)
  - About information

## 🔧 技术架构

### 文件结构
```
main/
├── app_manager.h/c          # 应用管理器核心
├── overlay_navigation.h/c   # 导航按钮Overlay
├── overlay_drawer.h/c       # 应用抽屉Overlay
├── app_launcher.h/c         # 启动器应用
├── app_settings.h/c         # 设置应用
├── gesture_handler.h/c      # 手势处理
├── menu_utils.h/c           # 菜单工具函数
├── system_test.h/c          # 系统测试
└── gui.c                    # 主界面初始化
```

### 核心数据结构
```c
// 应用结构体
typedef struct app_t {
    char name[32];              // 应用名称
    char icon[8];               // 应用图标
    app_type_t type;            // 应用类型
    app_state_t state;          // 应用状态
    lv_obj_t* container;        // 应用容器
    // 回调函数...
} app_t;

// Overlay结构体
typedef struct overlay_t {
    app_t base;                 // 继承自app_t
    int z_index;                // 显示层级
    bool auto_start;            // 自动启动
} overlay_t;
```

## 🚀 使用方法

### 1. 注册应用
```c
// 注册普通应用
app_manager_register_app("MyApp", LV_SYMBOL_SETTINGS, 
                         my_app_create, my_app_destroy);

// 注册Overlay
app_manager_register_overlay("MyOverlay", LV_SYMBOL_LIST,
                             my_overlay_create, my_overlay_destroy,
                             z_index, auto_start);
```

### 2. 应用控制
```c
// 启动应用
app_manager_launch_app("MyApp");

// 显示/隐藏Overlay
app_manager_show_overlay("MyOverlay");
app_manager_hide_overlay("MyOverlay");

// 返回启动器
app_manager_go_to_launcher();
```

### 3. 手势控制
```c
// 手势处理自动初始化
gesture_handler_init();

// 控制手势启用/禁用
gesture_handler_set_enabled(true/false);
```

## 🎮 用户操作

### 基本操作
1. **启动**: 系统启动后自动显示启动器并打开应用抽屉
2. **切换应用**: 点击抽屉中的应用项
3. **返回主屏幕**: 点击底部导航的主屏幕按钮
4. **手势操作**: 从左侧边缘向右滑动打开应用抽屉

### 手势说明
- **左侧边缘滑动**: 在屏幕左侧20像素范围内开始滑动
- **触发阈值**: 向右滑动超过50像素即可打开抽屉
- **动画效果**: 抽屉打开/关闭带有流畅的滑动动画

## 🔍 调试和测试

### 启用系统测试
在编译时定义 `DEBUG_SYSTEM_TESTS` 宏来启用系统测试输出：
```c
#define DEBUG_SYSTEM_TESTS
```

### 测试功能
- 应用管理器初始化检查
- 已注册应用列表显示
- 已注册Overlay列表显示
- 当前应用状态检查

## 📝 扩展开发

### 添加新应用
1. 创建应用的.h/.c文件
2. 实现create和destroy回调函数
3. 在gui.c中注册应用
4. 更新CMakeLists.txt

### 添加新Overlay
1. 创建Overlay的.h/.c文件
2. 实现create和destroy回调函数
3. 设置合适的z_index
4. 在gui.c中注册Overlay

### 自定义手势
1. 修改gesture_handler.c中的参数
2. 添加新的手势识别逻辑
3. 绑定到相应的回调函数

## 🛠️ 编译和部署

### 编译
```bash
idf.py build
```

### 烧录
```bash
idf.py flash monitor
```

### 调试
使用ESP-IDF的监控工具查看系统测试输出和调试信息。

## 📋 已知问题和限制

1. **字体支持**: 目前使用LVGL默认字体，中文显示可能需要额外配置
2. **内存管理**: 应用销毁时需要确保所有资源正确释放
3. **触摸响应**: 手势检测可能与某些LVGL控件的触摸事件冲突

## 🔮 未来计划

- [ ] 应用历史栈和返回功能
- [ ] 应用图标自定义
- [ ] 更多手势支持
- [ ] 应用间通信机制
- [ ] 系统主题和样式配置
- [ ] 性能优化和内存管理改进

---

这个应用管理器系统为M5Tab5开发板提供了一个完整的、可扩展的多应用管理框架，支持现代化的用户交互和直观的操作体验。 